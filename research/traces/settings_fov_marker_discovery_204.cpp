#include "stdafx.h"

#include <spdlog/sinks/basic_file_sink.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>

namespace
{
    constexpr std::size_t kMaxCandidates = 256;
    constexpr std::size_t kMaxScanBytesPerPass = 64ull * 1024ull * 1024ull;
    constexpr DWORD kIntervalMs = 500;
    constexpr float kEpsilon = 0.01f;

    struct Candidate
    {
        std::uintptr_t address{};
        float last{};
        float previous{};
        std::uint8_t stableBaseline{};
        bool active{};
        std::uint8_t nextMarker{};
        std::uint8_t sequenceHits{};
    };

    HMODULE g_module{};
    std::shared_ptr<spdlog::logger> g_logger;
    std::atomic<bool> g_running{true};
    std::array<Candidate, kMaxCandidates> g_candidates{};
    std::size_t g_count{};
    std::uint64_t g_sequence{};
    std::uintptr_t g_scanCursor{};

    bool ReadFloat(std::uintptr_t address, float& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address >= end || sizeof(value) > end - address) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
        return std::isfinite(value);
    }

    bool Writable(const MEMORY_BASIC_INFORMATION& info)
    {
        const auto p = info.Protect & 0xff;
        return info.State == MEM_COMMIT && !(info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
            (p == PAGE_READWRITE || p == PAGE_WRITECOPY || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY);
    }

    bool Near(float value, float expected)
    {
        return std::isfinite(value) && std::fabs(value - expected) <= kEpsilon;
    }

    Candidate* Find(std::uintptr_t address)
    {
        for (std::size_t i = 0; i < g_count; ++i)
            if (g_candidates[i].address == address) return &g_candidates[i];
        return nullptr;
    }

    void LogTransition(Candidate& candidate, float value, DWORD thread)
    {
        const auto old = candidate.last;
        candidate.previous = old;
        candidate.last = value;
        const auto now = GetTickCount64();
        g_logger->info(
            "TRACE seq={} stage=marker-transition thread={} address=0x{:X} old={} new={} nextMarker={} sequenceHits={} timeMs={}",
            ++g_sequence, thread, candidate.address, old, value, candidate.nextMarker, candidate.sequenceHits, now);
    }

    void Observe(Candidate& candidate, float value, DWORD thread)
    {
        if (!candidate.active) {
            candidate.last = value;
            if (Near(value, 90.0f)) {
                if (candidate.stableBaseline < 3) ++candidate.stableBaseline;
                if (candidate.stableBaseline == 3) {
                    candidate.active = true;
                    candidate.nextMarker = 77;
                    candidate.sequenceHits = 0;
                    g_logger->info("TRACE seq={} stage=marker-baseline thread={} address=0x{:X} value={} timeMs={}",
                        ++g_sequence, thread, candidate.address, value, GetTickCount64());
                }
            } else candidate.stableBaseline = 0;
            return;
        }
        if (Near(value, static_cast<float>(candidate.nextMarker))) {
            LogTransition(candidate, value, thread);
            if (candidate.nextMarker == 77) candidate.nextMarker = 79;
            else if (candidate.nextMarker == 79) candidate.nextMarker = 90;
            else {
                ++candidate.sequenceHits;
                candidate.nextMarker = 77;
                g_logger->info("TRACE seq={} stage=marker-cycle-complete thread={} address=0x{:X} cycles={} timeMs={}",
                    ++g_sequence, thread, candidate.address, candidate.sequenceHits, GetTickCount64());
            }
        }
        candidate.last = value;
    }

    void ScanPass()
    {
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        const auto minimum = reinterpret_cast<std::uintptr_t>(system.lpMinimumApplicationAddress);
        const auto maximum = reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress);
        if (g_scanCursor < minimum || g_scanCursor >= maximum) g_scanCursor = minimum;
        std::size_t scanned{};
        auto* address = reinterpret_cast<std::uint8_t*>(g_scanCursor);
        while (scanned < kMaxScanBytesPerPass) {
            MEMORY_BASIC_INFORMATION info{};
            if (!VirtualQuery(address, &info, sizeof(info))) break;
            const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
            const auto size = info.RegionSize;
            const auto next = base + size;
            address = reinterpret_cast<std::uint8_t*>(next);
            if (!Writable(info)) continue;
            const auto scanSize = std::min<std::size_t>(size, kMaxScanBytesPerPass - scanned);
            for (std::size_t offset = 0; offset + sizeof(float) <= scanSize; offset += sizeof(float)) {
                float value{};
                if (!ReadFloat(base + offset, value)) continue;
                auto* candidate = Find(base + offset);
                if (!candidate && g_count < kMaxCandidates &&
                    (Near(value, 90.0f) || Near(value, 90.65574f))) {
                    candidate = &g_candidates[g_count++];
                    candidate->address = base + offset;
                    candidate->last = value;
                }
                if (candidate) Observe(*candidate, value, GetCurrentThreadId());
            }
            scanned += scanSize;
            if (next <= base || next >= maximum) {
                g_scanCursor = minimum;
                return;
            }
        }
        g_scanCursor = reinterpret_cast<std::uintptr_t>(address);
    }

    DWORD WINAPI Monitor(void*)
    {
        while (g_running.load(std::memory_order_relaxed)) {
            ScanPass();
            Sleep(kIntervalMs);
        }
        return 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto path = std::filesystem::path(modulePath).remove_filename() / "STALKER2SettingsFovMarkerDiscovery204.log";
        std::ofstream(path, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2SettingsFovMarkerDiscovery204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
            g_logger->info("TRACE installed: 2.0.4 Settings FOV marker discovery; read-only, bounded polling.");
            const auto thread = CreateThread(nullptr, 0, Monitor, nullptr, 0, nullptr);
            if (thread) CloseHandle(thread);
        } catch (...) { return 0; }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH) g_running.store(false, std::memory_order_relaxed);
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = module;
    DisableThreadLibraryCalls(module);
    if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
    return TRUE;
}
