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
    constexpr std::uintptr_t kEnterConsumerRva = 0x2EE6936;
    constexpr std::uintptr_t kExitConsumerRva = 0x2EE69A7;
    constexpr std::uintptr_t kEnterScalarRva = 0x9EE151C;
    constexpr float kNativeAspect = 16.0f / 9.0f;
    constexpr float kTargetAspect = 32.0f / 9.0f;
    constexpr std::uint64_t kMaxRecords = 32;

    constexpr std::uint8_t kEnterBytes[] = { 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00, 0x39, 0x8E, 0xE3, 0x3F };
    constexpr std::uint8_t kExitBytes[] = { 0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x89, 0xCE };
    constexpr std::uint8_t kEnterCallBytes[] = { 0xE8, 0x71, 0x71, 0xC8, 0x03 };
    constexpr std::uint8_t kExitCallBytes[] = { 0xE8, 0x00, 0x71, 0xC8, 0x03 };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_enterHook, g_exitHook, g_enterConsumerHook, g_exitConsumerHook;
    std::atomic<std::uintptr_t> g_inner{};
    std::atomic<bool> g_active{};
    std::atomic<bool> g_exitSeen{};
    std::atomic<bool> g_correctionApplied{};
    std::atomic<std::uint64_t> g_sequence{};
    std::atomic<std::uint64_t> g_records{};

    template <typename T> bool Read(std::uintptr_t address, T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address >= end || sizeof(T) > end - address) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
        return true;
    }

    std::uintptr_t Base(std::uintptr_t rva) { return reinterpret_cast<std::uintptr_t>(g_executable) + rva; }
    bool Valid(std::uintptr_t rva, const std::uint8_t* bytes, std::size_t size)
    { return std::memcmp(reinterpret_cast<const void*>(Base(rva)), bytes, size) == 0; }

    bool State(std::uintptr_t inner, float& fov, float& aspect, std::uint8_t& flags)
    { return inner && Read(inner + 0x230, fov) && Read(inner + 0x254, aspect) && Read(inner + 0x259, flags); }

    float HorPlus(float fov)
    {
        constexpr float pi = 3.14159265358979323846f;
        const float half = fov * (pi / 360.0f);
        return 2.0f * std::atan(std::tan(half) * (kTargetAspect / kNativeAspect)) * (180.0f / pi);
    }

    void Enter(SafetyHookContext& context)
    {
        const auto inner = static_cast<std::uintptr_t>(context.rax);
        float fov{}, aspect{}; std::uint8_t flags{};
        if (!State(inner, fov, aspect, flags)) return;
        g_inner.store(inner, std::memory_order_release);
        g_active.store(true, std::memory_order_release);
        g_exitSeen.store(false, std::memory_order_release);
        g_records.store(0, std::memory_order_release);
        if (g_logger) g_logger->info("TRACE seq={} stage=enter inner=0x{:X} fov={} aspect={} flags=0x{:02X} thread={}",
            ++g_sequence, inner, fov, aspect, flags, GetCurrentThreadId());
    }

    void Exit(SafetyHookContext&)
    {
        const auto inner = g_inner.load(std::memory_order_acquire);
        if (g_logger && inner) g_logger->info("TRACE seq={} stage=exit inner=0x{:X} thread={}",
            ++g_sequence, inner, GetCurrentThreadId());
        g_exitSeen.store(true, std::memory_order_release);
        g_active.store(false, std::memory_order_release);
        g_inner.store(0, std::memory_order_release);
        g_correctionApplied.store(false, std::memory_order_release);
    }

    void TraceEnterConsumer(SafetyHookContext& context)
    {
        const bool active = g_active.load(std::memory_order_acquire);
        bool preEnter = false;
        if (!active) {
            bool expected = false;
            preEnter = g_correctionApplied.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
        } else {
            bool expected = false;
            preEnter = g_correctionApplied.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
        }
        if (!preEnter || g_records.fetch_add(1, std::memory_order_acq_rel) >= kMaxRecords) return;

        const float before = context.xmm0.f32[0];
        const float after = (std::isfinite(before) && before > 1.0f && before < 179.0f)
            ? HorPlus(before) : before;
        if (std::isfinite(after) && after > 1.0f && after < 179.0f)
            context.xmm0.f32[0] = after;

        float fov{}, aspect{}; std::uint8_t flags{};
        const auto inner = g_inner.load(std::memory_order_acquire);
        const bool readable = State(inner, fov, aspect, flags);
        if (g_logger) g_logger->info(
            "TRACE seq={} stage=enter-fov-transform phase={} callRva=0x{:X} source=0x{:X} "
            "beforeXmm0={} afterXmm0={} targetAspect={} inner=0x{:X} stateReadable={} "
            "stateFov={} stateAspect={} stateFlags=0x{:02X} thread={} active={}",
            ++g_sequence, active ? "ENTER" : "PRE_ENTER", kEnterConsumerRva, Base(kEnterScalarRva),
            before, context.xmm0.f32[0], kTargetAspect, inner, readable, fov, aspect, flags,
            GetCurrentThreadId(), active);
    }

    void TraceExitConsumer(SafetyHookContext& context)
    {
        if (!g_active.load(std::memory_order_acquire) ||
            g_records.fetch_add(1, std::memory_order_acq_rel) >= kMaxRecords) return;
        const auto inner = g_inner.load(std::memory_order_acquire);
        float fov{}, aspect{}; std::uint8_t flags{};
        const bool readable = State(inner, fov, aspect, flags);
        float source{}; const auto sourceAddress = static_cast<std::uintptr_t>(context.rdi) + 0x38;
        const bool sourceReadable = Read(sourceAddress, source);
        if (g_logger) g_logger->info(
            "TRACE seq={} stage=exit-consumer callRva=0x{:X} source=0x{:X} sourceReadable={} "
            "sourceValue={} xmm0={} inner=0x{:X} stateReadable={} stateFov={} stateAspect={} "
            "stateFlags=0x{:02X} thread={}", ++g_sequence, kExitConsumerRva, sourceAddress,
            sourceReadable, source, context.xmm0.f32[0], inner, readable, fov, aspect, flags,
            GetCurrentThreadId());
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{}; GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto path = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicLiveFovConsumerWriteFeasibility204.log";
        std::ofstream(path, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicLiveFovConsumerWriteFeasibility204", path.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v"); g_logger->flush_on(spdlog::level::info);
            if (!Valid(kEnterSetterRva, kEnterBytes, sizeof(kEnterBytes)) ||
                !Valid(kExitCallbackRva, kExitBytes, sizeof(kExitBytes)) ||
                !Valid(kEnterConsumerRva, kEnterCallBytes, sizeof(kEnterCallBytes)) ||
                !Valid(kExitConsumerRva, kExitCallBytes, sizeof(kExitCallBytes)))
                throw std::runtime_error("2.0.4 anchor/callsite bytes mismatch");
            g_enterHook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kEnterSetterRva)), Enter);
            g_exitHook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kExitCallbackRva)), Exit);
            g_enterConsumerHook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kEnterConsumerRva)), TraceEnterConsumer);
            g_exitConsumerHook = safetyhook::create_mid(reinterpret_cast<void*>(Base(kExitConsumerRva)), TraceExitConsumer);
            if (!g_enterHook || !g_exitHook || !g_enterConsumerHook || !g_exitConsumerHook)
                throw std::runtime_error("hook creation failed");
            g_logger->info("TRACE installed: 2.0.4 live-FOV consumer write feasibility; XMM0 ENTER only; no state writes.");
        } catch (const std::exception& error) {
            g_exitConsumerHook.reset(); g_enterConsumerHook.reset(); g_exitHook.reset(); g_enterHook.reset();
            if (g_logger) g_logger->error("TRACE setup refused safely: {}", error.what());
        }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = module; DisableThreadLibraryCalls(module);
    if (const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
    return TRUE;
}
