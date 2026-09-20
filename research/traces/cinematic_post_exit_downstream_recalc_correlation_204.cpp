#include "helper.hpp"

#include <safetyhook.hpp>
#include <Zydis.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace
{
    // Global integration copy; stable gameplay source remains untouched.

    // This sequence identifies the complete camera-view copy path, not merely a
    // generic MOVSS instruction. The JNZ displacement may change between builds.
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
    constexpr std::size_t kFovWriteOffsetInSignature = 25;
    constexpr std::uintptr_t kAspectOffset = 0x254;
    constexpr std::uintptr_t kFlagsOffset = 0x259;
    constexpr float kWideAspect = 32.0f / 9.0f;
    constexpr float kCinemaAspect = 21.0f / 9.0f;
    constexpr float kNativeAspect = 16.0f / 9.0f;
    constexpr std::uintptr_t kCinematicAspectStoreRva = 0x6B7CB05;
    constexpr std::uintptr_t kCinematicEnterConsumerRva = 0x2EE6936;
    constexpr std::uintptr_t kCinematicExitConsumerRva = 0x2EE69A7;
    constexpr std::uint8_t kCinematicStorePrefix[] = { 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00 };
    constexpr std::uint8_t kCinematicOriginalImmediate[] = { 0x39, 0x8E, 0xE3, 0x3F };
    constexpr std::uint8_t kCinematicEnterConsumerBytes[] = { 0xE8, 0x71, 0x71, 0xC8, 0x03 };
    constexpr std::uint8_t kCinematicExitConsumerBytes[] = { 0xE8, 0x00, 0x71, 0xC8, 0x03 };
    constexpr std::size_t kCinematicImmediateOffset = 6;
    constexpr float kRecoveryEpsilon = 0.01f;
    constexpr std::uintptr_t kDownstreamRecalcRva = 0xAF4FA4;
    constexpr std::uint8_t kDownstreamRecalcPrologue[] = {
        0x56, 0x57, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x0F, 0x29, 0x7C, 0x24, 0x30 };

    enum class CoordinatorState : std::uint32_t { Gameplay, CinematicActive, CinematicExiting };

    enum class ReplayState : std::uint32_t { WaitingForAutomaticUpdate, AppliedConstrainPass, Complete };

    HMODULE g_module{};
    HMODULE g_executable = GetModuleHandle(nullptr);
    std::uint8_t* g_fovWriteAddress{};
    SafetyHookMid g_hook;
    std::shared_ptr<spdlog::logger> g_logger;
    std::atomic<ReplayState> g_state{ReplayState::WaitingForAutomaticUpdate};
    std::atomic<std::uint32_t> g_lastCameraMode{};
    std::atomic<std::uint64_t> g_transitionTraceSequence{0};
    std::atomic<CoordinatorState> g_coordinator{CoordinatorState::Gameplay};
    std::atomic<bool> g_cinematicFovApplied{false};
    std::atomic<float> g_exitTargetFov{0.0f};
    SafetyHookMid g_cinematicEnterHook;
    SafetyHookMid g_cinematicExitHook;
    SafetyHookMid g_downstreamRecalcHook;
    std::uint8_t* g_cinematicAspectStore{};
    std::uint8_t g_cinematicOriginalImmediate[sizeof(kCinematicOriginalImmediate)]{};
    bool g_cinematicAspectPatched{};
    std::atomic<std::uintptr_t> g_lastAbcOutput{};
    std::atomic<int> g_pendingAbcStage{};
#ifdef GAMEPLAY_ONE_SHOT_CINEMATIC_TRIGGER
    std::atomic_bool g_oneShotCinematicTriggerArmed{false};
#endif
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
    std::atomic<std::uintptr_t> g_lastCameraSource{};
    std::atomic_bool g_suppressRearmAfterManualRestore{false};
    std::atomic_bool g_readyForOneShot{};
    std::atomic_bool g_readyMarkerLogged{};
    thread_local bool g_logCombinedOutputForInvocation{};

    struct CombinedOutputSnapshot
    {
        std::uintptr_t source{};
        std::uintptr_t output{};
        float sourceAspect{};
        std::uint8_t sourceFlags{};
        float outputAspect{};
        std::uint64_t outputField40{};
        std::uint64_t outputField58{};
        std::uint64_t outputField68{};
        bool valid{};
    };
    thread_local CombinedOutputSnapshot g_lastCombinedOutput{};

    struct ResolutionSnapshot
    {
        DWORD displayWidth{};
        DWORD displayHeight{};
        LONG windowWidth{};
        LONG windowHeight{};
        LONG clientWidth{};
        LONG clientHeight{};
        bool valid{};
    };
    ResolutionSnapshot g_lastResolutionSnapshot{};
#endif

    const char* ReplayStateName(ReplayState state)
    {
        switch (state) {
        case ReplayState::WaitingForAutomaticUpdate: return "WaitingForAutomaticUpdate";
        case ReplayState::AppliedConstrainPass: return "AppliedConstrainPass";
        case ReplayState::Complete: return "Complete";
        }
        return "Unknown";
    }

    template <typename... Args>
    void Log(Args&&... args)
    {
        if (!g_logger) return;
        std::ostringstream message;
        (message << ... << args);
        g_logger->info("{}", message.str());
    }

    bool IsWritable(std::uintptr_t address, std::size_t size)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        return address <= regionEnd && size <= regionEnd - address && (info.Protect & writable);
    }

    template <typename T>
    bool SafeRead(std::uintptr_t address, T& value)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (address > regionEnd || sizeof(value) > regionEnd - address) return false;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
        return true;
    }

    bool WriteAspectAndFlags(std::uintptr_t source, float aspect, std::uint8_t flags)
    {
        if (!IsWritable(source + kAspectOffset, sizeof(aspect)) || !IsWritable(source + kFlagsOffset, sizeof(flags))) return false;
        std::memcpy(reinterpret_cast<void*>(source + kAspectOffset), &aspect, sizeof(aspect));
        std::memcpy(reinterpret_cast<void*>(source + kFlagsOffset), &flags, sizeof(flags));
        return true;
    }

    bool LogCameraModeChange(std::uintptr_t source, std::uintptr_t output, float primaryFov,
        float aspect, std::uint8_t flags)
    {
        float secondaryFov = 0.0f;
        float outputFov = 0.0f;
        float outputAspect = 0.0f;
        float rawSourceAspect = 0.0f;
        std::uint8_t rawSourceFlags = 0;
        std::uint8_t selector = 0;
        SafeRead(source + 0x234, secondaryFov);
        SafeRead(source + kAspectOffset, rawSourceAspect);
        SafeRead(source + kFlagsOffset, rawSourceFlags);
        SafeRead(source + 0x262, selector);
        SafeRead(output + 0x30, outputFov);
        SafeRead(output + 0x5C, outputAspect);

#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
        std::uint64_t outputField40 = 0;
        std::uint64_t outputField58 = 0;
        std::uint64_t outputField68 = 0;
        if (!SafeRead(output + 0x40, outputField40) || !SafeRead(output + 0x58, outputField58) ||
            !SafeRead(output + 0x68, outputField68))
            return false;
        const CombinedOutputSnapshot snapshot{
            source, output, aspect, flags, outputAspect,
            outputField40, outputField58, outputField68, true };
        const bool outputChanged = !g_lastCombinedOutput.valid ||
            snapshot.source != g_lastCombinedOutput.source ||
            snapshot.output != g_lastCombinedOutput.output ||
            snapshot.sourceAspect != g_lastCombinedOutput.sourceAspect ||
            snapshot.sourceFlags != g_lastCombinedOutput.sourceFlags ||
            snapshot.outputAspect != g_lastCombinedOutput.outputAspect ||
            snapshot.outputField58 != g_lastCombinedOutput.outputField58;
        g_lastCombinedOutput = snapshot;
#else
        const bool outputChanged = false;
#endif

        std::uint32_t aspectBits{};
        std::memcpy(&aspectBits, &aspect, sizeof(aspectBits));
        const std::uint32_t mode = aspectBits ^ (static_cast<std::uint32_t>(flags) << 1) ^
            (static_cast<std::uint32_t>(selector) << 9);
        const bool sourceModeChanged = g_lastCameraMode.exchange(mode, std::memory_order_relaxed) != mode;
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
        g_logCombinedOutputForInvocation = sourceModeChanged || outputChanged;
        if (!g_logCombinedOutputForInvocation)
            return false;
#else
        if (!sourceModeChanged)
            return false;
#endif

        Log("Camera mode: source=0x", std::hex, source, std::dec,
            " rawSourceAspect=", rawSourceAspect, " rawSourceFlags=0x", std::hex,
            static_cast<int>(rawSourceFlags), std::dec,
            " primaryFOV=", primaryFov, " secondaryFOV=", secondaryFov,
            " aspect=", aspect, " flags=0x", std::hex, static_cast<int>(flags),
            " selector=0x", static_cast<int>(selector), std::dec,
            " outputFOV(before)=", outputFov, " outputAspect(before)=", outputAspect, ".");
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
        Log("Combined output PRE: source=0x", std::hex, source, " output=0x", output, std::dec,
            " outputFov=", outputFov, " outputAspect=", outputAspect,
            " output+0x40=0x", std::hex, outputField40,
            " output+0x58=0x", outputField58, " output+0x68=0x", outputField68, std::dec, ".");
#endif
        return true;
    }

#ifdef GAMEPLAY_ONE_SHOT_CINEMATIC_TRIGGER
    bool WriteFlagsOnly(std::uintptr_t source, std::uint8_t flags)
    {
        if (!IsWritable(source + kFlagsOffset, sizeof(flags))) return false;
        std::memcpy(reinterpret_cast<void*>(source + kFlagsOffset), &flags, sizeof(flags));
        return true;
    }

    DWORD WINAPI OneShotTriggerLoop(void*)
    {
        bool previousF7 = false;
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
        bool previousF6 = false;
        bool previousF8 = false;
        bool previousF9 = false;
#endif
        while (true) {
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
            const bool f6 = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
#endif
            const bool f7 = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
            const bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
            const bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
            if (f6 && !previousF6)
                Log("Combined diagnostic marker: native-wrong cinematic state.");
#endif
            if (f7 && !previousF7) {
                if (g_readyForOneShot.load(std::memory_order_acquire)) {
                    g_oneShotCinematicTriggerArmed.store(true, std::memory_order_release);
                    Log("Diagnostic one-shot cinematic trigger armed from validated READY state.");
                } else {
                    Log("Diagnostic F7 refused: validated 32:9/0x5 Waiting state is not ready.");
                }
            }
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
            if (f8 && !previousF8) {
                const auto source = g_lastCameraSource.load(std::memory_order_acquire);
                float currentAspect = 0.0f;
                DEVMODE display{ .dmSize = sizeof(DEVMODE) };
                const bool displayReady = EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &display) && display.dmPelsHeight != 0;
                const float displayAspect = displayReady
                    ? static_cast<float>(display.dmPelsWidth) / static_cast<float>(display.dmPelsHeight) : 0.0f;
                const bool applied = source && displayReady &&
                    g_state.load(std::memory_order_acquire) == ReplayState::Complete &&
                    SafeRead(source + kAspectOffset, currentAspect) &&
                    std::fabs(currentAspect - kNativeAspect) <= 0.001f &&
                    IsWritable(source + kAspectOffset, sizeof(displayAspect));
                if (applied) {
                    std::memcpy(reinterpret_cast<void*>(source + kAspectOffset), &displayAspect, sizeof(displayAspect));
                    g_suppressRearmAfterManualRestore.store(true, std::memory_order_release);
                    Log("Combined diagnostic F8: native aspect restore source=0x", std::hex, source,
                        std::dec, " aspect=", currentAspect, " -> ", displayAspect,
                        " flags preserved.");
                } else {
                    Log("Combined diagnostic F8 refused: Complete + constrained 16:9 state was not confirmed.");
                }
            }
            if (f9 && !previousF9)
                Log("Combined diagnostic marker: observation window.");
            previousF6 = f6;
#endif
            previousF7 = f7;
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
            previousF8 = f8;
            previousF9 = f9;
#endif
            Sleep(10);
        }
    }
#endif

    void LogTransitionSnapshot(const char* stage, std::uint64_t sequence, std::uintptr_t source)
    {
        float aspect = 0.0f;
        std::uint8_t flags = 0;
        if (!SafeRead(source + kAspectOffset, aspect) || !SafeRead(source + kFlagsOffset, flags)) {
            Log("Gameplay transition seq=", sequence, " stage=", stage,
                " source=0x", std::hex, source, std::dec, " readable=false");
            return;
        }

        Log("Gameplay transition seq=", sequence, " stage=", stage,
            " source=0x", std::hex, source, std::dec,
            " aspect=", aspect, " flags=0x", std::hex, static_cast<int>(flags), std::dec,
            " transitionState=", ReplayStateName(g_state.load(std::memory_order_relaxed)), ".");
    }

#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
    BOOL CALLBACK FindCurrentProcessWindow(HWND window, LPARAM parameter)
    {
        DWORD processId = 0;
        GetWindowThreadProcessId(window, &processId);
        if (processId == GetCurrentProcessId() && IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr) {
            *reinterpret_cast<HWND*>(parameter) = window;
            return FALSE;
        }
        return TRUE;
    }

    void LogResolutionChange()
    {
        DEVMODE display{ .dmSize = sizeof(DEVMODE) };
        const bool displayReady = EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &display) != FALSE;
        HWND window = nullptr;
        EnumWindows(FindCurrentProcessWindow, reinterpret_cast<LPARAM>(&window));
        RECT windowRect{};
        RECT clientRect{};
        const bool windowReady = window && GetWindowRect(window, &windowRect) && GetClientRect(window, &clientRect);
        ResolutionSnapshot current{
            displayReady ? display.dmPelsWidth : 0,
            displayReady ? display.dmPelsHeight : 0,
            windowReady ? windowRect.right - windowRect.left : 0,
            windowReady ? windowRect.bottom - windowRect.top : 0,
            windowReady ? clientRect.right - clientRect.left : 0,
            windowReady ? clientRect.bottom - clientRect.top : 0,
            displayReady || windowReady };
        if (!current.valid || (g_lastResolutionSnapshot.valid &&
            current.displayWidth == g_lastResolutionSnapshot.displayWidth &&
            current.displayHeight == g_lastResolutionSnapshot.displayHeight &&
            current.windowWidth == g_lastResolutionSnapshot.windowWidth &&
            current.windowHeight == g_lastResolutionSnapshot.windowHeight &&
            current.clientWidth == g_lastResolutionSnapshot.clientWidth &&
            current.clientHeight == g_lastResolutionSnapshot.clientHeight))
            return;

        g_lastResolutionSnapshot = current;
        Log("Resolution observation: display=", current.displayWidth, "x", current.displayHeight,
            " window=", current.windowWidth, "x", current.windowHeight,
            " client=", current.clientWidth, "x", current.clientHeight,
            " source=Win32 display/window bounds; engine render resolution not established.");
    }

    DWORD WINAPI ResolutionMonitorLoop(void*)
    {
        while (true) {
            LogResolutionChange();
            Sleep(250);
        }
    }

    void LogCombinedOutputPost(std::uint64_t sequence, std::uintptr_t source, std::uintptr_t output)
    {
        float outputFov = 0.0f;
        float outputAspect = 0.0f;
        std::uint64_t outputField40 = 0;
        std::uint64_t outputField58 = 0;
        std::uint64_t outputField68 = 0;
        if (!SafeRead(output + 0x30, outputFov) || !SafeRead(output + 0x5C, outputAspect) ||
            !SafeRead(output + 0x40, outputField40) || !SafeRead(output + 0x58, outputField58) ||
            !SafeRead(output + 0x68, outputField68)) {
            Log("Combined output POST seq=", sequence, " read-refused.");
            return;
        }
        Log("Combined output POST seq=", sequence, ": source=0x", std::hex, source,
            " output=0x", output, std::dec, " outputFov=", outputFov,
            " outputAspect=", outputAspect, " output+0x40=0x", std::hex, outputField40,
            " output+0x58=0x", outputField58, " output+0x68=0x", outputField68, std::dec, ".");
    }
#endif

    void LogAbcSnapshot(const char* marker, SafetyHookContext& context, std::uintptr_t source,
        float sourceFov, float sourceAspect, std::uint8_t sourceFlags)
    {
        float outputFov = 0.0f;
        float outputAspect = 0.0f;
        std::uint64_t outputMode40 = 0;
        std::uint64_t outputMode58 = 0;
        std::uint64_t outputMode68 = 0;
        const auto output = context.rbx;
        g_lastAbcOutput.store(output, std::memory_order_release);
        const int stage = std::strcmp(marker, "A_RECOVERY_COMPLETE") == 0 ? 1 :
            std::strcmp(marker, "B_CONSTRAIN_POST") == 0 ? 2 :
            std::strcmp(marker, "C_RESTORE_POST") == 0 ? 3 : 0;
        if (stage != 0) g_pendingAbcStage.store(stage, std::memory_order_release);
        const bool outputReadable = SafeRead(output + 0x30, outputFov) &&
            SafeRead(output + 0x5C, outputAspect) &&
            SafeRead(output + 0x40, outputMode40) &&
            SafeRead(output + 0x58, outputMode58) &&
            SafeRead(output + 0x68, outputMode68);
        Log("ABC ", marker, " source=0x", std::hex, source,
            " output=0x", output, std::dec,
            " sourceFov=", sourceFov, " sourceAspect=", sourceAspect,
            " sourceFlags=0x", std::hex, static_cast<int>(sourceFlags), std::dec,
            " outputReadable=", outputReadable,
            " outputFov=", outputFov, " outputAspect=", outputAspect,
            " outputMode40=0x", std::hex, outputMode40,
            " outputMode58=0x", outputMode58, " outputMode68=0x", outputMode68,
            std::dec, " sameObject=", source == output, ".");
    }

    void TraceDownstreamRecalc(SafetyHookContext& context)
    {
        const auto expectedOutput = g_lastAbcOutput.load(std::memory_order_acquire);
        const auto stage = g_pendingAbcStage.exchange(0, std::memory_order_acq_rel);
        if (stage == 0 || expectedOutput == 0 || context.rcx != expectedOutput) return;

        float outputFov = 0.0f;
        float outputScale = 0.0f;
        float derived9d0 = 0.0f;
        float derived9d4 = 0.0f;
        float derived9d8 = 0.0f;
        const auto output = static_cast<std::uintptr_t>(context.rcx);
        const bool readable = SafeRead(output + 0x30, outputFov) &&
            SafeRead(output + 0x40, outputScale) &&
            SafeRead(output + 0x9D0, derived9d0) &&
            SafeRead(output + 0x9D4, derived9d4) &&
            SafeRead(output + 0x9D8, derived9d8);
        Log("DOWNSTREAM stage=", stage, " rva=0x", std::hex, kDownstreamRecalcRva,
            std::dec, " output=0x", std::hex, output, std::dec,
            " xmm1=", context.xmm1.f32[0], " r8=0x", std::hex,
            static_cast<std::uint64_t>(context.r8) & 0xFF, " r9=0x",
            static_cast<std::uint64_t>(context.r9) & 0xFF, std::dec,
            " readable=", readable, " outputFov=", outputFov,
            " outputScale=", outputScale, " output9D0=", derived9d0,
            " output9D4=", derived9d4, " output9D8=", derived9d8, ".");
    }

    void ReplayManualTransitionOriginal(SafetyHookContext& context)
    {
        const auto source = context.rsi;
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
        g_lastCameraSource.store(source, std::memory_order_release);
#endif
        const float fov = context.xmm0.f32[0];
        float aspect = 0.0f;
        std::uint8_t flags = 0;
        if (!SafeRead(source + kAspectOffset, aspect) || !SafeRead(source + kFlagsOffset, flags)) return;
#ifdef GAMEPLAY_ONE_SHOT_CINEMATIC_TRIGGER
        if (g_oneShotCinematicTriggerArmed.load(std::memory_order_acquire) &&
            g_state.load(std::memory_order_relaxed) == ReplayState::WaitingForAutomaticUpdate &&
            std::fabs(aspect - kWideAspect) <= 0.001f && flags == 0x5) {
            if (WriteFlagsOnly(source, 0x4)) {
                g_oneShotCinematicTriggerArmed.store(false, std::memory_order_release);
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
                g_readyForOneShot.store(false, std::memory_order_release);
#endif
                flags = 0x4;
                Log("Diagnostic one-shot cinematic trigger consumed at camera-writer boundary: source=0x",
                    std::hex, source, std::dec, " aspect=", aspect, " flags=0x5 -> 0x4.");
            } else {
                Log("Diagnostic one-shot cinematic trigger refused: flags field was not writable.");
            }
        }
#endif
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
        const auto currentState = g_state.load(std::memory_order_acquire);
        const bool ready = currentState == ReplayState::WaitingForAutomaticUpdate &&
            std::fabs(aspect - kWideAspect) <= 0.001f && flags == 0x5;
        g_readyForOneShot.store(ready, std::memory_order_release);
        if (ready && !g_readyMarkerLogged.exchange(true, std::memory_order_acq_rel))
            Log("Combined diagnostic READY FOR F7: source=0x", std::hex, source,
                std::dec, " aspect=", aspect, " flags=0x5 transitionState=WaitingForAutomaticUpdate.");
        if (!ready)
            g_readyMarkerLogged.store(false, std::memory_order_release);
#endif
        const bool modeChanged = LogCameraModeChange(source, context.rbx, fov, aspect, flags);
        const auto traceSequence = modeChanged
            ? g_transitionTraceSequence.fetch_add(1, std::memory_order_relaxed) + 1
            : 0;
        if (modeChanged) LogTransitionSnapshot("PRE", traceSequence, source);
        const auto logPost = [&]() {
            if (modeChanged) LogTransitionSnapshot("POST", traceSequence, source);
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
            if (modeChanged && g_logCombinedOutputForInvocation)
                LogCombinedOutputPost(traceSequence, source, context.rbx);
#endif
        };

        if (std::fabs(aspect - kCinemaAspect) <= 0.001f && flags == 0x5) {
            // Preserve constrained aspect ratio for a real 21:9 output. The FOV source
            // is already correct; only the camera aspect state needs normalization.
            if (WriteAspectAndFlags(source, kNativeAspect, 0x5))
                Log("Normalized 21:9 camera aspect while preserving constrained aspect ratio.");
            logPost();
            return;
        }

        auto state = g_state.load(std::memory_order_relaxed);
        if (state == ReplayState::Complete && std::fabs(aspect - kWideAspect) <= 0.001f && flags == 0x4) {
            // Cutscenes can rebuild the gameplay camera and restore the same broken
            // Auto state seen during startup. Arm the proven two-pass transition again.
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
            if (g_suppressRearmAfterManualRestore.exchange(false, std::memory_order_acq_rel)) {
                Log("Combined diagnostic: manual native-aspect restore observed; automatic re-arm suppressed.");
            } else {
#endif
                g_state.store(ReplayState::WaitingForAutomaticUpdate, std::memory_order_relaxed);
                state = ReplayState::WaitingForAutomaticUpdate;
                Log("Gameplay camera was rebuilt; re-arming aspect transition.");
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
            }
#endif
        }

        if (state == ReplayState::WaitingForAutomaticUpdate) {
            // The game has performed its own startup update. Recreate the observed
            // temporary constrained pass that occurs when the user applies 16:9.
            if (std::fabs(aspect - kWideAspect) > 0.001f || flags != 0x4) {
                logPost();
                return;
            }
            LogAbcSnapshot("B_CONSTRAIN_PRE", context, source, fov, aspect, flags);
            if (WriteAspectAndFlags(source, kWideAspect, 0x5)) {
                g_state.store(ReplayState::AppliedConstrainPass, std::memory_order_relaxed);
                LogAbcSnapshot("B_CONSTRAIN_POST", context, source, fov, kWideAspect, 0x5);
            } else {
                Log("Replay refused: constrained-pass fields were not writable.");
            }
            logPost();
            return;
        }

        if (state == ReplayState::AppliedConstrainPass) {
            // On the following camera update, restore Auto's native aspect and flags.
            LogAbcSnapshot("C_RESTORE_PRE", context, source, fov, aspect, flags);
            if (WriteAspectAndFlags(source, kNativeAspect, 0x4)) {
                g_state.store(ReplayState::Complete, std::memory_order_relaxed);
                LogAbcSnapshot("C_RESTORE_POST", context, source, fov, kNativeAspect, 0x4);
            } else {
                Log("Replay refused: Auto-restore fields were not writable.");
            }
        }
        logPost();
    }

    float CinematicHorPlus(float fov, float aspect)
    {
        constexpr float pi = 3.14159265358979323846f;
        const float half = fov * (pi / 360.0f);
        return 2.0f * std::atan(std::tan(half) * (aspect / kNativeAspect)) * (180.0f / pi);
    }

    bool ReadInstallAspect(float& aspect, LONG& width, LONG& height, const char*& sourceKind)
    {
        HWND window = nullptr;
        EnumWindows([](HWND candidate, LPARAM parameter) {
            DWORD processId{};
            GetWindowThreadProcessId(candidate, &processId);
            if (processId == GetCurrentProcessId() && IsWindowVisible(candidate) &&
                GetWindow(candidate, GW_OWNER) == nullptr) {
                *reinterpret_cast<HWND*>(parameter) = candidate;
                return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&window));
        RECT rect{};
        if (window && GetClientRect(window, &rect)) {
            width = rect.right - rect.left;
            height = rect.bottom - rect.top;
            if (width > 0 && height > 0) {
                sourceKind = "client";
                aspect = static_cast<float>(width) / static_cast<float>(height);
                return true;
            }
        }
        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);
        sourceKind = "desktop";
        if (width <= 0 || height <= 0) return false;
        aspect = static_cast<float>(width) / static_cast<float>(height);
        return true;
    }

    bool IsExecutable(std::uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info))) return false;
        const auto protection = info.Protect & 0xFF;
        return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
            protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    }

    bool InstallCinematicAspect(float aspect)
    {
        g_cinematicAspectStore = reinterpret_cast<std::uint8_t*>(
            reinterpret_cast<std::uintptr_t>(g_executable) + kCinematicAspectStoreRva);
        if (!IsExecutable(reinterpret_cast<std::uintptr_t>(g_cinematicAspectStore)) ||
            std::memcmp(g_cinematicAspectStore, kCinematicStorePrefix, sizeof(kCinematicStorePrefix)) != 0 ||
            std::memcmp(g_cinematicAspectStore + kCinematicImmediateOffset,
                kCinematicOriginalImmediate, sizeof(kCinematicOriginalImmediate)) != 0) return false;
        std::memcpy(g_cinematicOriginalImmediate, g_cinematicAspectStore + kCinematicImmediateOffset,
            sizeof(g_cinematicOriginalImmediate));
        std::uint32_t bits{};
        std::memcpy(&bits, &aspect, sizeof(bits));
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(g_cinematicAspectStore, &info, sizeof(info))) return false;
        DWORD oldProtection{};
        if (!VirtualProtect(info.BaseAddress, info.RegionSize, PAGE_EXECUTE_READWRITE, &oldProtection)) return false;
        std::memcpy(g_cinematicAspectStore + kCinematicImmediateOffset, &bits, sizeof(bits));
        FlushInstructionCache(GetCurrentProcess(), g_cinematicAspectStore, kCinematicImmediateOffset + sizeof(bits));
        DWORD ignored{};
        VirtualProtect(info.BaseAddress, info.RegionSize, oldProtection, &ignored);
        g_cinematicAspectPatched = true;
        return true;
    }

    void RestoreCinematicAspect()
    {
        if (!g_cinematicAspectPatched || !g_cinematicAspectStore) return;
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(g_cinematicAspectStore, &info, sizeof(info))) return;
        DWORD oldProtection{};
        if (!VirtualProtect(info.BaseAddress, info.RegionSize, PAGE_EXECUTE_READWRITE, &oldProtection)) return;
        std::memcpy(g_cinematicAspectStore + kCinematicImmediateOffset, g_cinematicOriginalImmediate,
            sizeof(g_cinematicOriginalImmediate));
        FlushInstructionCache(GetCurrentProcess(), g_cinematicAspectStore,
            kCinematicImmediateOffset + sizeof(g_cinematicOriginalImmediate));
        DWORD ignored{};
        VirtualProtect(info.BaseAddress, info.RegionSize, oldProtection, &ignored);
        g_cinematicAspectPatched = false;
    }

    void TraceCinematicEnter(SafetyHookContext& context)
    {
        bool expected = false;
        if (!g_cinematicFovApplied.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
        const float before = context.xmm0.f32[0];
        LONG width{}, height{};
        const char* sourceKind = "unknown";
        float aspect{};
        ReadInstallAspect(aspect, width, height, sourceKind);
        const float after = std::isfinite(before) && std::isfinite(aspect) &&
            before > 1.0f && before < 179.0f && aspect > 1.0f
            ? CinematicHorPlus(before, aspect) : before;
        if (std::isfinite(after) && after > 1.0f && after < 179.0f) context.xmm0.f32[0] = after;
        g_coordinator.store(CoordinatorState::CinematicActive, std::memory_order_release);
        Log("Global cinematic ENTER: aspect=", aspect, " authoredFov=", before,
            " transformedFov=", context.xmm0.f32[0], " source=", sourceKind,
            " size=", width, "x", height, ". Gameplay replay suppressed.");
    }

    void TraceCinematicExit(SafetyHookContext& context)
    {
        g_exitTargetFov.store(context.xmm0.f32[0], std::memory_order_release);
        g_cinematicFovApplied.store(false, std::memory_order_release);
        g_coordinator.store(CoordinatorState::CinematicExiting, std::memory_order_release);
        Log("Global cinematic EXIT: nativeTargetFov=", context.xmm0.f32[0],
            ". Gameplay replay suppressed until native recovery.");
    }

    void ReplayManualTransition(SafetyHookContext& context)
    {
        const auto state = g_coordinator.load(std::memory_order_acquire);
        if (state == CoordinatorState::CinematicActive) {
            return;
        }
        if (state == CoordinatorState::CinematicExiting) {
            const auto source = static_cast<std::uintptr_t>(context.rsi);
            std::uint8_t flags{};
            SafeRead(source + kFlagsOffset, flags);
            const float currentFov = context.xmm0.f32[0];
            const float targetFov = g_exitTargetFov.load(std::memory_order_acquire);
            const float delta = std::fabs(currentFov - targetFov);
            if (flags == 0x04 && std::isfinite(currentFov) && std::isfinite(targetFov) && delta <= kRecoveryEpsilon) {
                g_coordinator.store(CoordinatorState::Gameplay, std::memory_order_release);
                float aspect = 0.0f;
                std::uint8_t sourceFlags = 0;
                if (SafeRead(source + kAspectOffset, aspect) && SafeRead(source + kFlagsOffset, sourceFlags))
                    LogAbcSnapshot("A_RECOVERY_COMPLETE", context, source, currentFov, aspect, sourceFlags);
            }
            return;
        }
        ReplayManualTransitionOriginal(context);
    }

    bool VerifyExecutableAndInstruction()
    {
        const auto* base = reinterpret_cast<const std::uint8_t*>(g_executable);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

        const auto* section = IMAGE_FIRST_SECTION(nt);
        const std::uint8_t* textStart = nullptr;
        std::size_t textSize = 0;
        for (std::uint16_t i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
            if (std::memcmp(section->Name, ".text", 5) == 0) {
                textStart = base + section->VirtualAddress;
                textSize = section->Misc.VirtualSize;
                break;
            }
        }
        if (!textStart || textSize < sizeof(kCameraWriterSignature)) return false;

        std::uint8_t* match = nullptr;
        std::size_t matches = 0;
        for (std::size_t offset = 0; offset <= textSize - sizeof(kCameraWriterSignature); ++offset) {
            const auto* candidate = textStart + offset;
            bool matchesSignature = true;
            for (std::size_t i = 0; i < sizeof(kCameraWriterSignature); ++i) {
                // The 32-bit relative target of JNZ may move when surrounding code changes.
                if ((i < 17 || i >= 21) && candidate[i] != kCameraWriterSignature[i]) {
                    matchesSignature = false;
                    break;
                }
            }
            if (matchesSignature) {
                match = const_cast<std::uint8_t*>(candidate);
                ++matches;
            }
        }
        if (matches != 1) {
            Log("Camera-writer signature rejected: matches=", matches, ".");
            return false;
        }

        ZydisDecoder decoder{};
        ZydisDecodedInstruction instruction{};
        // DecodeFull clears every possible operand slot, not just visible operands.
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
        auto* target = match + kFovWriteOffsetInSignature;
        const bool verified = ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)) &&
            ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, target, 15, &instruction, operands)) &&
            instruction.mnemonic == ZYDIS_MNEMONIC_MOVSS && instruction.operand_count_visible >= 2 &&
            operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER &&
            operands[0].mem.base == ZYDIS_REGISTER_RBX && operands[0].mem.disp.has_displacement &&
            operands[0].mem.disp.value == 0x30 && operands[1].reg.value == ZYDIS_REGISTER_XMM0;
        if (verified) {
            g_fovWriteAddress = target;
            Log("Camera-writer signature validated at RVA=0x", std::hex,
                reinterpret_cast<std::uintptr_t>(target) - reinterpret_cast<std::uintptr_t>(base), std::dec, ".");
        }
        return verified;
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto logPath = std::filesystem::path(modulePath).remove_filename() / "STALKER2CinematicPostExitDownstreamRecalcCorrelation204.log";
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CinematicPostExitDownstreamRecalcCorrelation204", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::info);
        } catch (...) { return 0; }

        try {
            Log("Gameplay aspect fix loaded. FOV is preserved from the game's settings.");
            LONG cinematicWidth{}, cinematicHeight{};
            const char* cinematicSource = "unknown";
            float cinematicAspect{};
            if (!ReadInstallAspect(cinematicAspect, cinematicWidth, cinematicHeight, cinematicSource) ||
                !std::isfinite(cinematicAspect) || cinematicAspect < 1.0f || cinematicAspect > 8.0f ||
                !InstallCinematicAspect(cinematicAspect))
                throw std::runtime_error("cinematic aspect immediate validation/patch failed");
            Log("Global cinematic aspect immediate patched: aspect=", cinematicAspect,
                " source=", cinematicSource, " size=", cinematicWidth, "x", cinematicHeight, ".");
            auto* cinematicEnter = reinterpret_cast<std::uint8_t*>(
                reinterpret_cast<std::uintptr_t>(g_executable) + kCinematicEnterConsumerRva);
            auto* cinematicExit = reinterpret_cast<std::uint8_t*>(
                reinterpret_cast<std::uintptr_t>(g_executable) + kCinematicExitConsumerRva);
            if (!IsExecutable(reinterpret_cast<std::uintptr_t>(cinematicEnter)) ||
                !IsExecutable(reinterpret_cast<std::uintptr_t>(cinematicExit)) ||
                std::memcmp(cinematicEnter, kCinematicEnterConsumerBytes, sizeof(kCinematicEnterConsumerBytes)) != 0 ||
                std::memcmp(cinematicExit, kCinematicExitConsumerBytes, sizeof(kCinematicExitConsumerBytes)) != 0)
                throw std::runtime_error("cinematic live boundary bytes mismatch");
            g_cinematicEnterHook = safetyhook::create_mid(cinematicEnter, TraceCinematicEnter);
            g_cinematicExitHook = safetyhook::create_mid(cinematicExit, TraceCinematicExit);
            if (!g_cinematicEnterHook || !g_cinematicExitHook)
                throw std::runtime_error("cinematic coordinator hook creation failed");
            auto* downstreamRecalc = reinterpret_cast<std::uint8_t*>(
                reinterpret_cast<std::uintptr_t>(g_executable) + kDownstreamRecalcRva);
            if (!IsExecutable(reinterpret_cast<std::uintptr_t>(downstreamRecalc)) ||
                std::memcmp(downstreamRecalc, kDownstreamRecalcPrologue,
                    sizeof(kDownstreamRecalcPrologue)) != 0)
                throw std::runtime_error("downstream recalculation prologue mismatch");
            g_downstreamRecalcHook = safetyhook::create_mid(downstreamRecalc, TraceDownstreamRecalc);
            if (!g_downstreamRecalcHook)
                throw std::runtime_error("downstream recalculation hook creation failed");
            Log("Downstream candidate validated: RVA=0x", std::hex, kDownstreamRecalcRva,
                std::dec, "; read-only correlation enabled.");
            if (!VerifyExecutableAndInstruction()) {
                throw std::runtime_error("camera-writer signature or validated FOV instruction did not match");
            }
            Log("Installing validated gameplay hook.");
            g_hook = safetyhook::create_mid(g_fovWriteAddress, ReplayManualTransition);
            if (!g_hook) throw std::runtime_error("validated gameplay hook creation failed");
            Log("Validated gameplay hook installed: true.");
#ifdef GAMEPLAY_ONE_SHOT_CINEMATIC_TRIGGER
            const auto triggerThread = CreateThread(nullptr, 0, OneShotTriggerLoop, nullptr, 0, nullptr);
            if (!triggerThread) throw std::runtime_error("one-shot trigger thread could not start");
            CloseHandle(triggerThread);
            Log("Diagnostic one-shot trigger enabled: press F7 to arm; no timer or repeated flag write is used.");
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
            const auto resolutionThread = CreateThread(nullptr, 0, ResolutionMonitorLoop, nullptr, 0, nullptr);
            if (!resolutionThread) throw std::runtime_error("resolution monitor thread could not start");
            CloseHandle(resolutionThread);
            Log("Combined diagnostic resolution monitor enabled: display/window/client changes are logged automatically.");
#endif
#endif
        } catch (const std::exception& exception) {
            g_downstreamRecalcHook.reset();
            g_cinematicExitHook.reset();
            g_cinematicEnterHook.reset();
            RestoreCinematicAspect();
            Log("Gameplay hook setup failed safely: ", exception.what());
        } catch (...) {
            Log("Gameplay hook setup failed safely with an unknown exception.");
        }
        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH) {
        g_downstreamRecalcHook.reset();
        RestoreCinematicAspect();
        return TRUE;
    }
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    g_module = module;
    DisableThreadLibraryCalls(module);
    const auto thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread);
    return TRUE;
}
