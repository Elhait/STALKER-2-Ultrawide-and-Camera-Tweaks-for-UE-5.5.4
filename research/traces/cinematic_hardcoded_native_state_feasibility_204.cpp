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
#include <sstream>
#include <stdexcept>

namespace
{
    // Current-build camera-writer contract observed in the native A/B/C trace.
    constexpr std::uint8_t kCameraWriterSignature[] = {
        0xF6, 0x86, 0x62, 0x02, 0x00, 0x00, 0x10,
        0xF3, 0x0F, 0x10, 0x86, 0x30, 0x02, 0x00, 0x00,
        0x0F, 0x85, 0x00, 0x00, 0x00, 0x00,
        0x48, 0x8D, 0x4B, 0x30,
        0xF3, 0x0F, 0x11, 0x43, 0x30,
        0xF3, 0x0F, 0x10, 0x86, 0x54, 0x02, 0x00, 0x00,
        0xF3, 0x0F, 0x11, 0x43, 0x5C,
        0x0F, 0xB6, 0x96, 0x59, 0x02, 0x00, 0x00,
        0x8B, 0x43, 0x68, 0x83, 0xE2, 0x01, 0x83, 0xE0, 0xFE,
        0x09, 0xD0, 0x89, 0x43, 0x68,
        0x0F, 0xB6, 0x96, 0x59, 0x02, 0x00, 0x00,
        0x83, 0xE2, 0x04, 0x83, 0xE0, 0xFB, 0x09, 0xD0, 0x89,
        0x43, 0x68, 0x8A, 0x96, 0x63, 0x02, 0x00, 0x00, 0x88,
        0x53, 0x6C,
    };

    constexpr std::size_t kFovWriteOffset = 25;
    constexpr std::uintptr_t kAspectOffset = 0x254;
    constexpr std::uintptr_t kFlagsOffset = 0x259;
    constexpr float kNativeAspect = 16.0f / 9.0f;
    constexpr float kObservedWideAspect = 32.0f / 9.0f;
    constexpr float kAspectEpsilon = 0.001f;

    // These are the already validated current-build lifecycle boundaries.
    constexpr std::uintptr_t kCinematicEnterRva = 0x2EE6936;
    constexpr std::uintptr_t kCinematicExitRva = 0x2EE69A7;
    constexpr std::uint8_t kCinematicEnterBytes[] = { 0xE8, 0x71, 0x71, 0xC8, 0x03 };
    constexpr std::uint8_t kCinematicExitBytes[] = { 0xE8, 0x00, 0x71, 0xC8, 0x03 };

    enum class Lifecycle : std::uint8_t { Gameplay, Cinematic };
    enum class ReplayPhase : std::uint8_t { Idle, ConstrainedPass, NativeState };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::shared_ptr<spdlog::logger> g_logger;
    SafetyHookMid g_writerHook;
    SafetyHookMid g_enterHook;
    SafetyHookMid g_exitHook;
    std::atomic<Lifecycle> g_lifecycle{ Lifecycle::Gameplay };
    std::atomic<ReplayPhase> g_phase{ ReplayPhase::Idle };
    std::atomic<std::uint64_t> g_sequence{};

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

    template <typename T>
    bool Write(std::uintptr_t address, const T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        constexpr DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if (address >= end || sizeof(T) > end - address || !(info.Protect & writable)) return false;
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T));
        return true;
    }

    template <typename... Args>
    void Log(Args&&... args)
    {
        if (!g_logger) return;
        std::ostringstream message;
        (message << ... << args);
        g_logger->info("{}", message.str());
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

    bool Near(float value, float expected)
    {
        return std::isfinite(value) && std::fabs(value - expected) <= kAspectEpsilon;
    }

    bool WriteNativeState(std::uintptr_t source, float aspect, std::uint8_t flags)
    {
        return Write(source + kAspectOffset, aspect) && Write(source + kFlagsOffset, flags);
    }

    void LogState(const char* stage, std::uintptr_t source, float fov, float aspect,
        std::uint8_t flags, bool changed)
    {
        float afterAspect{};
        std::uint8_t afterFlags{};
        const bool readable = Read(source + kAspectOffset, afterAspect) && Read(source + kFlagsOffset, afterFlags);
        Log("TRACE seq=", ++g_sequence, " stage=", stage,
            " source=0x", std::hex, source, std::dec,
            " fov=", fov, " beforeAspect=", aspect,
            " beforeFlags=0x", std::hex, static_cast<int>(flags), std::dec,
            " afterAspect=", readable ? afterAspect : 0.0f,
            " afterFlags=0x", readable ? static_cast<int>(afterFlags) : -1,
            " changed=", changed, " readable=", readable);
    }

    void OnCinematicEnter(SafetyHookContext& context)
    {
        g_phase.store(ReplayPhase::Idle, std::memory_order_release);
        g_lifecycle.store(Lifecycle::Cinematic, std::memory_order_release);
        Log("TRACE cinematic-enter observed; authored FOV lane preserved; hardcoded native-state probe armed.");
    }

    void OnCinematicExit(SafetyHookContext& context)
    {
        g_lifecycle.store(Lifecycle::Gameplay, std::memory_order_release);
        g_phase.store(ReplayPhase::Idle, std::memory_order_release);
        Log("TRACE cinematic-exit observed; probe disarmed; native gameplay pass-through restored.");
    }

    void OnCameraWriter(SafetyHookContext& context)
    {
        if (g_lifecycle.load(std::memory_order_acquire) != Lifecycle::Cinematic) return;

        const auto source = static_cast<std::uintptr_t>(context.rsi);
        float aspect{};
        std::uint8_t flags{};
        if (!Read(source + kAspectOffset, aspect) || !Read(source + kFlagsOffset, flags)) {
            Log("TRACE cinematic-state refused: source fields unreadable; pass-through.");
            return;
        }

        // Reproduce only the native gameplay transition topology with authored
        // FOV untouched. This is deliberately a one-way feasibility probe.
        const auto phase = g_phase.load(std::memory_order_acquire);
        if (phase == ReplayPhase::Idle && Near(aspect, kObservedWideAspect) && flags == 0x04) {
            const bool changed = WriteNativeState(source, kObservedWideAspect, 0x05);
            if (changed) g_phase.store(ReplayPhase::ConstrainedPass, std::memory_order_release);
            LogState("cinematic-probe-A-to-B1", source, context.xmm0.f32[0], aspect, flags, changed);
            return;
        }
        if (phase == ReplayPhase::ConstrainedPass && Near(aspect, kObservedWideAspect) && flags == 0x05) {
            const bool changed = WriteNativeState(source, kNativeAspect, 0x05);
            if (changed) g_phase.store(ReplayPhase::NativeState, std::memory_order_release);
            LogState("cinematic-probe-A-to-B2", source, context.xmm0.f32[0], aspect, flags, changed);
            return;
        }
        if (phase == ReplayPhase::NativeState && Near(aspect, kNativeAspect) && flags == 0x05) {
            const bool changed = WriteNativeState(source, kNativeAspect, 0x04);
            LogState("cinematic-probe-B-to-C", source, context.xmm0.f32[0], aspect, flags, changed);
            return;
        }

        LogState("cinematic-probe-pass-through", source, context.xmm0.f32[0], aspect, flags, false);
    }

    bool ValidateWriter(std::uint8_t*& hookAddress)
    {
        const auto matches = Memory::PatternScanAll(g_executable, "F6 86 62 02 00 00 10 F3 0F 10 86 30 02 00 00 0F 85 ?? ?? ?? ?? 48 8D 4B 30 F3 0F 11 43 30 F3 0F 10 86 54 02 00 00 F3 0F 11 43 5C 0F B6 96 59 02 00 00 8B 43 68 83 E2 01 83 E0 FE 09 D0 89 43 68 0F B6 96 59 02 00 00 83 E2 04 83 E0 FB 09 D0 89 43 68 8A 96 63 02 00 00 88 53 6C");
        if (matches.size() != 1) return false;
        hookAddress = matches.front() + kFovWriteOffset;
        return IsExecutable(reinterpret_cast<std::uintptr_t>(hookAddress)) &&
            std::memcmp(matches.front(), kCameraWriterSignature, 17) == 0 &&
            std::memcmp(matches.front() + 21, kCameraWriterSignature + 21,
                sizeof(kCameraWriterSignature) - 21) == 0;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() /
            "STALKER2CinematicHardcodedNativeStateFeasibility204.log";
        std::ofstream(logPath, std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicHardcodedNativeStateFeasibility204", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);

            Log("Cinematic hardcoded native-state feasibility bootstrap started.");
            Log("Scope: cinematic A/B/C state probe only; gameplay and production CameraTweaks untouched.");

            auto* enter = reinterpret_cast<std::uint8_t*>(reinterpret_cast<std::uintptr_t>(g_executable) + kCinematicEnterRva);
            auto* exit = reinterpret_cast<std::uint8_t*>(reinterpret_cast<std::uintptr_t>(g_executable) + kCinematicExitRva);
            if (!IsExecutable(reinterpret_cast<std::uintptr_t>(enter)) ||
                !IsExecutable(reinterpret_cast<std::uintptr_t>(exit)) ||
                std::memcmp(enter, kCinematicEnterBytes, sizeof(kCinematicEnterBytes)) != 0 ||
                std::memcmp(exit, kCinematicExitBytes, sizeof(kCinematicExitBytes)) != 0)
                throw std::runtime_error("cinematic lifecycle boundary validation failed");

            std::uint8_t* writer{};
            if (!ValidateWriter(writer)) throw std::runtime_error("camera-writer signature validation failed");

            g_enterHook = safetyhook::create_mid(enter, OnCinematicEnter);
            g_exitHook = safetyhook::create_mid(exit, OnCinematicExit);
            g_writerHook = safetyhook::create_mid(writer, OnCameraWriter);
            if (!g_enterHook || !g_exitHook || !g_writerHook)
                throw std::runtime_error("experimental hook creation failed");

            Log("TRACE installed: lifecycle boundaries and validated camera writer.");
            Log("TRACE policy: FOV preserved; probe target states are aspect=3.555556/flags=0x05, then aspect=1.777778/flags=0x05, then flags=0x04.");
        } catch (const std::exception& exception) {
            g_writerHook.reset();
            g_exitHook.reset();
            g_enterHook.reset();
            Log("TRACE setup refused safely: ", exception.what());
        } catch (...) {
            g_writerHook.reset();
            g_exitHook.reset();
            g_enterHook.reset();
            Log("TRACE setup refused safely: unknown exception.");
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
