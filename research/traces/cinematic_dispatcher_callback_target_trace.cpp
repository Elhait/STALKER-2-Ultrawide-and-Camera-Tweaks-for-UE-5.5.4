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
    // Version-specific evidence: Steam 2.0.4 runtime EXE hash
    // 2ECC5D19FE37F97E3F7F2467D652B299B5A47F010FA49FD803A49A4A6930A409.
    constexpr std::uintptr_t kDispatcherRva = 0x43F130;
    // Hook after the resolver's pointer-load and result-copy instructions;
    // 0x43F19E and 0x43F1A1 are valid instructions but are not used as
    // mid-hook boundaries. The resolver result remains in RBX here.
    constexpr std::uintptr_t kResolvePostRva = 0x43F1A4;
    constexpr std::uintptr_t kCallbackCallRva = 0x43F1FC;
    constexpr std::uint8_t kDispatcherEntryBytes[] = {
        0x56, 0x57, 0x55, 0x53, 0x48, 0x83, 0xEC, 0x48, 0x0F, 0x29,
    };
    constexpr std::uint8_t kResolvePostBytes[] = {0x85, 0xC0};
    constexpr std::uint8_t kCallbackCallBytes[] = {0xFF, 0x56, 0x20};

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_entryHook;
    SafetyHookMid g_resolvePostHook;
    std::uint64_t g_sequence{};

    struct Invocation
    {
        std::uint64_t sequence = 0;
        std::uintptr_t item = 0;
        std::uintptr_t itemContext = 0;
        std::uintptr_t resolveResult = 0;
        std::uintptr_t callerRva = 0;
    };

    thread_local Invocation g_invocation;

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

    std::uintptr_t ReadReturnRva(std::uintptr_t stack)
    {
        std::uintptr_t address = 0;
        return SafeRead(stack, address) && IsExecutable(address) ? ToRva(address) : 0;
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

    void TraceEntry(SafetyHookContext& context)
    {
        g_invocation = {};
        g_invocation.sequence = ++g_sequence;
        g_invocation.item = static_cast<std::uintptr_t>(context.rcx);
        SafeRead(g_invocation.item + 0x18, g_invocation.itemContext);
        g_invocation.callerRva = ReadReturnRva(context.rsp);

        LONG width = 0;
        LONG height = 0;
        ReadClientSize(width, height);
        if (g_logger) {
            g_logger->info(
                "TRACE seq={} stage=dispatcher-entry thread={} item=0x{:X} itemPlus18=0x{:X} callerRva=0x{:X} client={}x{}",
                g_invocation.sequence, GetCurrentThreadId(), g_invocation.item,
                g_invocation.itemContext, g_invocation.callerRva, width, height);
        }
    }

    void TraceResolvePost(SafetyHookContext& context)
    {
        if (!g_invocation.sequence) return;
        g_invocation.resolveResult = static_cast<std::uintptr_t>(context.rbx);
        if (g_logger) {
            g_logger->info(
                "TRACE seq={} stage=resolve-post thread={} item=0x{:X} itemPlus18=0x{:X} resolveResult=0x{:X}",
                g_invocation.sequence, GetCurrentThreadId(), g_invocation.item,
                g_invocation.itemContext, g_invocation.resolveResult);
        }
    }

    void TraceCallbackTarget(SafetyHookContext& context)
    {
        if (!g_invocation.sequence) return;
        const auto resolvedRecord = static_cast<std::uintptr_t>(context.rsi);
        std::uintptr_t callbackTarget = 0;
        const bool callbackReadable = SafeRead(resolvedRecord + 0x20, callbackTarget);
        const auto callbackRva = callbackReadable ? ToRva(callbackTarget) : 0;

        LONG width = 0;
        LONG height = 0;
        ReadClientSize(width, height);
        if (g_logger) {
            g_logger->info(
                "TRACE seq={} stage=callback-target thread={} item=0x{:X} itemPlus18=0x{:X} resolveResult=0x{:X} resolvedContext=0x{:X} callbackTarget=0x{:X} callbackRva=0x{:X} client={}x{}",
                g_invocation.sequence, GetCurrentThreadId(), g_invocation.item,
                g_invocation.itemContext, g_invocation.resolveResult, resolvedRecord,
                callbackTarget, callbackRva, width, height);
        }
        g_invocation = {};
    }

    bool ResolveTargets(std::uint8_t*& dispatcher, std::uint8_t*& resolvePost)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        dispatcher = reinterpret_cast<std::uint8_t*>(base + kDispatcherRva);
        resolvePost = reinterpret_cast<std::uint8_t*>(base + kResolvePostRva);
        if (!IsExecutable(reinterpret_cast<std::uintptr_t>(dispatcher)) ||
            !IsExecutable(reinterpret_cast<std::uintptr_t>(resolvePost))) return false;
        return std::memcmp(dispatcher, kDispatcherEntryBytes, sizeof(kDispatcherEntryBytes)) == 0 &&
            std::memcmp(resolvePost, kResolvePostBytes, sizeof(kResolvePostBytes)) == 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicDispatcherCallbackTargetTrace.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicDispatcherCallbackTargetTrace", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        std::uint8_t* dispatcher = nullptr;
        std::uint8_t* resolvePost = nullptr;
        if (!ResolveTargets(dispatcher, resolvePost)) {
            g_logger->error("TRACE setup refused: 2.0.4 dispatcher/resolver-post validation failed.");
            return 0;
        }
        try {
            g_entryHook = safetyhook::create_mid(dispatcher, TraceEntry);
            g_resolvePostHook = safetyhook::create_mid(resolvePost, TraceResolvePost);
            if (!g_entryHook || !g_resolvePostHook) throw std::runtime_error("dispatcher entry/resolve-post trace hook creation failed");
            g_logger->info("TRACE installed: 2.0.4 dispatcher entry + resolve-post capture; callback boundary gated pending ABI validation; read-only, no record cap.");
        } catch (...) {
            g_resolvePostHook.reset();
            g_entryHook.reset();
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
