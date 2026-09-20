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
    constexpr std::uintptr_t kEnterSetterRva = 0x6B7CB05;
    constexpr std::uintptr_t kExitCallbackRva = 0x6B6C482;
    constexpr std::uintptr_t kEnterConsumerCallRva = 0x2EE6936;
    constexpr std::uintptr_t kExitConsumerCallRva = 0x2EE69A7;
    constexpr std::uintptr_t kEnterScalarRva = 0x9EE151C;
    constexpr std::uint64_t kMaxRecords = 2000;

    constexpr std::uint8_t kEnterSetterBytes[] = {
        0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F,
    };
    constexpr std::uint8_t kExitCallbackBytes[] = {
        0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE,
    };
    constexpr std::uint8_t kConsumerCallBytes[] = { 0xE8, 0x71, 0x71, 0xC8, 0x03 };
    constexpr std::uint8_t kExitConsumerCallBytes[] = { 0xE8, 0x00, 0x71, 0xC8, 0x03 };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_enterHook;
    SafetyHookMid g_exitHook;
    SafetyHookMid g_enterConsumerHook;
    SafetyHookMid g_exitConsumerHook;
    std::atomic<std::uintptr_t> g_inner{};
    std::atomic<DWORD> g_enterThread{};
    std::atomic<bool> g_windowActive{};
    std::atomic<bool> g_exitSeen{};
    std::atomic<std::uint64_t> g_sequence{};
    std::atomic<std::uint64_t> g_records{};
    std::atomic<bool> g_preEnterSeen{};
    std::atomic<std::uintptr_t> g_preEnterContext{};
    std::atomic<std::int64_t> g_preEnterQpc{};
    std::atomic<DWORD> g_preEnterThread{};
    std::atomic<std::int64_t> g_enterQpc{};
    LARGE_INTEGER g_qpcFrequency{};

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
        return SafeRead(inner + 0x230, fov) && SafeRead(inner + 0x254, aspect) &&
            SafeRead(inner + 0x259, flags);
    }

    double SinceEnterMs()
    {
        const auto start = g_enterQpc.load(std::memory_order_acquire);
        if (!start || !g_qpcFrequency.QuadPart)
            return 0.0;
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        return (now.QuadPart - start) * 1000.0 / g_qpcFrequency.QuadPart;
    }

    void Enter(SafetyHookContext& context)
    {
        const auto inner = static_cast<std::uintptr_t>(context.rax);
        float fov{};
        float aspect{};
        std::uint8_t flags{};
        if (!inner || !ReadCameraState(inner, fov, aspect, flags))
            return;

        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        g_inner.store(inner, std::memory_order_release);
        g_enterThread.store(GetCurrentThreadId(), std::memory_order_release);
        g_enterQpc.store(now.QuadPart, std::memory_order_release);
        g_exitSeen.store(false, std::memory_order_release);
        g_records.store(0, std::memory_order_release);
        const auto preContext = g_preEnterContext.load(std::memory_order_acquire);
        const auto preQpc = g_preEnterQpc.load(std::memory_order_acquire);
        const auto preThread = g_preEnterThread.load(std::memory_order_acquire);
        g_windowActive.store(true, std::memory_order_release);
        if (g_logger)
            g_logger->info(
                "TRACE seq={} stage=enter inner=0x{:X} fov={} aspect={} flags=0x{:02X} "
                "thread={} preEnterSeen={} preEnterContext=0x{:X} preEnterThread={} "
                "preEnterDeltaQpc={}", ++g_sequence, inner, fov, aspect, flags,
                GetCurrentThreadId(), g_preEnterSeen.load(std::memory_order_acquire),
                preContext, preThread, preQpc ? now.QuadPart - preQpc : 0);
    }

    void Exit(SafetyHookContext&)
    {
        const auto inner = g_inner.load(std::memory_order_acquire);
        g_exitSeen.store(true, std::memory_order_release);
        if (g_logger && inner)
            g_logger->info("TRACE seq={} stage=exit inner=0x{:X} dtMs={} thread={}",
                ++g_sequence, inner, SinceEnterMs(), GetCurrentThreadId());
        g_windowActive.store(false, std::memory_order_release);
        g_preEnterSeen.store(false, std::memory_order_release);
    }

    void LogConsumer(SafetyHookContext& context, const char* branch, std::uintptr_t callRva,
        std::uintptr_t sourceAddress, const char* sourceKind)
    {
        const bool active = g_windowActive.load(std::memory_order_acquire);
        bool preEnter = false;
        if (!active && std::strcmp(branch, "ENTER") == 0) {
            bool expected = false;
            preEnter = g_preEnterSeen.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel);
            if (preEnter) {
                g_preEnterContext.store(static_cast<std::uintptr_t>(context.rcx),
                    std::memory_order_release);
                LARGE_INTEGER now{};
                QueryPerformanceCounter(&now);
                g_preEnterQpc.store(now.QuadPart, std::memory_order_release);
                g_preEnterThread.store(GetCurrentThreadId(), std::memory_order_release);
            }
        }
        if ((!active && !preEnter) ||
            g_records.fetch_add(1, std::memory_order_acq_rel) >= kMaxRecords)
            return;

        const auto inner = g_inner.load(std::memory_order_acquire);
        float fov{};
        float aspect{};
        std::uint8_t flags{};
        const bool readable = ReadCameraState(inner, fov, aspect, flags);
        float sourceValue{};
        const bool sourceReadable = sourceAddress != 0 && SafeRead(sourceAddress, sourceValue);
        const auto thread = GetCurrentThreadId();
        const auto enterThread = g_enterThread.load(std::memory_order_acquire);
        if (g_logger)
            g_logger->info(
                "TRACE seq={} stage=consumer-call phase={} preEnter={} branch={} callRva=0x{:X} sourceKind={} "
                "source=0x{:X} sourceReadable={} sourceValue={} xmm0={} "
                "inner=0x{:X} stateReadable={} fov={} aspect={} flags=0x{:02X} "
                "dtMs={} thread={} enterThread={} crossThread={} exitSeen={} "
                "rcx=0x{:X} rdx=0x{:X} r8=0x{:X} r9=0x{:X} rdi=0x{:X}",
                ++g_sequence, preEnter ? "PRE_ENTER" : branch, preEnter, branch, callRva, sourceKind, sourceAddress, sourceReadable,
                sourceValue, context.xmm0.f32[0], inner, readable, fov, aspect, flags,
                SinceEnterMs(), thread, enterThread, thread != enterThread,
                g_exitSeen.load(std::memory_order_acquire), context.rcx, context.rdx,
                context.r8, context.r9, context.rdi);
    }

    void TraceEnterConsumer(SafetyHookContext& context)
    {
        LogConsumer(context, "ENTER", kEnterConsumerCallRva, Base(kEnterScalarRva),
            "global");
    }

    void TraceExitConsumer(SafetyHookContext& context)
    {
        const auto source = static_cast<std::uintptr_t>(context.rdi);
        LogConsumer(context, "EXIT", kExitConsumerCallRva, source ? source + 0x38 : 0,
            "object+0x38");
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicLiveFovConsumerCorrelation204.log";
        std::ofstream(logPath, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt(
                "STALKER2CinematicLiveFovConsumerCorrelation204", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
            QueryPerformanceFrequency(&g_qpcFrequency);
            if (!Validate(kEnterSetterRva, kEnterSetterBytes, sizeof(kEnterSetterBytes)) ||
                !Validate(kExitCallbackRva, kExitCallbackBytes, sizeof(kExitCallbackBytes)) ||
                !Validate(kEnterConsumerCallRva, kConsumerCallBytes, sizeof(kConsumerCallBytes)) ||
                !Validate(kExitConsumerCallRva, kExitConsumerCallBytes, sizeof(kExitConsumerCallBytes)))
                throw std::runtime_error("2.0.4 anchor/callsite bytes mismatch");

            g_enterHook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kEnterSetterRva)), Enter);
            g_exitHook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kExitCallbackRva)), Exit);
            g_enterConsumerHook = safetyhook::create_mid(
                reinterpret_cast<void*>(Base(kEnterConsumerCallRva)), TraceEnterConsumer);
            g_exitConsumerHook = safetyhook::create_mid(
                reinterpret_cast<void*>(Base(kExitConsumerCallRva)), TraceExitConsumer);
            if (!g_enterHook || !g_exitHook || !g_enterConsumerHook || !g_exitConsumerHook)
                throw std::runtime_error("hook creation failed");

            g_logger->info(
                "TRACE installed: 2.0.4 live-FOV consumer correlation; two exact callsites; read-only, no writes.");
        } catch (const std::exception& error) {
            g_exitConsumerHook.reset();
            g_enterConsumerHook.reset();
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
