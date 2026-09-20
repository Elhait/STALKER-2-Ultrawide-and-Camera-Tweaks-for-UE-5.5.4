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
    constexpr std::uintptr_t kCallbackRva = 0x6B6C482;
    constexpr std::uint8_t kCallbackEntryBytes[] = {
        0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE,
    };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
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

    std::uintptr_t ReadExecutableStackRva(std::uintptr_t stackAddress, int index)
    {
        std::uintptr_t candidate = 0;
        if (!SafeRead(stackAddress + static_cast<std::uintptr_t>(index) * sizeof(std::uintptr_t), candidate) ||
            !IsExecutable(candidate)) return 0;
        return ToRva(candidate);
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

    void ReadTargetState(std::uintptr_t context, std::uintptr_t& target,
        float& aspect, std::uint8_t& flags, float& fov)
    {
        target = 0;
        aspect = 0.0f;
        flags = 0;
        fov = 0.0f;
        if (!SafeRead(context + 0xF8, target) || !target) return;
        SafeRead(target + 0x254, aspect);
        SafeRead(target + 0x259, flags);
        SafeRead(target + 0x30, fov);
    }

    void TraceCallback(SafetyHookContext& context)
    {
        const auto sequence = ++g_sequence;
        const auto returnRva = ReadExecutableStackRva(context.rsp, 0);
        const auto stack1Rva = ReadExecutableStackRva(context.rsp, 1);

        const auto callbackContext = static_cast<std::uintptr_t>(context.rcx);
        std::uintptr_t target = 0;
        float aspect = 0.0f;
        float fov = 0.0f;
        std::uint8_t flags = 0;
        ReadTargetState(callbackContext, target, aspect, flags, fov);

        LONG clientWidth = 0;
        LONG clientHeight = 0;
        ReadClientSize(clientWidth, clientHeight);

        if (g_logger) {
            g_logger->info(
                "TRACE seq={} stage=callback-entry callbackRva=0x{:X} thread={} context=0x{:X} target=0x{:X} aspect={} flags=0x{:02X} fov={} returnRva=0x{:X} stack1Rva=0x{:X} client={}x{}",
                sequence, kCallbackRva, GetCurrentThreadId(), callbackContext, target,
                aspect, flags, fov, returnRva, stack1Rva, clientWidth, clientHeight);
        }
    }

    bool ResolveCallback(std::uint8_t*& callback)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        callback = reinterpret_cast<std::uint8_t*>(base + kCallbackRva);
        if (!IsExecutable(reinterpret_cast<std::uintptr_t>(callback))) return false;
        return std::memcmp(callback, kCallbackEntryBytes, sizeof(kCallbackEntryBytes)) == 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicExitCallbackTrace.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();

        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicExitCallbackTrace", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) {
            return 0;
        }

        std::uint8_t* callback = nullptr;
        if (!ResolveCallback(callback)) {
            g_logger->error("TRACE setup refused: 2.0.4 FUN_146B6C482 prologue validation failed.");
            return 0;
        }

        try {
            g_callbackHook = safetyhook::create_mid(callback, TraceCallback);
            if (!g_callbackHook) throw std::runtime_error("callback entry hook creation failed");
            g_logger->info("TRACE installed: 2.0.4 FUN_146B6C482 callback entry; read-only context/caller capture.");
        } catch (...) {
            g_callbackHook.reset();
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
