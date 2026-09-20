#include "helper.hpp"

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
    constexpr std::uintptr_t kStoreRva = 0x6B7CB05;
    constexpr std::uintptr_t kAspectOffset = 0x254;
    constexpr std::uintptr_t kFlagsOffset = 0x259;
    constexpr float kNativeAspect = 3.5555556f;
    constexpr std::uint8_t kExpectedFlags = 0x05;
    constexpr std::uint8_t kStoreBytes[] = {
        0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F,
    };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_storeHook;
    std::atomic<std::uint64_t> g_sequence{};

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
            info.State != MEM_COMMIT) return false;
        const auto protection = info.Protect & 0xFF;
        return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
            protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    }

    std::uintptr_t ToRva(std::uintptr_t address)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        return address >= base && address - base < 0x20000000 ? address - base : 0;
    }

    void TraceStore(SafetyHookContext& context)
    {
        const auto sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        const auto inner = static_cast<std::uintptr_t>(context.rax);
        float beforeAspect{};
        std::uint8_t flags{};
        const bool aspectReadable = SafeRead(inner + kAspectOffset, beforeAspect);
        const bool flagsReadable = SafeRead(inner + kFlagsOffset, flags);
        const bool expectedState = aspectReadable && flagsReadable &&
            std::fabs(beforeAspect - kNativeAspect) <= 0.001f && flags == kExpectedFlags;
        const auto rip = static_cast<std::uintptr_t>(context.rip);

        if (g_logger) {
            g_logger->info(
                "TRACE seq={} stage=enter-aspect-store rip=0x{:X} storeRva=0x{:X} ripRva=0x{:X} "
                "raxInner=0x{:X} sameObject={} beforeAspect={} flags=0x{:02X} reads={} "
                "thread={} expectedState={}",
                sequence, rip, kStoreRva, ToRva(rip), inner, expectedState,
                aspectReadable ? beforeAspect : 0.0f, flags,
                aspectReadable && flagsReadable, GetCurrentThreadId(), expectedState);
        }
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicEnterAspectStoreProvenance204.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicEnterAspectStoreProvenance204", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        auto* store = reinterpret_cast<std::uint8_t*>(reinterpret_cast<std::uintptr_t>(g_executable) + kStoreRva);
        if (!IsExecutable(reinterpret_cast<std::uintptr_t>(store)) ||
            std::memcmp(store, kStoreBytes, sizeof(kStoreBytes)) != 0) {
            g_logger->error("TRACE setup refused: 2.0.4 ENTER aspect-store bytes mismatch.");
            return 0;
        }
        try {
            g_storeHook = safetyhook::create_mid(store, TraceStore);
            if (!g_storeHook) throw std::runtime_error("aspect-store hook creation failed");
            g_logger->info("TRACE installed: 2.0.4 ENTER aspect store provenance; one ordinary read-only hook, no writes.");
        } catch (...) {
            g_storeHook.reset();
            g_logger->error("TRACE setup refused safely; rollback completed.");
        }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = module;
    DisableThreadLibraryCalls(module);
    if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
    return TRUE;
}
