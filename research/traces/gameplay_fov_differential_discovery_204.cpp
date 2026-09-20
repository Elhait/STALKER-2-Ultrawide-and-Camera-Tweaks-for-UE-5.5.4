#include "stdafx.h"
#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace
{
    constexpr char kWriterSignature[] =
        "F6 86 62 02 00 00 10 F3 0F 10 86 30 02 00 00 "
        "0F 85 ?? ?? ?? ?? 48 8D 4B 30 F3 0F 11 43 30";
    constexpr std::size_t kFovStoreOffset = 25;
    constexpr std::size_t kMaxRecords = 256;
    constexpr float kChangeEpsilon = 0.0001f;

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_hook;
    std::atomic<std::uint64_t> g_sequence{};
    float g_lastFov = std::numeric_limits<float>::quiet_NaN();

    bool IsExecutable(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) || info.State != MEM_COMMIT) return false;
        const auto p = info.Protect & 0xff;
        return p == PAGE_EXECUTE || p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
    }

    template <typename T>
    bool Read(std::uintptr_t address, T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address >= end || sizeof(T) > end - address) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
        return true;
    }

    std::uintptr_t Rva(std::uintptr_t address)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        return address >= base && address - base < 0x20000000 ? address - base : 0;
    }

    std::uintptr_t ReturnAddress(const SafetyHookContext& context)
    {
        std::uintptr_t value{};
        Read(static_cast<std::uintptr_t>(context.rsp), value);
        return value;
    }

    void TraceWriter(SafetyHookContext& context)
    {
        const auto sequence = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
        if (!g_logger || sequence > kMaxRecords) return;
        const float fov = context.xmm0.f32[0];
        if (!std::isfinite(fov) || (std::isfinite(g_lastFov) && std::fabs(fov - g_lastFov) <= kChangeEpsilon)) return;
        const float previousFov = g_lastFov;
        g_lastFov = fov;
        const auto source = static_cast<std::uintptr_t>(context.rsi);
        const auto output = static_cast<std::uintptr_t>(context.rbx);
        float aspect{};
        float secondaryFov{};
        std::uint8_t flags{};
        float outputFov{};
        float outputAspect{};
        Read(source + 0x254, aspect);
        Read(source + 0x234, secondaryFov);
        Read(source + 0x259, flags);
        Read(output + 0x30, outputFov);
        Read(output + 0x5C, outputAspect);
        g_logger->info(
            "TRACE seq={} stage=fov-writer-change thread={} fov={} previousFov={} source=0x{:X} output=0x{:X} "
            "sourceAspect={} sourceFlags=0x{:02X} secondaryFov={} outputFovBefore={} outputAspectBefore={} "
            "callerRva=0x{:X} callerExec={} xmm0={} xmm1={} xmm2={} xmm3={}",
            sequence, GetCurrentThreadId(), fov, previousFov, source, output, aspect, flags, secondaryFov,
            outputFov, outputAspect, Rva(ReturnAddress(context)), IsExecutable(ReturnAddress(context)),
            context.xmm0.f32[0], context.xmm1.f32[0], context.xmm2.f32[0], context.xmm3.f32[0]);
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto path = std::filesystem::path(modulePath).remove_filename() / "STALKER2GameplayFovDifferentialDiscovery204.log";
        std::ofstream(path, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2GameplayFovDifferentialDiscovery204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
            const auto matches = Memory::PatternScanAll(g_executable, kWriterSignature);
            if (matches.size() != 1) throw std::runtime_error("unique gameplay writer signature not found");
            auto* target = matches.front() + kFovStoreOffset;
            if (!IsExecutable(reinterpret_cast<std::uintptr_t>(target)) ||
                target[0] != 0xF3 || target[1] != 0x0F || target[2] != 0x11 || target[3] != 0x43 || target[4] != 0x30)
                throw std::runtime_error("validated FOV store mismatch");
            g_hook = safetyhook::create_mid(target, TraceWriter);
            if (!g_hook) throw std::runtime_error("FOV writer hook creation failed");
            g_logger->info("TRACE installed: 2.0.4 gameplay FOV differential discovery; read-only, settings sequence 90->77->79->90.");
        } catch (const std::exception& error) {
            g_hook.reset();
            if (g_logger) g_logger->error("TRACE setup refused safely: {}", error.what());
        } catch (...) {
            g_hook.reset();
            if (g_logger) g_logger->error("TRACE setup refused safely: unknown error.");
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
