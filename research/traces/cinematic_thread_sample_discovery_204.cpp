#include "stdafx.h"
#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace
{
    constexpr std::uintptr_t kExitCallbackRva = 0x6B6C482;
    constexpr std::uint8_t kExitCallbackBytes[] = { 0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE };
    constexpr char kEnterSignature[] =
        "48 8B 41 18 48 8B 88 F8 00 00 00 80 89 59 02 00 00 01 "
        "48 8B 80 F8 00 00 00 C7 80 54 02 00 00 39 8E E3 3F B0 01 C3";
    constexpr DWORD kSampleIntervalMs = 10;
    constexpr std::size_t kMaxRips = 128;
    constexpr std::size_t kMaxSamples = 2048;

    struct RipEntry { std::uintptr_t rip{}; std::uint32_t hits{}; std::uint8_t lane{}; float value{}; };
    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_enterHook;
    SafetyHookMid g_exitHook;
    std::atomic<bool> g_running{true};
    std::atomic<bool> g_armed{false};
    std::atomic<DWORD> g_threadId{};
    std::atomic<std::uintptr_t> g_context{};
    std::uint64_t g_sequence{};
    std::size_t g_samples{};
    std::size_t g_ripCount{};
    std::array<RipEntry, kMaxRips> g_rips{};

    bool IsExecutable(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) || info.State != MEM_COMMIT) return false;
        const auto p = info.Protect & 0xff;
        return p == PAGE_EXECUTE || p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
    }

    std::uintptr_t Rva(std::uintptr_t address)
    {
        const auto base = reinterpret_cast<std::uintptr_t>(g_executable);
        return address >= base && address - base < 0x20000000 ? address - base : 0;
    }

    bool Plausible(float value) { return std::isfinite(value) && value >= 30.0f && value <= 160.0f; }

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

    float XmmLow(const M128A& value)
    {
        float result{};
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    void LogCandidate(std::uintptr_t rip, std::uint8_t lane, float value, const CONTEXT& context)
    {
        for (std::size_t i = 0; i < g_ripCount; ++i) {
            if (g_rips[i].rip != rip || g_rips[i].lane != lane) continue;
            ++g_rips[i].hits;
            return;
        }
        if (g_ripCount >= kMaxRips) return;
        auto& entry = g_rips[g_ripCount++];
        entry = { rip, 1, lane, value };
        g_logger->info("TRACE seq={} stage=sample-candidate thread={} context=0x{:X} ripRva=0x{:X} lane=xmm{} value={} "
            "rax=0x{:X} rcx=0x{:X} rdx=0x{:X} r8=0x{:X} r9=0x{:X} samples={}",
            ++g_sequence, GetCurrentThreadId(), g_context.load(std::memory_order_relaxed), Rva(rip), lane, value,
            static_cast<std::uintptr_t>(context.Rax), static_cast<std::uintptr_t>(context.Rcx),
            static_cast<std::uintptr_t>(context.Rdx), static_cast<std::uintptr_t>(context.R8),
            static_cast<std::uintptr_t>(context.R9), g_samples);
    }

    void SampleThread()
    {
        if (!g_armed.load(std::memory_order_acquire) || g_samples >= kMaxSamples) return;
        const auto threadId = g_threadId.load(std::memory_order_acquire);
        if (!threadId) return;
        HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, threadId);
        if (!thread) return;
        if (SuspendThread(thread) != static_cast<DWORD>(-1)) {
            CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT;
            if (GetThreadContext(thread, &context)) {
                ++g_samples;
                const float values[] = {
                    XmmLow(context.Xmm0), XmmLow(context.Xmm1), XmmLow(context.Xmm2), XmmLow(context.Xmm3),
                    XmmLow(context.Xmm4), XmmLow(context.Xmm5), XmmLow(context.Xmm6), XmmLow(context.Xmm7) };
                for (std::uint8_t lane = 0; lane < 8; ++lane)
                    if (Plausible(values[lane])) LogCandidate(static_cast<std::uintptr_t>(context.Rip), lane, values[lane], context);
            }
            ResumeThread(thread);
        }
        CloseHandle(thread);
    }

    DWORD WINAPI Monitor(void*)
    {
        while (g_running.load(std::memory_order_relaxed)) {
            SampleThread();
            Sleep(kSampleIntervalMs);
        }
        return 0;
    }

    void TraceEnter(SafetyHookContext& context)
    {
        std::uintptr_t itemContext{};
        const auto item = static_cast<std::uintptr_t>(context.rcx);
        if (!Read(item + 0x18, itemContext) || !itemContext) return;
        g_threadId.store(GetCurrentThreadId(), std::memory_order_release);
        g_context.store(itemContext, std::memory_order_release);
        g_samples = 0;
        g_ripCount = 0;
        g_rips = {};
        g_armed.store(true, std::memory_order_release);
        g_logger->info("TRACE seq={} stage=sample-window-enter thread={} item=0x{:X} context=0x{:X}",
            ++g_sequence, GetCurrentThreadId(), item, itemContext);
    }

    void TraceExit(SafetyHookContext&)
    {
        if (!g_armed.exchange(false, std::memory_order_acq_rel)) return;
        g_logger->info("TRACE seq={} stage=sample-window-exit thread={} context=0x{:X} samples={} uniqueRipLanes={}",
            ++g_sequence, GetCurrentThreadId(), g_context.load(std::memory_order_relaxed), g_samples, g_ripCount);
        for (std::size_t i = 0; i < g_ripCount; ++i)
            g_logger->info("TRACE seq={} stage=sample-summary ripRva=0x{:X} lane=xmm{} hits={} firstValue={}",
                ++g_sequence, Rva(g_rips[i].rip), g_rips[i].lane, g_rips[i].hits, g_rips[i].value);
    }

    bool Validate(std::uintptr_t address, const std::uint8_t* bytes, std::size_t size)
    { return IsExecutable(address) && std::memcmp(reinterpret_cast<const void*>(address), bytes, size) == 0; }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto path = std::filesystem::path(modulePath).remove_filename() / "STALKER2CinematicThreadSampleDiscovery204.log";
        std::ofstream(path, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicThreadSampleDiscovery204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
            const auto matches = Memory::PatternScanAll(g_executable, kEnterSignature);
            auto* exit = reinterpret_cast<std::uint8_t*>(g_executable) + kExitCallbackRva;
            if (matches.size() != 1 || !Validate(reinterpret_cast<std::uintptr_t>(exit), kExitCallbackBytes, sizeof(kExitCallbackBytes)))
                throw std::runtime_error("current 2.0.4 ENTER/EXIT validation failed");
            g_enterHook = safetyhook::create_mid(matches.front(), TraceEnter);
            g_exitHook = safetyhook::create_mid(exit, TraceExit);
            if (!g_enterHook || !g_exitHook) throw std::runtime_error("sample hooks failed");
            const auto monitor = CreateThread(nullptr, 0, Monitor, nullptr, 0, nullptr);
            if (!monitor) throw std::runtime_error("sample monitor thread failed");
            CloseHandle(monitor);
            g_logger->info("TRACE installed: 2.0.4 cinematic thread/XMM sampling discovery; read-only, bounded.");
        } catch (const std::exception& error) {
            g_running.store(false, std::memory_order_relaxed);
            g_exitHook.reset(); g_enterHook.reset();
            if (g_logger) g_logger->error("TRACE setup refused safely: {}", error.what());
        }
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
