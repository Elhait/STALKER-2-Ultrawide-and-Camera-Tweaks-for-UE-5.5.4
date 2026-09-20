#include "stdafx.h"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace
{
    // Steam 2.0.2: FUN_14695C348, reached by IA_OffsetAiming.
    constexpr std::uintptr_t kAimRefreshRva = 0x695C348;
    constexpr std::uintptr_t kAimStateOffset = 0x248;
    constexpr std::uintptr_t kRefreshVirtualSlot = 0xA98;
    constexpr std::uint64_t kMaxLogRecords = 20000;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_hook;
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

    void TraceAimRefresh(SafetyHookContext& context)
    {
        const auto sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        if (sequence > kMaxLogRecords || !g_logger) return;

        const auto object = static_cast<std::uintptr_t>(context.rcx);
        const auto requested = static_cast<std::uint8_t>(context.rdx & 0xff);
        const auto caller = CallerFromContext(context);
        std::uint8_t current{};
        std::uintptr_t vtable{};
        std::uintptr_t refreshTarget{};
        const bool stateOk = SafeRead(object + kAimStateOffset, current);
        const bool vtableOk = SafeRead(object, vtable);
        const bool targetOk = vtableOk && SafeRead(vtable + kRefreshVirtualSlot, refreshTarget) &&
            IsExecutable(refreshTarget);

        g_logger->info(
            "TRACE seq={} object=0x{:X} requested248={} current248={} caller=0x{:X} vtable=0x{:X} slotA98=0x{:X} targetValid={} stateRead={} sameState={}",
            sequence, object, requested, stateOk ? current : 0, caller, vtableOk ? vtable : 0,
            targetOk ? refreshTarget : 0, targetOk, stateOk, stateOk && current == requested);
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2AdsViewmodelRefreshTrace.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2AdsViewmodelRefreshTrace", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        const auto target = reinterpret_cast<std::uint8_t*>(
            reinterpret_cast<std::uintptr_t>(g_executable) + kAimRefreshRva);
        if (!IsExecutable(reinterpret_cast<std::uintptr_t>(target))) {
            g_logger->error("TRACE setup refused: Steam 2.0.2 ADS refresh RVA is not executable.");
            return 0;
        }

        try {
            g_hook = safetyhook::create_mid(target, TraceAimRefresh);
            if (!g_hook) throw std::runtime_error("ADS refresh hook was not created");
            g_logger->info("TRACE hook installed: FUN_14695C348 / IA_OffsetAiming refresh path.");
        } catch (...) {
            g_hook.reset();
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
