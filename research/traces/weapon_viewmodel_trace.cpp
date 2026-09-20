#include "stdafx.h"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace
{
    constexpr std::uintptr_t kFirstPersonFovAccessorRva = 0x55943C2;
    constexpr std::uintptr_t kEnableFirstPersonAccessorRva = 0x559427E;
    constexpr std::uintptr_t kInFirstPersonAccessorRva = 0x5594320;
    constexpr std::uintptr_t kVirtualFovSlot = 0x610;
    constexpr std::uint64_t kMaxLogRecords = 20000;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_fovHook;
    SafetyHookMid g_enableHook;
    SafetyHookMid g_inHook;
    std::atomic<std::uint64_t> g_sequence{0};

    template <typename T>
    bool SafeRead(std::uintptr_t address, T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address > end || sizeof(value) > end - address) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
        return true;
    }

    bool IsExecutable(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT) return false;
        const auto protect = info.Protect & 0xff;
        return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ ||
            protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
    }

    std::uintptr_t CallerFromContext(SafetyHookContext& context)
    {
        std::uintptr_t caller{};
        SafeRead(static_cast<std::uintptr_t>(context.rsp), caller);
        return caller;
    }

    void LogAccessor(const char* name, SafetyHookContext& context)
    {
        const auto sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        if (sequence > kMaxLogRecords) return;

        const auto object = static_cast<std::uintptr_t>(context.rcx);
        const auto argument = static_cast<std::uintptr_t>(context.rdx);
        const auto caller = CallerFromContext(context);
        std::uint8_t mode{};
        std::uintptr_t vtable{};
        std::uintptr_t virtualTarget{};
        const bool modeOk = SafeRead(object + 0x262, mode);
        const bool vtableOk = SafeRead(object, vtable);
        const bool targetOk = vtableOk && SafeRead(vtable + kVirtualFovSlot, virtualTarget) &&
            IsExecutable(virtualTarget);

        if (!g_logger) return;
        g_logger->info(
            "TRACE seq={} accessor={} object=0x{:X} argument=0x{:X} caller=0x{:X} mode262={} vtable=0x{:X} slot610=0x{:X} targetValid={}",
            sequence, name, object, argument, caller, modeOk ? mode : 0, vtableOk ? vtable : 0,
            targetOk ? virtualTarget : 0, targetOk);
    }

    void TraceFirstPersonFov(SafetyHookContext& context)
    {
        LogAccessor("SetFirstPersonFieldOfView", context);
    }

    void TraceEnableFirstPerson(SafetyHookContext& context)
    {
        LogAccessor("SetEnableFirstPersonFieldOfView", context);
    }

    void TraceInFirstPerson(SafetyHookContext& context)
    {
        LogAccessor("InFirstPersonFieldOfView", context);
    }

    bool ResolveTargets(std::uint8_t*& fov, std::uint8_t*& enable, std::uint8_t*& inFirstPerson)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        const auto targets = std::array<std::uintptr_t, 3>{
            base + kFirstPersonFovAccessorRva,
            base + kEnableFirstPersonAccessorRva,
            base + kInFirstPersonAccessorRva,
        };
        for (const auto target : targets) {
            if (!IsExecutable(target)) return false;
        }
        fov = reinterpret_cast<std::uint8_t*>(targets[0]);
        enable = reinterpret_cast<std::uint8_t*>(targets[1]);
        inFirstPerson = reinterpret_cast<std::uint8_t*>(targets[2]);
        return true;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2WeaponViewmodelTrace.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2WeaponViewmodelTrace", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        std::uint8_t* fov = nullptr;
        std::uint8_t* enable = nullptr;
        std::uint8_t* inFirstPerson = nullptr;
        if (!ResolveTargets(fov, enable, inFirstPerson)) {
            g_logger->error("TRACE setup refused: one or more Steam 2.0.2 accessor RVAs are not executable.");
            return 0;
        }

        try {
            g_fovHook = safetyhook::create_mid(fov, TraceFirstPersonFov);
            if (!g_fovHook) throw std::runtime_error("FOV accessor hook was not created");
            g_enableHook = safetyhook::create_mid(enable, TraceEnableFirstPerson);
            if (!g_enableHook) throw std::runtime_error("enable accessor hook was not created");
            g_inHook = safetyhook::create_mid(inFirstPerson, TraceInFirstPerson);
            if (!g_inHook) throw std::runtime_error("in-first-person hook was not created");
            g_logger->info("TRACE hooks installed: first-person FOV accessor and mode accessors.");
        } catch (...) {
            g_inHook.reset();
            g_enableHook.reset();
            g_fovHook.reset();
            g_logger->error("TRACE setup refused safely; partial hooks rolled back.");
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
