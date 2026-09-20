#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace
{
    constexpr std::uintptr_t kDispatcherRva = 0x67D4A46;
    constexpr std::uintptr_t kPostCallRva = 0x67D4A97;
    constexpr std::uintptr_t kCallbackRva = 0x6B6C482;
    constexpr std::uint8_t kDispatcherEntryBytes[] = {
        0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x89, 0xCE,
    };
    constexpr std::uint8_t kPostCallBytes[] = { 0x48, 0x85, 0xFF };
    constexpr std::uint8_t kCallbackEntryBytes[] = {
        0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE,
    };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_dispatcherHook;
    SafetyHookMid g_postCallHook;
    SafetyHookMid g_callbackHook;
    std::uint64_t g_sequence{};

    template <typename T>
    bool SafeRead(std::uintptr_t address, T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address >= end || sizeof(value) > end - address) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
        return true;
    }

    bool IsExecutable(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        return (info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    }

    std::uintptr_t ToRva(std::uintptr_t address)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        return address >= base && address - base < 0x20000000 ? address - base : 0;
    }

    std::uintptr_t StackRva(std::uintptr_t rsp, int index)
    {
        std::uintptr_t value{};
        return SafeRead(rsp + static_cast<std::uintptr_t>(index) * sizeof(value), value) && IsExecutable(value)
            ? ToRva(value) : 0;
    }

    void ReadClientSize(LONG& width, LONG& height)
    {
        HWND window = nullptr;
        EnumWindows([](HWND candidate, LPARAM parameter) {
            DWORD processId = 0;
            GetWindowThreadProcessId(candidate, &processId);
            if (processId == GetCurrentProcessId() && IsWindowVisible(candidate) && GetWindow(candidate, GW_OWNER) == nullptr) {
                *reinterpret_cast<HWND*>(parameter) = candidate;
                return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&window));
        RECT rect{};
        if (window && GetClientRect(window, &rect)) {
            width = rect.right - rect.left;
            height = rect.bottom - rect.top;
        }
    }

    struct ThreadState
    {
        std::uint64_t sequence{};
        std::uintptr_t item{};
        std::uintptr_t vtable{};
        std::uintptr_t callerRva{};
        bool active{};
    };
    thread_local ThreadState g_threadState;

    void TraceDispatcherEntry(SafetyHookContext& context)
    {
        g_threadState.sequence = ++g_sequence;
        g_threadState.item = static_cast<std::uintptr_t>(context.rcx);
        SafeRead(g_threadState.item, g_threadState.vtable);
        g_threadState.callerRva = StackRva(context.rsp, 0);
        g_threadState.active = true;

        LONG width = 0, height = 0;
        ReadClientSize(width, height);
        std::uintptr_t sharedContext{};
        SafeRead(g_threadState.item + 0x18, sharedContext);
        if (g_logger) {
            g_logger->info("TRACE seq={} stage=dispatcher-entry thread={} item=0x{:X} vtable=0x{:X} itemPlus18=0x{:X} callerRva=0x{:X} client={}x{}",
                g_threadState.sequence, GetCurrentThreadId(), g_threadState.item, g_threadState.vtable,
                sharedContext, g_threadState.callerRva, width, height);
        }
    }

    void TracePostCall(SafetyHookContext& context)
    {
        std::uintptr_t record{};
        std::uintptr_t callbackTarget{};
        SafeRead(static_cast<std::uintptr_t>(context.rsi), record);
        SafeRead(record + 0x20, callbackTarget);
        if (g_logger) {
            g_logger->info("TRACE seq={} stage=dispatcher-post-call thread={} item=0x{:X} vtable=0x{:X} record=0x{:X} callbackTarget=0x{:X} callbackRva=0x{:X} callerRva=0x{:X}",
                g_threadState.sequence, GetCurrentThreadId(), g_threadState.item, g_threadState.vtable,
                record, callbackTarget, ToRva(callbackTarget), g_threadState.callerRva);
        }
        g_threadState.active = false;
    }

    void TraceConfirmedCallback(SafetyHookContext& context)
    {
        if (!g_threadState.active || !g_logger) return;
        LONG width = 0, height = 0;
        ReadClientSize(width, height);
        std::uint64_t itemPlus08{}, itemPlus10{}, itemPlus18{}, itemPlus20{}, itemPlus28{};
        std::uint32_t itemPlus14{};
        SafeRead(g_threadState.item + 0x08, itemPlus08);
        SafeRead(g_threadState.item + 0x10, itemPlus10);
        SafeRead(g_threadState.item + 0x14, itemPlus14);
        SafeRead(g_threadState.item + 0x18, itemPlus18);
        SafeRead(g_threadState.item + 0x20, itemPlus20);
        SafeRead(g_threadState.item + 0x28, itemPlus28);
        g_logger->info("TRACE seq={} stage=confirmed-callback thread={} dispatcherItem=0x{:X} dispatcherVtable=0x{:X} itemPlus08=0x{:X} itemPlus10=0x{:X} itemPlus14=0x{:X} itemPlus18=0x{:X} itemPlus20=0x{:X} itemPlus28=0x{:X} callbackContext=0x{:X} callbackRva=0x{:X} callerRva=0x{:X} client={}x{}",
            g_threadState.sequence, GetCurrentThreadId(), g_threadState.item, g_threadState.vtable,
            itemPlus08, itemPlus10, itemPlus14,
            itemPlus18, itemPlus20, itemPlus28,
            static_cast<std::uintptr_t>(context.rcx), kCallbackRva, g_threadState.callerRva, width, height);
    }

    bool Validate(std::uintptr_t rva, const std::uint8_t* bytes, std::size_t size)
    {
        const auto address = reinterpret_cast<std::uint8_t*>(reinterpret_cast<std::uintptr_t>(g_executable) + rva);
        return IsExecutable(reinterpret_cast<std::uintptr_t>(address)) && std::memcmp(address, bytes, size) == 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicDispatcherOwnerTrace.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicDispatcherOwnerTrace", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        if (!Validate(kDispatcherRva, kDispatcherEntryBytes, sizeof(kDispatcherEntryBytes)) ||
            !Validate(kPostCallRva, kPostCallBytes, sizeof(kPostCallBytes)) ||
            !Validate(kCallbackRva, kCallbackEntryBytes, sizeof(kCallbackEntryBytes))) {
            g_logger->error("TRACE setup refused: 2.0.4 dispatcher/callback validation failed.");
            return 0;
        }
        try {
            const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
            g_dispatcherHook = safetyhook::create_mid(reinterpret_cast<void*>(base + kDispatcherRva), TraceDispatcherEntry);
            g_postCallHook = safetyhook::create_mid(reinterpret_cast<void*>(base + kPostCallRva), TracePostCall);
            g_callbackHook = safetyhook::create_mid(reinterpret_cast<void*>(base + kCallbackRva), TraceConfirmedCallback);
            if (!g_dispatcherHook || !g_postCallHook || !g_callbackHook) throw std::runtime_error("hook creation failed");
            g_logger->info("TRACE installed: 2.0.4 dispatcher owner correlation; read-only, callback-filtered, no camera writes.");
        } catch (...) {
            g_callbackHook.reset();
            g_postCallHook.reset();
            g_dispatcherHook.reset();
            g_logger->error("TRACE setup refused safely; hook rollback completed.");
        }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = module;
    DisableThreadLibraryCalls(module);
    const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread);
    return TRUE;
}
