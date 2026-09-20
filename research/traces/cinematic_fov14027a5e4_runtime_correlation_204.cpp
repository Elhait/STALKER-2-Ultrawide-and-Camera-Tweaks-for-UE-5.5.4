#include "helper.hpp"

#include <safetyhook.hpp>
#include <spdlog/sinks/basic_file_sink.h>

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
    constexpr std::uintptr_t kEnterSetterRva = 0x6B7CB05;
    constexpr std::uintptr_t kExitCallbackRva = 0x6B6C482;
    constexpr std::uintptr_t kCandidateRva = 0x27A5E4;
    constexpr std::uint64_t kMaxCandidateRecords = 5000;

    constexpr std::uint8_t kEnterSetterBytes[] = {
        0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F,
    };
    constexpr std::uint8_t kExitCallbackBytes[] = {
        0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE,
    };
    constexpr std::uint8_t kCandidateBytes[] = {
        0x41, 0x56, 0x56, 0x57, 0x53, 0x48, 0x81, 0xEC, 0x98, 0x00,
    };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_enterHook;
    SafetyHookMid g_exitHook;
    SafetyHookMid g_candidateHook;
    std::atomic<std::uintptr_t> g_inner{};
    std::atomic<DWORD> g_enterThread{};
    std::atomic<bool> g_windowActive{};
    std::atomic<std::uint64_t> g_sequence{};
    std::atomic<std::uint64_t> g_candidateRecords{};
    std::atomic<std::int64_t> g_enterQpc{};

    template <typename T>
    bool SafeRead(std::uintptr_t address, T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address >= end || sizeof(T) > end - address)
            return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
        return true;
    }

    std::uintptr_t Base(std::uintptr_t rva)
    {
        return reinterpret_cast<std::uintptr_t>(g_executable) + rva;
    }

    bool Validate(std::uintptr_t rva, const std::uint8_t* bytes, std::size_t size)
    {
        return std::memcmp(reinterpret_cast<const void*>(Base(rva)), bytes, size) == 0;
    }

    bool ReadCameraState(std::uintptr_t inner, float& fov, float& aspect, std::uint8_t& flags)
    {
        return SafeRead(inner + 0x230, fov) &&
            SafeRead(inner + 0x254, aspect) &&
            SafeRead(inner + 0x259, flags);
    }

    struct CandidateState
    {
        float time43c{};
        float time440{};
        float accumulator460{};
        float threshold464{};
        float progress46c{};
        std::uint32_t state470{};
        std::uint8_t gate6a3{};
        std::uint8_t gate6a4{};
        float input6f8{};
    };

    bool ReadCandidateState(std::uintptr_t object, CandidateState& state)
    {
        return SafeRead(object + 0x43C, state.time43c) &&
            SafeRead(object + 0x440, state.time440) &&
            SafeRead(object + 0x460, state.accumulator460) &&
            SafeRead(object + 0x464, state.threshold464) &&
            SafeRead(object + 0x46C, state.progress46c) &&
            SafeRead(object + 0x470, state.state470) &&
            SafeRead(object + 0x6A3, state.gate6a3) &&
            SafeRead(object + 0x6A4, state.gate6a4) &&
            SafeRead(object + 0x6F8, state.input6f8);
    }

    std::int64_t QpcNow()
    {
        LARGE_INTEGER value{};
        QueryPerformanceCounter(&value);
        return value.QuadPart;
    }

    void Enter(SafetyHookContext& context)
    {
        const auto inner = static_cast<std::uintptr_t>(context.rax);
        float fov{};
        float aspect{};
        std::uint8_t flags{};
        if (!inner || !ReadCameraState(inner, fov, aspect, flags))
            return;

        g_inner.store(inner, std::memory_order_release);
        g_enterThread.store(GetCurrentThreadId(), std::memory_order_release);
        g_enterQpc.store(QpcNow(), std::memory_order_release);
        g_candidateRecords.store(0, std::memory_order_release);
        g_windowActive.store(true, std::memory_order_release);
        if (g_logger)
            g_logger->info(
                "TRACE seq={} stage=enter inner=0x{:X} fov={} aspect={} flags=0x{:02X} thread={}",
                ++g_sequence, inner, fov, aspect, flags, GetCurrentThreadId());
    }

    void Exit(SafetyHookContext&)
    {
        const auto inner = g_inner.load(std::memory_order_acquire);
        if (g_logger && inner)
            g_logger->info("TRACE seq={} stage=exit inner=0x{:X} thread={}",
                ++g_sequence, inner, GetCurrentThreadId());
        g_windowActive.store(false, std::memory_order_release);
        g_inner.store(0, std::memory_order_release);
        g_enterThread.store(0, std::memory_order_release);
        g_enterQpc.store(0, std::memory_order_release);
    }

    void Candidate(SafetyHookContext& context)
    {
        if (!g_windowActive.load(std::memory_order_acquire) ||
            g_candidateRecords.fetch_add(1, std::memory_order_acq_rel) >= kMaxCandidateRecords)
            return;

        const auto object = static_cast<std::uintptr_t>(context.rcx);
        CandidateState state{};
        if (!object || !ReadCandidateState(object, state))
            return;

        const auto inner = g_inner.load(std::memory_order_acquire);
        float fov{};
        float aspect{};
        std::uint8_t flags{};
        if (!ReadCameraState(inner, fov, aspect, flags))
            return;

        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        const auto enterQpc = g_enterQpc.load(std::memory_order_acquire);
        const auto dtMs = frequency.QuadPart != 0
            ? (QpcNow() - enterQpc) * 1000.0 / frequency.QuadPart
            : 0.0;
        const auto thread = GetCurrentThreadId();
        const auto enterThread = g_enterThread.load(std::memory_order_acquire);
        const bool crossThread = thread != enterThread;
        const bool finiteProgress = std::isfinite(state.time43c) &&
            std::isfinite(state.time440) && std::isfinite(state.accumulator460) &&
            std::isfinite(state.threshold464) && std::isfinite(state.progress46c) &&
            std::isfinite(state.input6f8);

        if (g_logger)
            g_logger->info(
                "TRACE seq={} stage=candidate-hit candidate=FUN_14027A5E4 siteRva=0x{:X} "
                "object=0x{:X} inner=0x{:X} sameObject={} fov={} aspect={} flags=0x{:02X} "
                "argXmm1={} rdx=0x{:X} r8=0x{:X} r9=0x{:X} time43c={} time440={} "
                "accumulator460={} threshold464={} progress46c={} state470=0x{:08X} "
                "gate6a3=0x{:02X} gate6a4=0x{:02X} input6f8={} finiteProgress={} "
                "dtMs={} thread={} enterThread={} crossThread={}",
                ++g_sequence, kCandidateRva, object, inner, object == inner, fov, aspect, flags,
                context.xmm1.f32[0], context.rdx, context.r8, context.r9, state.time43c,
                state.time440, state.accumulator460, state.threshold464, state.progress46c,
                state.state470, state.gate6a3, state.gate6a4, state.input6f8, finiteProgress,
                dtMs, thread, enterThread, crossThread);
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicFov14027A5E4RuntimeCorrelation204.log";
        std::ofstream(logPath, std::ios::trunc).close();

        try {
            g_logger = spdlog::basic_logger_mt(
                "STALKER2CinematicFov14027A5E4RuntimeCorrelation204", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
            if (!Validate(kEnterSetterRva, kEnterSetterBytes, sizeof(kEnterSetterBytes)) ||
                !Validate(kExitCallbackRva, kExitCallbackBytes, sizeof(kExitCallbackBytes)) ||
                !Validate(kCandidateRva, kCandidateBytes, sizeof(kCandidateBytes)))
                throw std::runtime_error("current 2.0.4 bytes mismatch");

            g_enterHook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kEnterSetterRva)), Enter);
            g_exitHook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kExitCallbackRva)), Exit);
            g_candidateHook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kCandidateRva)), Candidate);
            if (!g_enterHook || !g_exitHook || !g_candidateHook)
                throw std::runtime_error("hook creation failed");

            g_logger->info(
                "TRACE installed: 2.0.4 FUN_14027A5E4 runtime correlation; read-only, no writes.");
        } catch (const std::exception& error) {
            g_candidateHook.reset();
            g_exitHook.reset();
            g_enterHook.reset();
            if (g_logger)
                g_logger->error("TRACE setup refused safely: {}", error.what());
        }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH)
        return TRUE;
    g_module = module;
    DisableThreadLibraryCalls(module);
    if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr))
        CloseHandle(thread);
    return TRUE;
}
