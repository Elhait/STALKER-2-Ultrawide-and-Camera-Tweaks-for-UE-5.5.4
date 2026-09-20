#include "runtime.hpp"
#include "../config/feature_config.hpp"
#include "../config/config_repository.hpp"
#include "../config/config_template.hpp"
#include "../hooks/signatures/signature_definitions.hpp"
#include "../hooks/signature_scanner.hpp"
#include "../hooks/instruction_validator.hpp"
#include "../hooks/hook_set.hpp"
#include "../gameplay/gameplay_state.hpp"
#include "../gameplay/gameplay_camera.hpp"
#include "../gameplay/horplus_gameplay.hpp"
#include "../gameplay/aspect_policy.hpp"
#include "../cinematics/cinematic_initialization.hpp"
#include "../cinematics/cinematic_fov.hpp"
#include "../cinematics/cinematic_selection.hpp"
#include "../diagnostics/matchgameplay_prediction.hpp"
#include "../cinematics/cinematic_aspect.hpp"
#include "../dialogue/dialogue_fov.hpp"
#include "../dialogue/dialogue_state.hpp"
#include "../diagnostics/diagnostic_runtime.hpp"
#include "../camera/fov_observation.hpp"
#include "../camera/gameplay_baseline.hpp"
#include "../camera/gameplay_aspect_restoration.hpp"
#include "../camera/horplus.hpp"
#include "../camera/presentation_state.hpp"
#ifdef CAMERA_STATE_SNAPSHOT_DIAGNOSTIC
#include "../camera/camera_state_snapshot.hpp"
#endif
#include "../platform/win32/memory.hpp"
#include "../platform/win32/sha256.hpp"
#include "../platform/win32/viewport.hpp"
#include "../platform/win32/window.hpp"
#include <safetyhook.hpp>
#include <Zydis.h>
#include <bcrypt.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <array>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

#if defined(POST_EXIT_RAW_TRACE_DIAGNOSTIC) || \
    defined(POST_EXIT_INTERPOLATED_FOV_PRODUCER_TRACE) || \
    defined(POST_EXIT_FOV_PRODUCER_STORE_TRACE) || \
    defined(POST_EXIT_CAMERA_FOV_WRITE_OWNER_TRACE) || \
    defined(POST_EXIT_FOV_STATE_CONSUMER_TRACE) || \
    defined(POST_EXIT_GAMEPLAY_OBSERVER_DIAGNOSTIC) || \
    defined(NATIVE_GAMEPLAY_BASELINE_DIAGNOSTIC) || \
    defined(POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_TEST)
#define POST_EXIT_TRACE_STATE
#endif

namespace
{
    using plugin::FeatureStatus;
    using plugin::FeatureStatusName;
    using config::CinematicAspectPolicy;
    using config::DialogueZoomPolicy;
    using config::FeatureConfig;
    using config::CinematicAspectPolicyName;
    using config::DialogueZoomPolicyName;
    using config::HotkeyName;
    using config::NextCinematicAspectPolicy;
    using config::NextDialogueZoomPolicy;
    using config::ParseBool;
    using config::ParseCinematicAspectPolicy;
    using config::ParseDialogueZoomPolicy;
    using config::ParseHotkey;
    using config::Trim;
    using namespace hooks::signatures;
    using camera::CoordinatorState;
    using camera::CoordinatorStateName;
    using gameplay::ReplayState;
    using gameplay::ReplayStateName;
    using dialogue::Phase;
    using dialogue::PhaseName;
    using DialoguePhase = dialogue::Phase;
    constexpr auto DialoguePhaseName = dialogue::PhaseName;

    // Global integration copy; stable gameplay source remains untouched.
    constexpr std::uintptr_t kAspectOffset = 0x254;
    constexpr std::uintptr_t kFlagsOffset = 0x259;
    constexpr float kWideAspect = 32.0f / 9.0f;
    // Canonical PC 21:9 framing used by the validated 3440x1440 profile.
    constexpr float kCinemaAspect = 3440.0f / 1440.0f;
    constexpr float kNativeAspect = 16.0f / 9.0f;
    constexpr std::uint8_t kCinematicStorePrefix[] = { 0xC7, 0x80, 0x54, 0x02, 0x00, 0x00 };
    constexpr std::uint8_t kCinematicOriginalImmediate[] = { 0x39, 0x8E, 0xE3, 0x3F };
    constexpr std::size_t kCinematicImmediateOffset = 6;
    constexpr std::size_t kCinematicStoreInstructionLength = 10;
    constexpr float kRecoveryEpsilon = 0.01f;
    constexpr std::size_t kDialogueBoundaryHookOffset = 9;
    constexpr float kDialogueRecoveryEpsilon = 1.0f;
    constexpr float kDialogueTransformEpsilon = 0.01f;
    constexpr std::uint32_t kDialogueRecoveryStableSamples = 2;
    constexpr std::uint32_t kDialogueCandidateStableSamples = 3;
    constexpr float kDialogueContextFovJump = 5.0f;


    // Runtime ownership boundary. RuntimeState is intentionally private to
    // this translation unit; the aliases below preserve established
    // callback/control-flow names while keeping mutable production state under
    // one explicit lifetime owner.
    struct RuntimeState
    {
        HMODULE module{};
        HMODULE executable = GetModuleHandle(nullptr);
        plugin::WorkerLifecycle workers;
        std::uint8_t* fovWriteAddress{};
        hooks::HookSet hooks;
        std::shared_ptr<spdlog::logger> logger;
        config::FeatureConfig config{};
        std::atomic<bool> gameplayAvailable{false};
        std::atomic<config::GameplayMode> runtimeGameplayMode{
            config::GameplayMode::HorPlus};
        std::atomic<bool> gameplayModeTransitionPending{};
        std::atomic<bool> gameplayModeTransitionDeferralLogged{};
        std::atomic<config::GameplayMode> gameplayModeTransitionTarget{
            config::GameplayMode::HorPlus};
        std::atomic<config::CinematicAspectPolicy> runtimeCinematicPolicy{
            config::CinematicAspectPolicy::Auto};
        std::atomic<config::CinematicFovMode> runtimeCinematicFovMode{
            config::CinematicFovMode::GameplayHorPlus};
        std::atomic<config::CinematicAspectPolicy> activeCinematicAspectPolicy{
            config::CinematicAspectPolicy::Auto};
        std::atomic<config::CinematicFovMode> activeCinematicFovMode{
            config::CinematicFovMode::GameplayHorPlus};
        std::atomic<std::uint64_t> cinematicSelectionGeneration{};
        std::atomic<bool> cinematicSelectionValid{};
        std::atomic<bool> gameplayHookGate{};
        std::atomic<bool> cinematicHookGate{};
        std::atomic<bool> stopping{};
        std::atomic<config::DialogueZoomPolicy> runtimeDialoguePolicy{
            config::DialogueZoomPolicy::Native};
        std::atomic<bool> cinematicLifecycleObservationAvailable{};
        std::atomic<bool> dialogueNonNativeCapabilityAvailable{};
        std::filesystem::path configPath;
        std::atomic<ReplayState> state{ReplayState::WaitingForAutomaticUpdate};
        std::atomic<std::uint32_t> lastCameraMode{};
        std::atomic<std::uint64_t> transitionTraceSequence{0};
        std::atomic<CoordinatorState> coordinator{CoordinatorState::Gameplay};
        camera::CameraFovObservationStore fovObservationStore{};
        camera::GameplayBaselineStore gameplayBaselineStore{};
        camera::GameplayAspectRestorationStore gameplayAspectRestorationStore{};
        std::atomic<bool> cinematicFovApplied{false};
        std::atomic<float> cinematicTransformedFov{std::numeric_limits<float>::quiet_NaN()};
        std::atomic<float> matchGameplayEnterNativeFov{std::numeric_limits<float>::quiet_NaN()};
        std::atomic<float> matchGameplayPreEnterNativeFov{std::numeric_limits<float>::quiet_NaN()};
        std::atomic<float> matchGameplayPreEnterHorPlusFov{std::numeric_limits<float>::quiet_NaN()};
        std::atomic<float> matchGameplayPreEnterAspect{std::numeric_limits<float>::quiet_NaN()};
        std::atomic<bool> matchGameplayPreEnterPairValid{};
#ifdef HORPLUS_CINEMATIC_WRITER_TRACE_DIAGNOSTIC
        std::atomic<float> cinematicEnterAspect{std::numeric_limits<float>::quiet_NaN()};
#endif
        std::atomic<float> exitTargetFov{0.0f};
        dialogue::PostCinematicRecoveryExclusion postCinematicDialogueExclusion{};
#ifdef POST_EXIT_TRACE_STATE
        std::atomic<bool> postExitTraceArmed{false};
        std::atomic<std::uint32_t> postExitTraceWriterCount{0};
        std::atomic<std::int64_t> postExitTraceStartNs{0};
        std::atomic<std::uint64_t> postExitTraceSequence{0};
#endif
#ifdef POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_TEST
        std::atomic<bool> deferredGameplayReplayPending{false};
        std::atomic<std::uintptr_t> deferredGameplaySource{};
        std::atomic<std::uint32_t> deferredGameplayStableSamples{0};
        std::atomic<float> deferredGameplayLastFov{std::numeric_limits<float>::quiet_NaN()};
        std::atomic<std::int64_t> deferredGameplayStableSinceNs{0};
#endif
        std::atomic<bool> atomicExitHandoffPending{false};
        std::atomic<std::uintptr_t> atomicExitHandoffSource{};
        std::atomic<float> atomicExitHandoffPreviousFov{std::numeric_limits<float>::quiet_NaN()};
#ifdef POST_EXIT_DELAYED_ASPECT_DIAGNOSTIC
        std::atomic<bool> delayedAspectDiagnosticApplied{false};
#endif
#ifdef HORPLUS_GAMEPLAY_DIAGNOSTIC
        std::atomic<std::uint64_t> horPlusDiagnosticWriterCallbacks{0};
        std::atomic<std::uint64_t> horPlusDiagnosticApplications{0};
        std::atomic<std::uint64_t> horPlusDiagnosticChanges{0};
#endif
#ifdef DIALOGUE_DISCOVERY_DIAGNOSTIC
        std::atomic<bool> dialogueDiscoveryActive{false};
        std::atomic<std::int64_t> dialogueDiscoveryStartNs{0};
        std::atomic<std::uint64_t> dialogueDiscoverySamples{0};
        std::atomic<std::uint64_t> dialogueDiscoveryChanges{0};
        std::atomic<std::uint64_t> dialogueDiscoveryResetGeneration{0};
#endif
#ifdef ZOOM_TRANSITION_DIAGNOSTIC
        SafetyHookMid zoomInHook;
        SafetyHookMid zoomOutHook;
#endif
#ifdef ZOOM_TRANSITION_DIAGNOSTIC
        std::atomic<std::uint64_t> zoomInHits{0};
        std::atomic<std::uint64_t> zoomOutHits{0};
        std::atomic<std::uint64_t> zoomLoggedEdges{0};
#ifdef CAMERA_STATE_SNAPSHOT_DIAGNOSTIC
        std::atomic<std::uint8_t> snapshotZoomDirection{};
        std::atomic<std::uint64_t> snapshotZoomSequence{};
        std::atomic<std::uintptr_t> snapshotZoomSource{};
        std::atomic<float> snapshotZoomPrimary{std::numeric_limits<float>::quiet_NaN()};
        std::atomic<float> snapshotZoomSecondary{std::numeric_limits<float>::quiet_NaN()};
#endif
#endif
#ifdef CAMERA_STATE_SNAPSHOT_DIAGNOSTIC
        std::atomic<std::uint64_t> snapshotEventSequence{};
        std::atomic<std::uintptr_t> snapshotDialogueSource{};
        std::atomic<float> snapshotDialogueTarget{std::numeric_limits<float>::quiet_NaN()};
        std::atomic<bool> snapshotDialogueTargetValid{};
#endif
#ifdef HORPLUS_FOV_STATE_DIAGNOSTIC
        std::mutex horPlusFovTelemetryMutex;
        bool horPlusFovTelemetryValid{};
        bool horPlusGameplayCacheValid{};
        std::uintptr_t stableObservationSource{};
        float stableObservationFov{std::numeric_limits<float>::quiet_NaN()};
        std::uint32_t stableObservationSamples{};
        bool stableObservationReported{};
        float horPlusTelemetryNativeFov{std::numeric_limits<float>::quiet_NaN()};
        float horPlusTelemetryAspect{std::numeric_limits<float>::quiet_NaN()};
        float horPlusTelemetryResult{std::numeric_limits<float>::quiet_NaN()};
        std::uint8_t horPlusTelemetryFlags{};
        config::GameplayMode horPlusTelemetryMode{config::GameplayMode::AspectRecalculation};
        std::uint8_t horPlusTelemetryOwner{};
#endif
#ifdef POST_EXIT_CAMERA_FOV_WRITE_OWNER_TRACE
        std::atomic<std::uint32_t> postExitWriteOwnerHitCount{0};
#endif
#ifdef POST_EXIT_FOV_PRODUCER_STORE_TRACE
        std::atomic<std::uint32_t> postExitProducerStoreHitCount{0};
#endif
        std::uint8_t* cinematicAspectStore{};
        std::uint8_t cinematicOriginalImmediate[sizeof(kCinematicOriginalImmediate)]{};
        bool cinematicAspectPatched{};
        std::atomic<std::uintptr_t> lastAutoRestoreSource{};
        std::atomic<std::uintptr_t> lastGameplayCameraSource{};
        std::atomic<float> lastGameplayCameraFov{std::numeric_limits<float>::quiet_NaN()};
        std::uintptr_t lastLoggedFovSource{};
        float lastLoggedWriterInputFov = std::numeric_limits<float>::quiet_NaN();
        float lastLoggedCameraWorldFov = std::numeric_limits<float>::quiet_NaN();
        float lastLoggedCameraFirstPersonFov = std::numeric_limits<float>::quiet_NaN();
        std::mutex dialogueMutex;
        Phase dialoguePhase{Phase::Inactive};
        dialogue::PolicySnapshot activeDialoguePolicy{};
        float dialogueBaseline = std::numeric_limits<float>::quiet_NaN();
        float dialoguePrevious = std::numeric_limits<float>::quiet_NaN();
        float dialogueExitIncomingStart{std::numeric_limits<float>::quiet_NaN()};
        dialogue::CandidateTracker dialogueCandidate{};
        dialogue::RecoveryRearm dialogueRecoveryRearm{};
    // The runtime is process-resident by contract. Deliberately allocate it
    // without registering a C++ static destructor so normal DLL teardown
    // cannot destroy hooks, mutexes or filesystem state under loader lock.
    };

    RuntimeState& g_runtime = *new RuntimeState{};

    auto& g_module = g_runtime.module;
    auto& g_executable = g_runtime.executable;
    auto& g_workers = g_runtime.workers;
    auto& g_fovWriteAddress = g_runtime.fovWriteAddress;
    auto& g_hooks = g_runtime.hooks;
    auto& g_hook = g_hooks.gameplay;
    auto& g_logger = g_runtime.logger;
    auto& g_config = g_runtime.config;
    auto& g_gameplayAvailable = g_runtime.gameplayAvailable;
    auto& g_runtimeGameplayMode = g_runtime.runtimeGameplayMode;
    auto& g_gameplayModeTransitionPending = g_runtime.gameplayModeTransitionPending;
    auto& g_gameplayModeTransitionDeferralLogged = g_runtime.gameplayModeTransitionDeferralLogged;
    auto& g_gameplayModeTransitionTarget = g_runtime.gameplayModeTransitionTarget;
    auto& g_runtimeCinematicPolicy = g_runtime.runtimeCinematicPolicy;
    auto& g_runtimeCinematicFovMode = g_runtime.runtimeCinematicFovMode;
    auto& g_activeCinematicAspectPolicy = g_runtime.activeCinematicAspectPolicy;
    auto& g_activeCinematicFovMode = g_runtime.activeCinematicFovMode;
    auto& g_cinematicSelectionGeneration = g_runtime.cinematicSelectionGeneration;
    auto& g_cinematicSelectionValid = g_runtime.cinematicSelectionValid;
    auto& g_gameplayHookGate = g_runtime.gameplayHookGate;
    auto& g_cinematicHookGate = g_runtime.cinematicHookGate;
    auto& g_stopping = g_runtime.stopping;
    auto& g_runtimeDialoguePolicy = g_runtime.runtimeDialoguePolicy;
    auto& g_cinematicLifecycleObservationAvailable =
        g_runtime.cinematicLifecycleObservationAvailable;
    auto& g_dialogueNonNativeCapabilityAvailable =
        g_runtime.dialogueNonNativeCapabilityAvailable;
#ifdef HORPLUS_FOV_STATE_DIAGNOSTIC
    auto& g_horPlusFovTelemetryMutex = g_runtime.horPlusFovTelemetryMutex;
#endif
    auto& g_configPath = g_runtime.configPath;
    auto& g_state = g_runtime.state;
    auto& g_lastCameraMode = g_runtime.lastCameraMode;
    auto& g_transitionTraceSequence = g_runtime.transitionTraceSequence;
    auto& g_coordinator = g_runtime.coordinator;
    auto& g_fovObservationStore = g_runtime.fovObservationStore;
    auto& g_gameplayBaselineStore = g_runtime.gameplayBaselineStore;
    auto& g_gameplayAspectRestorationStore = g_runtime.gameplayAspectRestorationStore;
    auto& g_cinematicFovApplied = g_runtime.cinematicFovApplied;
    auto& g_cinematicTransformedFov = g_runtime.cinematicTransformedFov;
#ifdef HORPLUS_CINEMATIC_WRITER_TRACE_DIAGNOSTIC
    auto& g_cinematicEnterAspect = g_runtime.cinematicEnterAspect;
#endif
    auto& g_matchGameplayEnterNativeFov = g_runtime.matchGameplayEnterNativeFov;
    auto& g_matchGameplayPreEnterNativeFov = g_runtime.matchGameplayPreEnterNativeFov;
    auto& g_matchGameplayPreEnterHorPlusFov = g_runtime.matchGameplayPreEnterHorPlusFov;
    auto& g_matchGameplayPreEnterAspect = g_runtime.matchGameplayPreEnterAspect;
    auto& g_matchGameplayPreEnterPairValid = g_runtime.matchGameplayPreEnterPairValid;
    auto& g_exitTargetFov = g_runtime.exitTargetFov;
    auto& g_postCinematicDialogueExclusion = g_runtime.postCinematicDialogueExclusion;
#ifdef POST_EXIT_TRACE_STATE
    auto& g_postExitTraceArmed = g_runtime.postExitTraceArmed;
    auto& g_postExitTraceWriterCount = g_runtime.postExitTraceWriterCount;
    auto& g_postExitTraceStartNs = g_runtime.postExitTraceStartNs;
    auto& g_postExitTraceSequence = g_runtime.postExitTraceSequence;
#endif
#ifdef POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_TEST
#ifndef POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_DELAY_MS
#define POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_DELAY_MS 120
#endif
    constexpr std::uint32_t kDeferredGameplayStableSamples = 3;
    constexpr std::int64_t kDeferredGameplayDelayNs =
        static_cast<std::int64_t>(POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_DELAY_MS) * 1000000LL;
    auto& g_deferredGameplayReplayPending = g_runtime.deferredGameplayReplayPending;
    auto& g_deferredGameplaySource = g_runtime.deferredGameplaySource;
    auto& g_deferredGameplayStableSamples = g_runtime.deferredGameplayStableSamples;
    auto& g_deferredGameplayLastFov = g_runtime.deferredGameplayLastFov;
    auto& g_deferredGameplayStableSinceNs = g_runtime.deferredGameplayStableSinceNs;
#endif
    auto& g_atomicExitHandoffPending = g_runtime.atomicExitHandoffPending;
    auto& g_atomicExitHandoffSource = g_runtime.atomicExitHandoffSource;
    auto& g_atomicExitHandoffPreviousFov = g_runtime.atomicExitHandoffPreviousFov;
#ifdef POST_EXIT_DELAYED_ASPECT_DIAGNOSTIC
    auto& g_delayedAspectDiagnosticApplied = g_runtime.delayedAspectDiagnosticApplied;
#endif
#ifdef HORPLUS_GAMEPLAY_DIAGNOSTIC
    auto& g_horPlusDiagnosticWriterCallbacks = g_runtime.horPlusDiagnosticWriterCallbacks;
    auto& g_horPlusDiagnosticApplications = g_runtime.horPlusDiagnosticApplications;
    auto& g_horPlusDiagnosticChanges = g_runtime.horPlusDiagnosticChanges;
#endif
#ifdef DIALOGUE_DISCOVERY_DIAGNOSTIC
    auto& g_dialogueDiscoveryActive = g_runtime.dialogueDiscoveryActive;
    auto& g_dialogueDiscoveryStartNs = g_runtime.dialogueDiscoveryStartNs;
    auto& g_dialogueDiscoverySamples = g_runtime.dialogueDiscoverySamples;
    auto& g_dialogueDiscoveryChanges = g_runtime.dialogueDiscoveryChanges;
    constexpr std::int64_t kDialogueDiscoveryDurationNs = 10'000'000'000LL;
#endif
#ifdef ZOOM_TRANSITION_DIAGNOSTIC
    auto& g_zoomInHook = g_runtime.zoomInHook;
    auto& g_zoomOutHook = g_runtime.zoomOutHook;
    auto& g_zoomInHits = g_runtime.zoomInHits;
    auto& g_zoomOutHits = g_runtime.zoomOutHits;
    auto& g_zoomLoggedEdges = g_runtime.zoomLoggedEdges;
#endif
#ifdef POST_EXIT_CAMERA_FOV_WRITE_OWNER_TRACE
    auto& g_postExitWriteOwnerHitCount = g_runtime.postExitWriteOwnerHitCount;
#endif
#ifdef POST_EXIT_FOV_PRODUCER_STORE_TRACE
    auto& g_postExitProducerStoreHitCount = g_runtime.postExitProducerStoreHitCount;
#endif
    auto& g_cinematicEnterHook = g_hooks.cinematicEnter;
    auto& g_cinematicExitHook = g_hooks.cinematicExit;
    auto& g_cinematicAspectStore = g_runtime.cinematicAspectStore;
    auto& g_cinematicOriginalImmediate = g_runtime.cinematicOriginalImmediate;
    auto& g_cinematicAspectPatched = g_runtime.cinematicAspectPatched;
    auto& g_cinematicAspectStoreHook = g_hooks.cinematicAspectStore;
    auto& g_dialogueBoundaryHook = g_hooks.dialogueBoundary;
#ifdef POST_EXIT_GAMEPLAY_OBSERVER_DIAGNOSTIC
    auto& g_postExitGameplayObserverHook = g_hooks.postExitGameplayObserver;
#endif
#ifdef POST_EXIT_FOV_STATE_CONSUMER_TRACE
    auto& g_postExitConsumerHook = g_hooks.postExitConsumer;
    constexpr std::uintptr_t kPostExitConsumerRva = 0x318DCD4;
    constexpr char kPostExitConsumerGameSha256[] =
        "e7b481a97c02d80581fab0bece940214a88ebe30211088a00129845a039f9293";
#endif
#ifdef POST_EXIT_CAMERA_FOV_WRITE_OWNER_TRACE
    auto& g_postExitWriteOwnerHook = g_hooks.postExitWriteOwner;
    constexpr std::uintptr_t kPostExitWriteOwnerRva = 0x3DB2CE7;
    constexpr char kPostExitWriteOwnerGameSha256[] =
        "e7b481a97c02d80581fab0bece940214a88ebe30211088a00129845a039f9293";
#endif

#ifdef POST_EXIT_FOV_PRODUCER_STORE_TRACE
    auto& g_postExitProducerStoreHook = g_hooks.postExitProducerStore;
    constexpr std::uintptr_t kPostExitProducerStoreRva = 0x32B779D;
    constexpr char kPostExitProducerStoreGameSha256[] =
        "e7b481a97c02d80581fab0bece940214a88ebe30211088a00129845a039f9293";
#endif
    auto& g_lastAutoRestoreSource = g_runtime.lastAutoRestoreSource;
    auto& g_lastGameplayCameraSource = g_runtime.lastGameplayCameraSource;
    auto& g_lastGameplayCameraFov = g_runtime.lastGameplayCameraFov;
    auto& g_lastLoggedFovSource = g_runtime.lastLoggedFovSource;
    auto& g_lastLoggedWriterInputFov = g_runtime.lastLoggedWriterInputFov;
    auto& g_lastLoggedCameraWorldFov = g_runtime.lastLoggedCameraWorldFov;
    auto& g_lastLoggedCameraFirstPersonFov = g_runtime.lastLoggedCameraFirstPersonFov;
    auto& g_dialogueMutex = g_runtime.dialogueMutex;
    auto& g_dialoguePhase = g_runtime.dialoguePhase;
    auto& g_activeDialoguePolicy = g_runtime.activeDialoguePolicy;
    auto& g_dialogueBaseline = g_runtime.dialogueBaseline;
    auto& g_dialoguePrevious = g_runtime.dialoguePrevious;
    auto& g_dialogueExitIncomingStart = g_runtime.dialogueExitIncomingStart;
    auto& g_dialogueCandidate = g_runtime.dialogueCandidate;
    auto& g_dialogueRecoveryRearm = g_runtime.dialogueRecoveryRearm;

    template <typename... Args>
    void Log(Args&&... args) noexcept;

    bool WaitForWorkerStop(DWORD timeout)
    {
        return g_workers.WaitForStop(timeout);
    }

    bool StartWorker(LPTHREAD_START_ROUTINE entry, const char* name)
    {
        if (!g_workers.Start(entry)) {
            Log("Worker startup failed: ", name, ".");
            return false;
        }
        Log("Worker started: ", name, ".");
        return true;
    }

    bool StopWorkers(bool wait)
    {
        if (!wait) {
            g_workers.SignalStop();
            return true;
        }
        return g_workers.StopAndJoin();
    }

    bool CreateWorkerStopEvent()
    {
        return g_workers.Initialize();
    }

    void RestoreCinematicAspect() noexcept;

    void ResetDialogueRuntimeState()
    {
        if (g_activeDialoguePolicy.IsValid())
            Log("Dialogue active policy released: policy=", DialogueZoomPolicyName(g_activeDialoguePolicy.Value()), ".");
        g_dialoguePhase = Phase::Inactive;
        g_activeDialoguePolicy.Clear();
        g_dialogueBaseline = std::numeric_limits<float>::quiet_NaN();
        g_dialoguePrevious = std::numeric_limits<float>::quiet_NaN();
        g_dialogueExitIncomingStart = std::numeric_limits<float>::quiet_NaN();
        g_dialogueCandidate.Reset();
        g_dialogueRecoveryRearm.Reset();
#ifdef CAMERA_STATE_SNAPSHOT_DIAGNOSTIC
        g_runtime.snapshotDialogueSource.store(0, std::memory_order_release);
        g_runtime.snapshotDialogueTarget.store(std::numeric_limits<float>::quiet_NaN(),
            std::memory_order_release);
        g_runtime.snapshotDialogueTargetValid.store(false, std::memory_order_release);
#endif
    }

    void ResetPostCinematicDialogueExclusion(const char* reason)
    {
        const bool wasActive = g_postCinematicDialogueExclusion.Reset();
        if (wasActive) Log("Post-cinematic Dialogue exclusion cleared: reason=", reason, ".");
    }

    void ArmPostCinematicDialogueExclusion()
    {
        g_postCinematicDialogueExclusion.Arm(g_exitTargetFov.load(std::memory_order_acquire));
        Log("Post-cinematic Dialogue exclusion armed: release=validated-native-recovery.");
    }

    void ObservePostCinematicDialogueRecovery(SafetyHookContext& context)
    {
        const auto source = static_cast<std::uintptr_t>(context.rsi);
        const float currentFov = context.xmm0.f32[0];
        const bool wasSuppressed = g_postCinematicDialogueExclusion.Observe(
            source, currentFov, kRecoveryEpsilon);
        if (wasSuppressed && !g_postCinematicDialogueExclusion.IsActive()) {
            Log("Post-cinematic Dialogue exclusion cleared: reason=native-recovery-converged.");
        }
    }

#ifdef DIALOGUE_BOUNDARY_DIAGNOSTIC
    void LogDialogueDiagnosticSummary();
#endif

    void ResetAllRuntimeResources()
    {
        g_stopping.store(true, std::memory_order_release);
        g_gameplayHookGate.store(false, std::memory_order_release);
        g_cinematicHookGate.store(false, std::memory_order_release);
        if (!StopWorkers(true)) {
            Log("Runtime reset deferred: required worker join did not complete; hook resources retained.");
            return;
        }
#ifdef DIALOGUE_BOUNDARY_DIAGNOSTIC
        LogDialogueDiagnosticSummary();
#endif
        ResetPostCinematicDialogueExclusion("runtime-reset");
#ifdef POST_EXIT_GAMEPLAY_OBSERVER_DIAGNOSTIC
        g_postExitGameplayObserverHook.reset();
#endif
#ifdef POST_EXIT_CAMERA_FOV_WRITE_OWNER_TRACE
        g_postExitWriteOwnerHook.reset();
#endif
#ifdef POST_EXIT_FOV_PRODUCER_STORE_TRACE
        g_postExitProducerStoreHook.reset();
#endif
#ifdef POST_EXIT_FOV_STATE_CONSUMER_TRACE
        g_postExitConsumerHook.reset();
#endif
#ifdef ZOOM_TRANSITION_DIAGNOSTIC
        g_zoomInHook.reset();
        g_zoomOutHook.reset();
        if (g_zoomInHits.load(std::memory_order_relaxed) != 0 ||
            g_zoomOutHits.load(std::memory_order_relaxed) != 0)
            Log("ZOOM_TRANSITION SUMMARY inHits=", g_zoomInHits.load(std::memory_order_relaxed),
                " outHits=", g_zoomOutHits.load(std::memory_order_relaxed),
                " loggedEdges=", g_zoomLoggedEdges.load(std::memory_order_relaxed), ".");
#endif
        g_hook.reset();
        g_dialogueBoundaryHook.reset();
        g_cinematicExitHook.reset();
        g_cinematicEnterHook.reset();
        RestoreCinematicAspect();
        g_gameplayAvailable.store(false, std::memory_order_release);
        g_cinematicSelectionValid.store(false, std::memory_order_release);
    }
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
#ifdef FOV_SETTINGS_TRACE_DIAGNOSTIC
    float g_lastLoggedGameplayFov = std::numeric_limits<float>::quiet_NaN();
    std::uintptr_t g_lastLoggedGameplayFovSource{};
#endif

    bool IsValidAspect(float aspect)
    {
        return std::isfinite(aspect) && aspect > 0.0f;
    }

    void TraceGameplayAspectRestorationState(
        camera::GameplayAspectRestorationDisposition disposition,
        const camera::GameplayAspectRestorationState& shadow)
    {
        if (!diagnostics::Enabled()) return;
        Log("RESTORATION_STATE action=",
            camera::GameplayAspectRestorationDispositionName(disposition),
            " aspect=", shadow.aspect, " valid=", shadow.valid ? "true" : "false",
            " source=0x", std::hex, shadow.source.value, std::dec,
            " sequence=", shadow.observationSequence, ".");
    }

    void UpdateGameplayAspectRestorationState(float aspect, std::uintptr_t source)
    {
        const auto result = g_gameplayAspectRestorationStore.Update(
            aspect, { source, source != 0 });
        if (result.changed)
            TraceGameplayAspectRestorationState(result.disposition, result.state);
    }

    void InvalidateGameplayAspectRestorationState()
    {
        const auto result = g_gameplayAspectRestorationStore.Invalidate();
        if (result.changed)
            TraceGameplayAspectRestorationState(result.disposition, result.state);
    }

    template <typename... Args>
    void Log(Args&&... args) noexcept
    {
        try {
            if (!g_logger) return;
            std::ostringstream message;
            (message << ... << args);
            g_logger->info("{}", message.str());
        } catch (...) {
            // Logging is auxiliary at native callback/thread boundaries.
        }
    }

    template <void (*Callback)(SafetyHookContext&)>
    void SafeMidHookEntry(SafetyHookContext& context) noexcept
    {
        try {
            Callback(context);
        } catch (...) {
            // Native hook boundaries fail closed by preserving the generated
            // trampoline's pass-through continuation.
        }
    }

#ifdef POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_TEST
    std::int64_t NowSteadyNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void CancelDeferredGameplayReplay(const char* reason, std::uintptr_t source = 0,
        float fov = std::numeric_limits<float>::quiet_NaN(), float aspect = 0.0f,
        std::uint8_t flags = 0)
    {
        const bool wasPending = g_deferredGameplayReplayPending.exchange(false,
            std::memory_order_acq_rel);
        const auto samples = g_deferredGameplayStableSamples.exchange(0,
            std::memory_order_acq_rel);
        g_deferredGameplaySource.store(0, std::memory_order_release);
        g_deferredGameplayLastFov.store(std::numeric_limits<float>::quiet_NaN(),
            std::memory_order_release);
        g_deferredGameplayStableSinceNs.store(0, std::memory_order_release);
        if (wasPending || samples != 0)
            Log("DeferCancelled: reason=", reason, " source=0x", std::hex, source,
                std::dec, " fov=", fov, " aspect=", aspect, " flags=0x", std::hex,
                static_cast<unsigned>(flags), std::dec, " stableSamples=", samples, ".");
    }

    void ArmDeferredGameplayReplay(float targetFov)
    {
        g_deferredGameplaySource.store(0, std::memory_order_release);
        g_deferredGameplayStableSamples.store(0, std::memory_order_release);
        g_deferredGameplayLastFov.store(std::numeric_limits<float>::quiet_NaN(),
            std::memory_order_release);
        g_deferredGameplayStableSinceNs.store(0, std::memory_order_release);
        g_deferredGameplayReplayPending.store(true, std::memory_order_release);
        Log("DeferArmed: targetFov=", targetFov, " requiredStableSamples=3 delayMs=",
            POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_DELAY_MS, ".");
    }
#endif

    void ResetAtomicExitHandoff(const char* reason)
    {
        const bool wasPending = g_atomicExitHandoffPending.exchange(false,
            std::memory_order_acq_rel);
        g_atomicExitHandoffSource.store(0, std::memory_order_release);
        g_atomicExitHandoffPreviousFov.store(std::numeric_limits<float>::quiet_NaN(),
            std::memory_order_release);
        if (wasPending) Log("AtomicExitHandoffCancelled: reason=", reason, ".");
    }

    void ArmAtomicExitHandoff()
    {
        g_atomicExitHandoffSource.store(0, std::memory_order_release);
        g_atomicExitHandoffPreviousFov.store(std::numeric_limits<float>::quiet_NaN(),
            std::memory_order_release);
        g_atomicExitHandoffPending.store(true, std::memory_order_release);
        Log("AtomicExitHandoffArmed: trigger=first-downward-fov-sample.");
    }

    bool WriteAspectAndFlags(std::uintptr_t source, float aspect, std::uint8_t flags);

#ifdef POST_EXIT_DELAYED_ASPECT_DIAGNOSTIC
    bool ApplyDelayedAspectDiagnostic(std::uintptr_t source, float currentFov,
        float currentAspect, std::uint8_t currentFlags)
    {
        bool expectedApplied = false;
        if (!g_delayedAspectDiagnosticApplied.compare_exchange_strong(
                expectedApplied, true, std::memory_order_acq_rel)) return false;
        if (!std::isfinite(currentAspect) ||
            std::fabs(currentAspect - kWideAspect) > 0.001f || currentFlags != 0x4) {
            g_delayedAspectDiagnosticApplied.store(false, std::memory_order_release);
            Log("DELAYED_ASPECT B REFUSED: unexpected source state; aspect=", currentAspect,
                " flags=0x", std::hex, static_cast<unsigned>(currentFlags), std::dec, ".");
            return false;
        }
        Log("DELAYED_ASPECT B BEFORE: source=0x", std::hex, source, std::dec,
            " fov=", currentFov, " aspect=", currentAspect, " flags=0x", std::hex,
            static_cast<unsigned>(currentFlags), std::dec, ".");
        if (!WriteAspectAndFlags(source, kNativeAspect, 0x4)) {
            g_delayedAspectDiagnosticApplied.store(false, std::memory_order_release);
            Log("DELAYED_ASPECT B REFUSED: validated aspect/flags write was not writable.");
            return false;
        }
        g_state.store(ReplayState::Complete, std::memory_order_release);
        g_lastAutoRestoreSource.store(source, std::memory_order_release);
        Log("DELAYED_ASPECT B AFTER: source=0x", std::hex, source, std::dec,
            " fov=", currentFov, " aspect=", kNativeAspect, " flags=0x4.");
        return true;
    }
#endif

    void LogGameplayFovSourceChange(std::uintptr_t source, float writerInputXmm0,
        float cameraWorldFov, float cameraFirstPersonFov, float aspect, std::uint8_t flags)
    {
        if (!diagnostics::Enabled()) return;
        const bool sourceChanged = g_lastLoggedFovSource != source;
        const auto changed = [](float current, float previous) {
            return std::isfinite(current) != std::isfinite(previous) ||
                (std::isfinite(current) && std::isfinite(previous) &&
                    std::fabs(current - previous) > 0.01f);
        };
        if (!sourceChanged && !changed(writerInputXmm0, g_lastLoggedWriterInputFov) &&
            !changed(cameraWorldFov, g_lastLoggedCameraWorldFov) &&
            !changed(cameraFirstPersonFov, g_lastLoggedCameraFirstPersonFov)) return;

        const auto coordinator = g_coordinator.load(std::memory_order_acquire);
        DialoguePhase dialoguePhase = DialoguePhase::Inactive;
        {
            std::lock_guard dialogueLock(g_dialogueMutex);
            dialoguePhase = g_dialoguePhase;
        }
        Log("Gameplay FOV source change: event=", CoordinatorStateName(coordinator),
            " sourceChanged=", sourceChanged, " source=0x", std::hex, source, std::dec,
            " writerInputXmm0=", writerInputXmm0,
            " cameraWorldFov(+0x230)=", cameraWorldFov,
            " cameraFirstPersonFov(+0x234)=", cameraFirstPersonFov,
            " aspect=", aspect, " flags=0x", std::hex, static_cast<int>(flags), std::dec,
            " replayState=", ReplayStateName(g_state.load(std::memory_order_acquire)),
            " coordinator=", CoordinatorStateName(coordinator),
            " dialoguePhase=", DialoguePhaseName(dialoguePhase), ".");
        g_lastLoggedFovSource = source;
        g_lastLoggedWriterInputFov = writerInputXmm0;
        g_lastLoggedCameraWorldFov = cameraWorldFov;
        g_lastLoggedCameraFirstPersonFov = cameraFirstPersonFov;
    }

    template <typename T>
    bool SafeRead(std::uintptr_t address, T& value);

#if defined(POST_EXIT_RAW_TRACE_DIAGNOSTIC) || defined(POST_EXIT_INTERPOLATED_FOV_PRODUCER_TRACE)
    struct PostExitTraceSnapshot
    {
        bool valid{};
        std::uint64_t sequence{};
        std::uintptr_t source{};
        std::uintptr_t output{};
        float writerInputXmm0{};
        float cameraWorldFov{};
        float cameraFirstPersonFov{};
        float aspect{};
        std::uint8_t flags{};
        std::uint8_t selector{};
        float outputFov{};
        float outputAspect{};
        CoordinatorState coordinator{CoordinatorState::Gameplay};
        ReplayState replayState{ReplayState::WaitingForAutomaticUpdate};
        DialoguePhase dialoguePhase{DialoguePhase::Inactive};
    };

    thread_local PostExitTraceSnapshot g_previousPostExitTrace{};

    void LogPostExitWriterObservation(std::uintptr_t source, std::uintptr_t output,
        float writerInputXmm0, float cameraWorldFov, float cameraFirstPersonFov,
        float aspect, std::uint8_t flags, std::uint8_t selector,
        float outputFov, float outputAspect)
    {
        if (!g_postExitTraceArmed.load(std::memory_order_acquire) || !g_logger) return;
        const auto ordinal = g_postExitTraceWriterCount.fetch_add(1, std::memory_order_acq_rel) + 1;
        const auto startNs = g_postExitTraceStartNs.load(std::memory_order_acquire);
        const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        const auto elapsedUs = startNs > 0 ? (nowNs - startNs) / 1000 : -1;
        DialoguePhase dialoguePhase = DialoguePhase::Inactive;
        {
            std::lock_guard dialogueLock(g_dialogueMutex);
            dialoguePhase = g_dialoguePhase;
        }

        const auto sequence = g_postExitTraceSequence.load(std::memory_order_acquire);
        const auto currentCoordinator = g_coordinator.load(std::memory_order_acquire);
        const auto replayState = g_state.load(std::memory_order_acquire);
        const auto floatChanged = [](float current, float previous) {
            return std::isfinite(current) != std::isfinite(previous) ||
                (std::isfinite(current) && std::isfinite(previous) &&
                    std::fabs(current - previous) > 0.01f);
        };
        const bool newSequence = !g_previousPostExitTrace.valid ||
            g_previousPostExitTrace.sequence != sequence;
        std::array<char, 256> changedFields{};
        std::size_t changedFieldLength = 0;
        const auto addChanged = [&changedFields, &changedFieldLength](const char* name) {
            const auto nameLength = std::strlen(name);
            const auto separatorLength = changedFieldLength == 0 ? 0u : 1u;
            if (changedFieldLength + separatorLength + nameLength >= changedFields.size()) return;
            if (separatorLength != 0) changedFields[changedFieldLength++] = '|';
            std::memcpy(changedFields.data() + changedFieldLength, name, nameLength);
            changedFieldLength += nameLength;
        };
        if (newSequence || g_previousPostExitTrace.source != source) addChanged("source");
        if (newSequence || g_previousPostExitTrace.output != output) addChanged("output");
        if (newSequence || floatChanged(writerInputXmm0, g_previousPostExitTrace.writerInputXmm0))
            addChanged("fovIn");
        if (newSequence || floatChanged(cameraWorldFov, g_previousPostExitTrace.cameraWorldFov))
            addChanged("cameraWorldFov");
        if (newSequence || floatChanged(cameraFirstPersonFov,
            g_previousPostExitTrace.cameraFirstPersonFov)) addChanged("cameraFirstPersonFov");
        if (newSequence || floatChanged(aspect, g_previousPostExitTrace.aspect)) addChanged("aspect");
        if (newSequence || flags != g_previousPostExitTrace.flags) addChanged("flags");
        if (newSequence || selector != g_previousPostExitTrace.selector) addChanged("selector");
        if (newSequence || floatChanged(outputFov, g_previousPostExitTrace.outputFov))
            addChanged("outputFov");
        if (newSequence || floatChanged(outputAspect, g_previousPostExitTrace.outputAspect))
            addChanged("outputAspect");
        if (newSequence || coordinator != g_previousPostExitTrace.coordinator)
            addChanged("coordinator");
        if (newSequence || replayState != g_previousPostExitTrace.replayState)
            addChanged("replay");
        if (newSequence || dialoguePhase != g_previousPostExitTrace.dialoguePhase)
            addChanged("dialogue");

        if (changedFieldLength != 0) {
            changedFields[changedFieldLength] = '\0';
            Log("POST_EXIT seq=", sequence,
            " ordinal=", ordinal, " elapsedUs=", elapsedUs,
            " changed=", changedFields.data(),
            " source=0x", std::hex, source, " output=0x", output, std::dec,
            " writerInputXmm0=", writerInputXmm0,
            " cameraWorldFov(+0x230)=", cameraWorldFov,
            " cameraFirstPersonFov(+0x234)=", cameraFirstPersonFov,
            " aspect=", aspect, " flags=0x", std::hex, static_cast<int>(flags),
            " selector=0x", static_cast<int>(selector), std::dec,
            " outputFOV(before)=", outputFov, " outputAspect(before)=", outputAspect,
            " coordinator=", CoordinatorStateName(coordinator),
            " replayState=", ReplayStateName(replayState),
            " dialoguePhase=", DialoguePhaseName(dialoguePhase), ".");
        }

        g_previousPostExitTrace = PostExitTraceSnapshot{
            true, sequence, source, output, writerInputXmm0, cameraWorldFov,
            cameraFirstPersonFov, aspect, flags, selector, outputFov, outputAspect,
            coordinator, replayState, dialoguePhase };
    }
#endif

#if defined(POST_EXIT_RAW_TRACE_DIAGNOSTIC) || defined(POST_EXIT_INTERPOLATED_FOV_PRODUCER_TRACE)
    void ObservePostExitRawWriterEntry(SafetyHookContext& context)
    {
        if (!g_postExitTraceArmed.load(std::memory_order_acquire)) return;
        const auto source = static_cast<std::uintptr_t>(context.rsi);
        const auto output = static_cast<std::uintptr_t>(context.rbx);
        float aspect = 0.0f;
        std::uint8_t flags = 0;
        float cameraWorldFov = 0.0f;
        float cameraFirstPersonFov = 0.0f;
        float outputFov = 0.0f;
        float outputAspect = 0.0f;
        std::uint8_t selector = 0;
        if (!SafeRead(source + kAspectOffset, aspect) ||
            !SafeRead(source + kFlagsOffset, flags) ||
            !SafeRead(source + 0x230, cameraWorldFov) ||
            !SafeRead(source + 0x234, cameraFirstPersonFov) ||
            !SafeRead(source + 0x262, selector) ||
            !SafeRead(output + 0x30, outputFov) ||
            !SafeRead(output + 0x5C, outputAspect)) return;
        LogPostExitWriterObservation(source, output, context.xmm0.f32[0],
            cameraWorldFov, cameraFirstPersonFov, aspect, flags, selector,
            outputFov, outputAspect);
#ifdef POST_EXIT_INTERPOLATED_FOV_PRODUCER_TRACE
        std::uintptr_t returnAddress = 0;
        SafeRead(static_cast<std::uintptr_t>(context.rsp), returnAddress);
        Log("Post-EXIT interpolated FOV producer marker: seq=",
            g_postExitTraceSequence.load(std::memory_order_acquire),
            " elapsedUs=", [&]() {
                const auto startNs = g_postExitTraceStartNs.load(std::memory_order_acquire);
                const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                return startNs > 0 ? (nowNs - startNs) / 1000 : -1;
            }(),
            " writerRIP=0x", std::hex,
            reinterpret_cast<std::uintptr_t>(g_fovWriteAddress),
            " return=0x", returnAddress, " source=0x", source, std::dec,
            " writerInputXmm0=", context.xmm0.f32[0],
            " cameraWorldFov=", cameraWorldFov,
            " threadId=", GetCurrentThreadId(), ".");
#endif
    }
#endif

#ifdef POST_EXIT_FOV_PRODUCER_STORE_TRACE
    void ObservePostExitProducerStore(SafetyHookContext& context)
    {
        if (!g_logger) return;
        const auto camera = static_cast<std::uintptr_t>(context.rsi);
        const auto destination = camera + 0x230;
        float previousFov = 0.0f;
        const bool readable = camera != 0 && SafeRead(destination, previousFov);
        const bool armed = g_postExitTraceArmed.load(std::memory_order_acquire);
        const auto coordinator = g_coordinator.load(std::memory_order_acquire);
        const auto startNs = g_postExitTraceStartNs.load(std::memory_order_acquire);
        const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        const auto elapsedUs = startNs > 0 ? (nowNs - startNs) / 1000 : -1;
        const auto hitOrdinal = g_postExitProducerStoreHitCount.fetch_add(
            1, std::memory_order_acq_rel) + 1;
        Log("Post-EXIT FOV producer store HIT: hitOrdinal=", hitOrdinal,
            " armed=", armed, " elapsedUs=", elapsedUs,
            " storeRIP=0x", std::hex,
            reinterpret_cast<std::uintptr_t>(g_executable) + kPostExitProducerStoreRva,
            " camera=0x", camera, " destination=0x", destination, std::dec,
            " previousFov=", previousFov, " writtenFov=", context.xmm0.f32[0],
            " readable=", readable, " coordinator=", CoordinatorStateName(coordinator),
            " sequence=", g_postExitTraceSequence.load(std::memory_order_acquire),
            " threadId=", GetCurrentThreadId(), ".");
        if (hitOrdinal >= 256) g_postExitProducerStoreHitCount.store(0, std::memory_order_release);
    }

    bool InstallPostExitProducerStoreTrace(const std::string& gameHash)
    {
        if (_stricmp(gameHash.c_str(), kPostExitProducerStoreGameSha256) != 0) {
            Log("Post-EXIT FOV producer store trace refused: game identity mismatch.");
            return false;
        }
        auto* target = reinterpret_cast<std::uint8_t*>(
            reinterpret_cast<std::uintptr_t>(g_executable) + kPostExitProducerStoreRva);
        constexpr std::uint8_t expected[] = {
            0xF3, 0x0F, 0x11, 0x86, 0x30, 0x02, 0x00, 0x00
        };
        if (!hooks::validation::IsExecutable(reinterpret_cast<std::uintptr_t>(target)) ||
            std::memcmp(target, expected, sizeof(expected)) != 0) {
            Log("Post-EXIT FOV producer store trace refused: 2.0.5 store bytes mismatch.");
            return false;
        }
        g_postExitProducerStoreHook = safetyhook::create_mid(target, ObservePostExitProducerStore);
        if (!g_postExitProducerStoreHook) {
            Log("Post-EXIT FOV producer store trace refused: hook creation failed.");
            return false;
        }
        Log("Post-EXIT FOV producer store trace installed: RVA=0x32B779D; observation only.");
        return true;
    }
#endif

#ifdef POST_EXIT_CAMERA_FOV_WRITE_OWNER_TRACE
    void ObservePostExitFovWriteOwner(SafetyHookContext& context)
    {
        if (!g_logger) return;

        const auto camera = static_cast<std::uintptr_t>(context.rax);
        const auto destination = camera + 0x230;
        float previousFov = 0.0f;
        const bool readable = camera != 0 && SafeRead(camera + 0x230, previousFov);
        const bool armed = g_postExitTraceArmed.load(std::memory_order_acquire);
        const auto coordinator = g_coordinator.load(std::memory_order_acquire);
        const auto startNs = g_postExitTraceStartNs.load(std::memory_order_acquire);
        const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        const auto elapsedUs = startNs > 0 ? (nowNs - startNs) / 1000 : -1;
        std::uintptr_t returnAddress = 0;
        SafeRead(static_cast<std::uintptr_t>(context.rsp), returnAddress);
        const auto hitOrdinal = g_postExitWriteOwnerHitCount.fetch_add(1, std::memory_order_acq_rel) + 1;
        Log("Post-EXIT FOV store candidate HIT: hitOrdinal=", hitOrdinal,
            " armed=", armed, " elapsedUs=", elapsedUs,
            " writeRIP=0x", std::hex,
            reinterpret_cast<std::uintptr_t>(g_executable) + kPostExitWriteOwnerRva,
            " return=0x", returnAddress, " camera=0x", camera, std::dec,
            " destination=0x", std::hex, destination, std::dec,
            " previousFov=", previousFov,
            " writtenFov=", context.xmm0.f32[0],
            " readable=", readable,
            " coordinator=", CoordinatorStateName(coordinator),
            " sequence=", g_postExitTraceSequence.load(std::memory_order_acquire),
            " threadId=", GetCurrentThreadId(), ".");
        if (hitOrdinal >= 256) g_postExitWriteOwnerHitCount.store(0, std::memory_order_release);
    }

    bool InstallPostExitFovWriteOwnerTrace(const std::string& gameHash)
    {
        if (_stricmp(gameHash.c_str(), kPostExitWriteOwnerGameSha256) != 0) {
            Log("Post-EXIT FOV physical-write trace refused: game identity mismatch.");
            return false;
        }
        auto* target = reinterpret_cast<std::uint8_t*>(
            reinterpret_cast<std::uintptr_t>(g_executable) + kPostExitWriteOwnerRva);
        constexpr std::uint8_t expected[] = {
            0xF3, 0x0F, 0x11, 0x80, 0x30, 0x02, 0x00, 0x00
        };
        if (!hooks::validation::IsExecutable(reinterpret_cast<std::uintptr_t>(target)) ||
            std::memcmp(target, expected, sizeof(expected)) != 0) {
            Log("Post-EXIT FOV physical-write trace refused: 2.0.5 store bytes mismatch.");
            return false;
        }
        g_postExitWriteOwnerHook = safetyhook::create_mid(target, ObservePostExitFovWriteOwner);
        if (!g_postExitWriteOwnerHook) {
            Log("Post-EXIT FOV physical-write trace refused: hook creation failed.");
            return false;
        }
        Log("Post-EXIT FOV physical-write trace installed: RVA=0x3DB2CE7; observation only.");
        return true;
    }
#endif

#ifdef POST_EXIT_FOV_STATE_CONSUMER_TRACE
    void ObservePostExitFovConsumer(SafetyHookContext& context)
    {
        const auto coordinator = g_coordinator.load(std::memory_order_acquire);
#ifdef CINEMATIC_FOV_TRANSITION_TRACE
        if (!g_logger || (coordinator != CoordinatorState::CinematicActive &&
            coordinator != CoordinatorState::CinematicExiting)) return;
#else
        if (!g_postExitTraceArmed.load(std::memory_order_acquire) ||
            coordinator != CoordinatorState::CinematicExiting || !g_logger) return;
#endif
        const auto state = static_cast<std::uintptr_t>(context.rcx);
        const auto callerAddress = [&]() {
            std::uintptr_t value{};
            SafeRead(static_cast<std::uintptr_t>(context.rsp), value);
            return value;
        }();
        float state50 = 0.0f;
        float state54 = 0.0f;
        float state58 = 0.0f;
        const bool reads = state != 0 &&
            SafeRead(state + 0x50, state50) && SafeRead(state + 0x54, state54) &&
            SafeRead(state + 0x58, state58);
        const auto startNs = g_postExitTraceStartNs.load(std::memory_order_acquire);
        const auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        const auto elapsedUs = coordinator == CoordinatorState::CinematicExiting && startNs > 0
            ? (nowNs - startNs) / 1000 : -1;
        Log("Cinematic FOV consumer: phase=", CoordinatorStateName(coordinator),
            " seq=", g_postExitTraceSequence.load(std::memory_order_acquire),
            " elapsedUs=", elapsedUs, " incomingFov=", context.xmm1.f32[0],
            " state=0x", std::hex, state, " caller=0x", callerAddress, std::dec,
            " threadId=", GetCurrentThreadId(), " stateRead=", reads,
            " state+0x50=", state50, " state+0x54(before)=", state54,
            " state+0x58=", state58, ".");
    }

    bool InstallPostExitFovConsumerTrace(const std::string& gameHash)
    {
        if (_stricmp(gameHash.c_str(), kPostExitConsumerGameSha256) != 0) {
            Log("Post-EXIT FOV consumer trace refused: game identity mismatch.");
            return false;
        }
        auto* target = reinterpret_cast<std::uint8_t*>(
            reinterpret_cast<std::uintptr_t>(g_executable) + kPostExitConsumerRva);
        constexpr std::uint8_t expected[] = {
            0x56, 0x57, 0x48, 0x83, 0xEC, 0x28, 0x48, 0x89, 0xCE,
            0xF3, 0x0F, 0x11
        };
        if (!hooks::validation::IsExecutable(reinterpret_cast<std::uintptr_t>(target)) ||
            std::memcmp(target, expected, sizeof(expected)) != 0) {
            Log("Post-EXIT FOV consumer trace refused: 2.0.5 prologue mismatch.");
            return false;
        }
        g_postExitConsumerHook = safetyhook::create_mid(target, ObservePostExitFovConsumer);
        if (!g_postExitConsumerHook) {
            Log("Post-EXIT FOV consumer trace refused: hook creation failed.");
            return false;
        }
        Log("Post-EXIT FOV consumer trace installed: RVA=0x318DCD4; observation only.");
        return true;
    }
#endif

#ifdef FOV_SETTINGS_TRACE_DIAGNOSTIC
    void LogGameplayFovChange(std::uintptr_t source, float fov)
    {
        const auto coordinator = g_coordinator.load(std::memory_order_acquire);
        const auto replayState = g_state.load(std::memory_order_acquire);
        if (source != g_lastLoggedGameplayFovSource ||
            !std::isfinite(g_lastLoggedGameplayFov) ||
            std::fabs(fov - g_lastLoggedGameplayFov) > 0.01f) {
            Log("Gameplay FOV trace: source=0x", std::hex, source, std::dec,
                " writerInputXmm0=", fov,
                " coordinator=", static_cast<std::uint32_t>(coordinator),
                " replayState=", static_cast<std::uint32_t>(replayState), ".");
            g_lastLoggedGameplayFovSource = source;
            g_lastLoggedGameplayFov = fov;
        }
    }
#endif

    bool ComputeSha256(const std::filesystem::path& path, std::string& result)
    {
        return platform::win32::ComputeSha256(path, result);
    }

    bool CinematicAspectOverrideEnabled()
    {
        return cinematics::OverrideEnabled(g_runtimeCinematicPolicy.load(std::memory_order_acquire));
    }

    bool IsWritable(std::uintptr_t address, std::size_t size)
    {
        return platform::win32::IsWritable(address, size);
    }

    bool IsValidFovValue(float value)
    {
        return std::isfinite(value) && value > 1.0f && value < 179.0f;
    }

    template <typename T>
    bool SafeRead(std::uintptr_t address, T& value)
    {
        return platform::win32::ReadMemory(address, &value, sizeof(value));
    }

    float ReadClientViewportAspect()
    {
        return platform::win32::ReadClientViewportAspect();
    }

    float ResolveAutoAspect()
    {
        const float aspect = ReadClientViewportAspect();
        if (IsValidAspect(aspect)) {
            Log("Auto cinematic aspect resolved from client/display viewport: aspect=", aspect, ".");
            return aspect;
        }
        Log("Auto cinematic aspect resolution unavailable; native aspect fallback=", kNativeAspect, ".");
        return kNativeAspect;
    }

    float ResolveCinematicAspect()
    {
        return cinematics::ResolveAspect(g_activeCinematicAspectPolicy.load(std::memory_order_acquire),
            kNativeAspect, kCinemaAspect, kWideAspect, ResolveAutoAspect);
    }

    float ResolveCinematicAspect(config::CinematicAspectPolicy policy)
    {
        return cinematics::ResolveAspect(policy, kNativeAspect, kCinemaAspect,
            kWideAspect, ResolveAutoAspect);
    }

    bool WriteAspectAndFlags(std::uintptr_t source, float aspect, std::uint8_t flags)
    {
        return gameplay::WriteAspectAndFlags(source, kAspectOffset, kFlagsOffset,
            aspect, flags, IsWritable);
    }

    bool WriteAspectOnly(std::uintptr_t source, float aspect)
    {
        return gameplay::WriteAspectOnly(source, kAspectOffset, aspect, IsWritable);
    }

    bool ApplyPendingGameplayModeTransition(SafetyHookContext& context)
    {
        if (!g_gameplayModeTransitionPending.load(std::memory_order_acquire) ||
            g_gameplayModeTransitionTarget.load(std::memory_order_acquire) !=
                config::GameplayMode::HorPlus ||
            !g_config.gameplayEnabled ||
            g_coordinator.load(std::memory_order_acquire) != CoordinatorState::Gameplay)
            return false;

        const auto source = static_cast<std::uintptr_t>(context.rsi);
        float currentAspect = 0.0f;
        std::uint8_t flags = 0;
        if (!source || !SafeRead(source + kAspectOffset, currentAspect) ||
            !SafeRead(source + kFlagsOffset, flags)) {
            if (!g_gameplayModeTransitionDeferralLogged.exchange(true, std::memory_order_acq_rel))
                Log("Gameplay mode transition deferred: reason=unreadable_camera_state.");
            if (diagnostics::Enabled())
                Log("RESTORATION_CONSUMER decision=DEFER reason=unreadable_camera_state.");
            return false;
        }

        const auto restoration = g_gameplayAspectRestorationStore.Read();
        const camera::GameplayAspectRestorationDecisionInput decisionInput{
            true,
            true,
            g_config.gameplayEnabled,
            g_coordinator.load(std::memory_order_acquire) == CoordinatorState::Gameplay,
            true,
            IsValidAspect(currentAspect),
            gameplay::IsUltrawideAspect(currentAspect, kNativeAspect),
            restoration.valid,
            IsValidAspect(restoration.aspect),
            gameplay::IsUltrawideAspect(restoration.aspect, kNativeAspect),
        };
        const auto decision = camera::ResolveGameplayAspectRestorationDecision(
            decisionInput, restoration.aspect);
        if (diagnostics::Enabled())
            Log("RESTORATION_CONSUMER decision=",
                camera::GameplayAspectRestorationDecisionName(decision.decision),
                " currentAspect=", currentAspect, " currentFlags=0x", std::hex,
                static_cast<unsigned>(flags), std::dec,
                " productionAspect=", restoration.aspect,
                " productionValid=", restoration.valid ? "true" : "false",
                " productionSource=0x", std::hex, restoration.source.value, std::dec,
                " productionSequence=", restoration.observationSequence, ".");

        if (decision.decision == camera::GameplayAspectRestorationDecision::ConsumeNoWrite) {
            g_gameplayModeTransitionPending.store(false, std::memory_order_release);
            g_gameplayModeTransitionDeferralLogged.store(false, std::memory_order_release);
            Log("Gameplay mode transition applied: AspectRecalculation -> HorPlus; "
                "actual runtime aspect already available aspect=", currentAspect,
                " flags=0x", std::hex, static_cast<unsigned>(flags), std::dec, ".");
            return false;
        }

        if (decision.decision != camera::GameplayAspectRestorationDecision::Restore) {
            if (!g_gameplayModeTransitionDeferralLogged.exchange(true, std::memory_order_acq_rel))
                Log("Gameplay mode transition deferred: reason=actual_runtime_aspect_not_established.");
            return false;
        }
        const float restorationAspect = decision.restorationAspect;
        const bool writeSuccess = WriteAspectAndFlags(source, restorationAspect, flags);
        if (diagnostics::Enabled())
            Log("RESTORATION_CONSUMER result=", writeSuccess ? "RESTORE_APPLIED" : "DEFER_WRITE_FAILED",
                " restoredAspect=", restorationAspect,
                " writeSuccess=", writeSuccess ? "true" : "false", ".");
        if (!writeSuccess) {
            if (!g_gameplayModeTransitionDeferralLogged.exchange(true, std::memory_order_acq_rel))
                Log("Gameplay mode transition deferred: reason=actual_aspect_restore_not_writable.");
            return false;
        }

        g_gameplayModeTransitionPending.store(false, std::memory_order_release);
        g_gameplayModeTransitionDeferralLogged.store(false, std::memory_order_release);
        Log("Gameplay mode transition applied: AspectRecalculation -> HorPlus; restored aspect=",
            restorationAspect, " flags=0x", std::hex, static_cast<unsigned>(flags), std::dec, ".");
        return false;
    }

    bool ApplyGameplayAspectFixAtomic(std::uintptr_t source, const char* phase, float fov,
        float previousAspect, std::uint8_t previousFlags)
    {
        if (!WriteAspectAndFlags(source, kNativeAspect, 0x4)) {
            Log("AtomicReplayRefused: phase=", phase,
                " final aspect/flags fields were not writable.");
            return false;
        }
        g_state.store(ReplayState::Complete, std::memory_order_release);
        g_lastAutoRestoreSource.store(source, std::memory_order_release);
        Log("AtomicReplayApplied: phase=", phase, " source=0x", std::hex, source,
            std::dec, " fov=", fov, " aspect=", kNativeAspect,
            " flags=0x4 previousAspect=", previousAspect, " previousFlags=0x",
            std::hex, static_cast<unsigned>(previousFlags), std::dec, ".");
        return true;
    }

    void ApplyCinematicAspectStore(SafetyHookContext& context)
    {
        // Complete the replaced ten-byte store's control-flow adjustment
        // before any auxiliary processing can throw.
        context.rip += kCinematicStoreInstructionLength;
        const auto targetObject = static_cast<std::uintptr_t>(context.rax);
        if (!g_cinematicHookGate.load(std::memory_order_acquire) ||
            !g_cinematicSelectionValid.load(std::memory_order_acquire) ||
            g_stopping.load(std::memory_order_acquire)) {
            static_cast<void>(WriteAspectOnly(targetObject, kNativeAspect));
            return;
        }
        const auto policy = g_activeCinematicAspectPolicy.load(std::memory_order_acquire);
        const float autoViewportAspect = policy == CinematicAspectPolicy::Auto
            ? ReadClientViewportAspect() : 0.0f;
        // For Auto, the native store may already have written 16:9 into the
        // object by the time this boundary is observed. The resolved Auto
        // value comes from the client/display viewport, not this camera field.
        const float resolvedAspect = ResolveCinematicAspect(policy);
        const auto application = cinematics::ApplyAspectStore(targetObject, resolvedAspect,
            kNativeAspect, kAspectOffset, IsWritable, IsValidAspect);
        if (application.writable) {
            Log("Cinematic aspect store: object=0x", std::hex, targetObject, std::dec,
                " aspect=", application.aspect, " policy=", CinematicAspectPolicyName(policy),
                " source=", policy == CinematicAspectPolicy::Auto
                    ? (IsValidAspect(autoViewportAspect) ? "client-display-viewport" : "native-fallback")
                    : "configured", ".");
        } else {
            Log("Cinematic aspect store refused: target object was not writable; native store skipped.");
        }
        // The hook replaces C7 80 [disp32] [imm32]. The original store has no
        // control-flow side effects, so resuming after its ten-byte encoding
        // preserves the native lifecycle and following instructions.
    }

    bool LogCameraModeChange(std::uintptr_t source, std::uintptr_t output, float writerInputXmm0,
        float aspect, std::uint8_t flags)
    {
        if (!diagnostics::Enabled()) return false;
        float cameraWorldFov = 0.0f;
        float cameraFirstPersonFov = 0.0f;
        float outputFov = 0.0f;
        float outputAspect = 0.0f;
        float rawSourceAspect = 0.0f;
        std::uint8_t rawSourceFlags = 0;
        std::uint8_t selector = 0;
        SafeRead(source + 0x230, cameraWorldFov);
        SafeRead(source + 0x234, cameraFirstPersonFov);
        SafeRead(source + kAspectOffset, rawSourceAspect);
        SafeRead(source + kFlagsOffset, rawSourceFlags);
        SafeRead(source + 0x262, selector);
        SafeRead(output + 0x30, outputFov);
        SafeRead(output + 0x5C, outputAspect);
        LogGameplayFovSourceChange(source, writerInputXmm0, cameraWorldFov,
            cameraFirstPersonFov, aspect, flags);

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
            " writerInputXmm0=", writerInputXmm0,
            " cameraWorldFov(+0x230)=", cameraWorldFov,
            " cameraFirstPersonFov(+0x234)=", cameraFirstPersonFov,
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
        while (!WaitForWorkerStop(10)) {
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
    void LogResolutionChange()
    {
        DEVMODE display{ .dmSize = sizeof(DEVMODE) };
        const bool displayReady = EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &display) != FALSE;
        HWND window = platform::win32::FindCurrentProcessWindow();
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
        while (!WaitForWorkerStop(250)) {
            LogResolutionChange();
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

    void ReplayManualTransitionOriginal(SafetyHookContext& context)
    {
        const auto source = context.rsi;
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
        g_lastCameraSource.store(source, std::memory_order_release);
#endif
        const float fov = context.xmm0.f32[0];
        const auto previousSource = g_lastGameplayCameraSource.exchange(source, std::memory_order_acq_rel);
        const float previousFov = g_lastGameplayCameraFov.exchange(fov, std::memory_order_acq_rel);
        const auto contextChange = gameplay::EvaluateGameplayContextChange(
            previousSource, previousFov, source, fov, kDialogueContextFovJump);
        if (contextChange.InvalidatesDialogue()) {
            std::lock_guard dialogueLock(g_dialogueMutex);
            if (g_dialoguePhase != DialoguePhase::Inactive) {
                Log("Dialogue runtime invalidated by gameplay camera context change: sourceChanged=",
                    contextChange.sourceChanged, " previousSource=0x", std::hex, previousSource,
                    " source=0x", source, std::dec, " previousFOV=", previousFov,
                    " currentFOV=", fov, ". Native pass-through until a new descent.");
                ResetDialogueRuntimeState();
            }
        }
#ifdef FOV_SETTINGS_TRACE_DIAGNOSTIC
        if (std::isfinite(fov) && fov > 1.0f && fov < 179.0f)
            LogGameplayFovChange(source, fov);
#endif
        float aspect = 0.0f;
        std::uint8_t flags = 0;
        if (!SafeRead(source + kAspectOffset, aspect) || !SafeRead(source + kFlagsOffset, flags)) return;
        const auto stateBeforeObservation = g_state.load(std::memory_order_acquire);
        const bool isOwnAutoRestore = stateBeforeObservation == ReplayState::Complete &&
            std::fabs(aspect - kNativeAspect) <= 0.001f &&
            g_lastAutoRestoreSource.load(std::memory_order_acquire) == source;
        // The restore observation arrives while the replay state is still
        // AppliedConstrainPass; g_lastAutoRestoreSource is assigned only after
        // the restore write below. Use the lifecycle state as the ownership
        // marker so the fix-owned native value cannot replace the last
        // externally observed ultrawide Auto aspect.
        const bool isPendingOwnAutoRestore = stateBeforeObservation == ReplayState::AppliedConstrainPass &&
            std::fabs(aspect - kNativeAspect) <= 0.001f;
        if (!isOwnAutoRestore && !isPendingOwnAutoRestore && IsValidAspect(aspect))
        {
            UpdateGameplayAspectRestorationState(aspect, source);
        }
        else if (isPendingOwnAutoRestore)
            Log("Skipped fix-owned Auto restore in authoritative aspect cache; aspect=", aspect, ".");

        if (g_runtimeGameplayMode.load(std::memory_order_acquire) ==
                config::GameplayMode::AspectRecalculation &&
            g_coordinator.load(std::memory_order_acquire) == CoordinatorState::Gameplay &&
            !isOwnAutoRestore && !isPendingOwnAutoRestore &&
            IsValidFovValue(fov) && IsValidAspect(aspect)) {
            camera::CameraFovObservation observation{};
            observation.boundary = camera::FovObservationBoundary::CameraWriter;
            observation.inputFov = {
                fov, camera::FovSpace::Native,
                camera::FovProvenance::NativeRegisterInput, true };
            observation.resultFov = {
                fov, camera::FovSpace::Native,
                camera::FovProvenance::PassThroughResult, true };
            observation.aspect = { aspect, camera::FovProvenance::ResolvedAspect, true };
            observation.writerSource = { source, source != 0 };
            observation.writerFlags = flags;
            observation.writerFlagsValid = true;
            const auto committed = g_fovObservationStore.Publish(observation);
            g_gameplayBaselineStore.Project(committed, true);
        }
#ifdef GAMEPLAY_ONE_SHOT_CINEMATIC_TRIGGER
        if (g_oneShotCinematicTriggerArmed.load(std::memory_order_acquire) &&
            g_state.load(std::memory_order_relaxed) == ReplayState::WaitingForAutomaticUpdate &&
            gameplay::IsConstrainedUltrawideAspect(aspect, flags, kNativeAspect)) {
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
            gameplay::IsConstrainedUltrawideAspect(aspect, flags, kNativeAspect);
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

        if (gameplay::IsConstrainedUltrawideAspect(aspect, flags, kNativeAspect)) {
            // Preserve constrained aspect ratio for a real 21:9 output. The FOV source
            // is already correct; only the camera aspect state needs normalization.
            if (WriteAspectAndFlags(source, kNativeAspect, 0x5))
                Log("Normalized 21:9 camera aspect while preserving constrained aspect ratio.");
            logPost();
            return;
        }

        auto state = g_state.load(std::memory_order_relaxed);
        if (state == ReplayState::Complete && gameplay::IsUltrawideAspect(aspect, kNativeAspect) && flags == 0x4) {
            // Cutscenes can rebuild the gameplay camera and restore the same broken
            // Auto state seen during startup. Arm the proven two-pass transition again.
            {
                std::lock_guard dialogueLock(g_dialogueMutex);
                ResetDialogueRuntimeState();
            }
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
            if (!gameplay::IsUltrawideAspect(aspect, kNativeAspect) || flags != 0x4) {
                logPost();
                return;
            }
            ApplyGameplayAspectFixAtomic(source, "GameplayDetected", fov, aspect, flags);
            logPost();
            return;
        }
        logPost();
    }

#ifdef POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_TEST
    bool TryDeferredGameplayReplay(SafetyHookContext& context)
    {
        if (!g_deferredGameplayReplayPending.load(std::memory_order_acquire)) return false;

        const auto source = static_cast<std::uintptr_t>(context.rsi);
        const float fov = context.xmm0.f32[0];
        const float targetFov = g_exitTargetFov.load(std::memory_order_acquire);
        float aspect = 0.0f;
        std::uint8_t flags = 0;
        if (!SafeRead(source + kAspectOffset, aspect) || !SafeRead(source + kFlagsOffset, flags)) {
            CancelDeferredGameplayReplay("camera-state-unreadable", source, fov, aspect, flags);
            return true;
        }

        bool dialogueActive = false;
        {
            std::lock_guard dialogueLock(g_dialogueMutex);
            dialogueActive = g_dialoguePhase != DialoguePhase::Inactive;
        }
        if (g_coordinator.load(std::memory_order_acquire) != CoordinatorState::Gameplay) {
            CancelDeferredGameplayReplay("coordinator-not-gameplay", source, fov, aspect, flags);
            return true;
        }
        if (dialogueActive) {
            CancelDeferredGameplayReplay("dialogue-active", source, fov, aspect, flags);
            return true;
        }

        const auto expectedSource = g_deferredGameplaySource.load(std::memory_order_acquire);
        if (expectedSource != 0 && expectedSource != source) {
            CancelDeferredGameplayReplay("source-changed", source, fov, aspect, flags);
            return true;
        }
        if (!std::isfinite(fov) || !std::isfinite(targetFov) ||
            std::fabs(fov - targetFov) > kRecoveryEpsilon) {
            CancelDeferredGameplayReplay("fov-left-target-tolerance", source, fov, aspect, flags);
            return true;
        }
        if (!gameplay::IsUltrawideAspect(aspect, kNativeAspect) || flags != 0x4) {
            CancelDeferredGameplayReplay("gameplay-replay-not-applicable", source, fov, aspect, flags);
            return true;
        }

        const float previousFov = g_deferredGameplayLastFov.load(std::memory_order_acquire);
        if (std::isfinite(previousFov) && std::fabs(fov - previousFov) > kDialogueTransformEpsilon) {
            CancelDeferredGameplayReplay("fov-not-stable", source, fov, aspect, flags);
            return true;
        }
        if (expectedSource == 0)
            g_deferredGameplaySource.store(source, std::memory_order_release);
        g_deferredGameplayLastFov.store(fov, std::memory_order_release);

        auto samples = g_deferredGameplayStableSamples.load(std::memory_order_acquire);
        if (samples < kDeferredGameplayStableSamples) {
            samples = g_deferredGameplayStableSamples.fetch_add(1,
                std::memory_order_acq_rel) + 1;
            Log("StableSample ", samples, "/3: source=0x", std::hex, source, std::dec,
                " fov=", fov, " targetFov=", targetFov, " aspect=", aspect,
                " flags=0x", std::hex, static_cast<unsigned>(flags), std::dec,
                " elapsedUs=", (NowSteadyNs() - g_postExitTraceStartNs.load(std::memory_order_acquire)) / 1000, ".");
            if (samples < kDeferredGameplayStableSamples) return true;

            const auto stableSince = NowSteadyNs();
            g_deferredGameplayStableSinceNs.store(stableSince, std::memory_order_release);
            Log("StableStateConfirmed: source=0x", std::hex, source, std::dec,
                " fov=", fov, " aspect=", aspect, " flags=0x", std::hex,
                static_cast<unsigned>(flags), std::dec, ".");
        }
        const auto stableSince = g_deferredGameplayStableSinceNs.load(std::memory_order_acquire);
        if (NowSteadyNs() - stableSince < kDeferredGameplayDelayNs) return true;

        bool expectedPending = true;
        if (!g_deferredGameplayReplayPending.compare_exchange_strong(expectedPending, false,
            std::memory_order_acq_rel)) return true;
        g_deferredGameplayStableSamples.store(0, std::memory_order_release);
        Log("DeferredReplayApplied: source=0x", std::hex, source, std::dec,
            " fov=", fov, " aspect=", aspect, " flags=0x", std::hex,
            static_cast<unsigned>(flags), std::dec, " delayMs=",
            POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_DELAY_MS,
#ifdef POST_CINEMATIC_GAMEPLAY_REPLAY_ATOMICITY_TEST
            " mode=atomic.");
#else
            " mode=legacy.");
#endif
#ifdef POST_CINEMATIC_GAMEPLAY_REPLAY_ATOMICITY_TEST
        if (WriteAspectAndFlags(source, kNativeAspect, 0x4)) {
            g_state.store(ReplayState::Complete, std::memory_order_release);
            g_lastAutoRestoreSource.store(source, std::memory_order_release);
            Log("AtomicReplayApplied: source=0x", std::hex, source, std::dec,
                " fov=", fov, " aspect=", kNativeAspect, " flags=0x4 delayMs=",
                POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_DELAY_MS, ".");
        } else {
            Log("AtomicReplayRefused: final aspect/flags fields were not writable.");
        }
#else
        g_state.store(ReplayState::WaitingForAutomaticUpdate, std::memory_order_release);
        ReplayManualTransitionOriginal(context);
#endif
        return true;
    }
#endif

    bool ValidateEnterBoundary(std::uint8_t* match, std::uint8_t*& callsite)
    {
        hooks::ExecutableSpan textSpan{};
        if (!hooks::FindSectionSpan(g_executable, ".text", textSpan)) return false;
        ZydisDecodedInstruction instruction{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
        if (!hooks::validation::DecodeInstruction(match, instruction, operands, textSpan) ||
            instruction.mnemonic != ZYDIS_MNEMONIC_MOVSS || instruction.length != 8 ||
            instruction.operand_count_visible < 2 ||
            operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER ||
            operands[0].reg.value != ZYDIS_REGISTER_XMM0 ||
            operands[1].type != ZYDIS_OPERAND_TYPE_MEMORY ||
            operands[1].mem.base != ZYDIS_REGISTER_RIP) return false;
        callsite = match + 8;
        return hooks::validation::IsCallRel32(callsite, textSpan) &&
            hooks::validation::ContainsBytes(match, 96, EnterVcallPair, sizeof(EnterVcallPair), textSpan);
    }

    bool ValidateExitBoundary(std::uint8_t* match, std::uint8_t*& callsite)
    {
        hooks::ExecutableSpan textSpan{};
        if (!hooks::FindSectionSpan(g_executable, ".text", textSpan)) return false;
        ZydisDecodedInstruction instruction{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
        if (!hooks::validation::DecodeInstruction(match, instruction, operands, textSpan) ||
            instruction.mnemonic != ZYDIS_MNEMONIC_MOVSS || instruction.length != 5 ||
            instruction.operand_count_visible < 2 ||
            operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER ||
            operands[0].reg.value != ZYDIS_REGISTER_XMM0 ||
            operands[1].type != ZYDIS_OPERAND_TYPE_MEMORY ||
            operands[1].mem.base != ZYDIS_REGISTER_RDI ||
            !operands[1].mem.disp.has_displacement || operands[1].mem.disp.value != 0x38) return false;
        callsite = match + 5;
        return hooks::validation::IsCallRel32(callsite, textSpan) &&
            hooks::validation::ContainsBytes(match, 96, ExitVcallPair, sizeof(ExitVcallPair), textSpan);
    }

    bool ValidateIndexedExitBoundary(std::uint8_t* match, std::uint8_t*& callsite)
    {
        hooks::ExecutableSpan textSpan{};
        if (!hooks::FindSectionSpan(g_executable, ".text", textSpan)) return false;
        constexpr std::size_t kMovssOffset = 4;
        constexpr std::size_t kCallOffset = 10;
        ZydisDecodedInstruction instruction{};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
        if (!hooks::validation::DecodeInstruction(match + kMovssOffset, instruction, operands, textSpan) ||
            instruction.mnemonic != ZYDIS_MNEMONIC_MOVSS || instruction.length != 6 ||
            instruction.operand_count_visible < 2 ||
            operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER ||
            operands[0].reg.value != ZYDIS_REGISTER_XMM0 ||
            operands[1].type != ZYDIS_OPERAND_TYPE_MEMORY ||
            operands[1].mem.base != ZYDIS_REGISTER_RBX ||
            operands[1].mem.index != ZYDIS_REGISTER_RAX ||
            operands[1].mem.scale != 4 ||
            !operands[1].mem.disp.has_displacement ||
            operands[1].mem.disp.value != 0x38) return false;
        callsite = match + kCallOffset;
        return hooks::validation::IsCallRel32(callsite, textSpan) &&
            hooks::validation::ContainsBytes(match, 96, ExitVcallPair, sizeof(ExitVcallPair), textSpan);
    }

    bool ResolveCinematicAspectStore(std::uint8_t*& store)
    {
        const auto resolution = cinematics::ResolveAspectStore(g_executable, CinematicAspectSetter,
            kAspectOffset, kCinematicStorePrefix, sizeof(kCinematicStorePrefix),
            kCinematicOriginalImmediate, kCinematicImmediateOffset,
            sizeof(kCinematicOriginalImmediate), kCinematicStoreInstructionLength, 0x3FE38E39);
        if (!resolution.imageValid) return false;
        if (resolution.matches != 1) {
            Log("Cinematic aspect signature rejected: matches=", resolution.matches, ".");
            return false;
        }
        if (!resolution.store) return false;
        store = resolution.store;
        Log("Cinematic aspect signature validated at RVA=0x", std::hex,
            reinterpret_cast<std::uintptr_t>(store) - reinterpret_cast<std::uintptr_t>(g_executable), std::dec, ".");
        return true;
    }

    bool ResolveCinematicFovCallsites(std::uint8_t*& enter, std::uint8_t*& exit)
    {
        const auto enterMatches = hooks::FindAll(g_executable, CinematicEnter);
        auto exitMatches = hooks::FindAll(g_executable, CinematicExit);
        bool indexedExit = false;
        Log("Cinematic FOV resolver: ENTER matches=", enterMatches.size(),
            " EXIT legacy matches=", exitMatches.size(), ".");
        if (exitMatches.empty()) {
            exitMatches = hooks::FindAll(g_executable, CinematicExitIndexed);
            indexedExit = true;
            Log("Cinematic FOV resolver: EXIT indexed fallback=ACTIVE matches=", exitMatches.size(), ".");
        } else {
            Log("Cinematic FOV resolver: EXIT indexed fallback=NOT_USED.");
        }
        if (enterMatches.size() != 1 || exitMatches.size() != 1) {
            Log("Cinematic FOV signatures rejected: enterMatches=", enterMatches.size(),
                " exitMatches=", exitMatches.size(), ".");
            return false;
        }
        const bool enterValid = ValidateEnterBoundary(enterMatches.front(), enter);
        Log("Cinematic FOV ENTER structural/decode/operand validation=",
            enterValid ? "PASS" : "FAIL", ".");
        if (!enterValid) {
            return false;
        }
        const bool exitValid = indexedExit
            ? ValidateIndexedExitBoundary(exitMatches.front(), exit)
            : ValidateExitBoundary(exitMatches.front(), exit);
        Log("Cinematic FOV EXIT structural/decode/operand validation=",
            exitValid ? "PASS" : "FAIL", " mode=", indexedExit ? "indexed" : "legacy", ".");
        if (!exitValid) {
            return false;
        }
        hooks::ExecutableSpan textSpan{};
        if (!hooks::FindSectionSpan(g_executable, ".text", textSpan)) return false;
        const auto enterTarget = hooks::validation::ResolveRel32CallTarget(enter, textSpan);
        const auto exitTarget = hooks::validation::ResolveRel32CallTarget(exit, textSpan);
        const bool enterTargetValid = enterTarget != nullptr &&
            hooks::validation::IsExecutable(reinterpret_cast<std::uintptr_t>(enterTarget));
        const bool exitTargetValid = exitTarget != nullptr &&
            hooks::validation::IsExecutable(reinterpret_cast<std::uintptr_t>(exitTarget));
        const bool sameTarget = enterTarget != nullptr && enterTarget == exitTarget;
        Log("Cinematic FOV call-target validation: ENTER=", enterTargetValid ? "PASS" : "FAIL",
            " EXIT=", exitTargetValid ? "PASS" : "FAIL",
            " sameTarget=", sameTarget ? "PASS" : "FAIL", ".");
        if (!enterTargetValid || !exitTargetValid || !sameTarget) {
            Log("Cinematic FOV consumer target validation failed: ENTER=0x", std::hex,
                reinterpret_cast<std::uintptr_t>(enterTarget), " EXIT=0x",
                reinterpret_cast<std::uintptr_t>(exitTarget), std::dec, ".");
            return false;
        }
        Log("Cinematic FOV signatures validated at ENTER RVA=0x", std::hex,
            reinterpret_cast<std::uintptr_t>(enter) - reinterpret_cast<std::uintptr_t>(g_executable),
            " EXIT RVA=0x", reinterpret_cast<std::uintptr_t>(exit) - reinterpret_cast<std::uintptr_t>(g_executable),
            " consumer=0x", reinterpret_cast<std::uintptr_t>(enterTarget), std::dec, ".");
        return true;
    }

    bool InstallCinematicAspect(std::uint8_t* store)
    {
        g_cinematicAspectStore = store;
        if (!hooks::validation::IsExecutable(reinterpret_cast<std::uintptr_t>(g_cinematicAspectStore)) ||
            std::memcmp(g_cinematicAspectStore, kCinematicStorePrefix, sizeof(kCinematicStorePrefix)) != 0 ||
            std::memcmp(g_cinematicAspectStore + kCinematicImmediateOffset,
                kCinematicOriginalImmediate, sizeof(kCinematicOriginalImmediate)) != 0) return false;
        std::memcpy(g_cinematicOriginalImmediate, g_cinematicAspectStore + kCinematicImmediateOffset,
            sizeof(g_cinematicOriginalImmediate));
        auto hook = safetyhook::MidHook::create(
            g_cinematicAspectStore, SafeMidHookEntry<&ApplyCinematicAspectStore>,
            safetyhook::MidHook::StartDisabled);
        if (!hook) return false;
        g_cinematicAspectStoreHook = std::move(*hook);
        g_cinematicAspectPatched = true;
        return true;
    }

    void RestoreCinematicAspect() noexcept
    {
        static_cast<void>(g_cinematicAspectStoreHook.disable());
        g_cinematicAspectStoreHook.reset();
        g_cinematicAspectPatched = false;
    }

    void TraceCinematicEnter(SafetyHookContext& context)
    {
        if (!g_cinematicHookGate.load(std::memory_order_acquire) ||
            g_stopping.load(std::memory_order_acquire)) return;
#ifdef POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_TEST
        CancelDeferredGameplayReplay("new-cinematic-enter");
#endif
        ResetAtomicExitHandoff("new-cinematic-enter");
        ResetPostCinematicDialogueExclusion("new-cinematic-enter");
        g_cinematicTransformedFov.store(std::numeric_limits<float>::quiet_NaN(),
            std::memory_order_release);
        cinematics::CinematicSelectionSnapshot selection{};
        if (!cinematics::TryCaptureCinematicSelection(
                g_cinematicSelectionValid.load(std::memory_order_acquire),
                g_runtimeCinematicPolicy.load(std::memory_order_acquire),
                g_runtimeCinematicFovMode.load(std::memory_order_acquire),
                g_cinematicSelectionGeneration.fetch_add(1, std::memory_order_acq_rel) + 1,
                selection)) return;
        g_cinematicFovApplied.store(true, std::memory_order_release);
        g_activeCinematicAspectPolicy.store(selection.aspectPolicy, std::memory_order_relaxed);
        g_activeCinematicFovMode.store(selection.fovMode, std::memory_order_relaxed);
        g_cinematicSelectionValid.store(true, std::memory_order_release);
        const auto policy = selection.aspectPolicy;
        const bool cinematicFovEnabled = cinematics::OverrideEnabled(selection.aspectPolicy);
        const float before = context.xmm0.f32[0];
        const float aspect = ResolveCinematicAspect();
#ifdef HORPLUS_CINEMATIC_WRITER_TRACE_DIAGNOSTIC
        g_cinematicEnterAspect.store(aspect, std::memory_order_release);
#endif
        g_matchGameplayEnterNativeFov.store(before, std::memory_order_release);
        const auto gameplayBaseline = g_gameplayBaselineStore.Read();
        const bool gameplayBaselineUsable = camera::IsUsableGameplayBaseline(gameplayBaseline);
        const float gameplayNative = gameplayBaselineUsable
            ? gameplayBaseline.nativeFov.value
            : std::numeric_limits<float>::quiet_NaN();
        const float gameplayAspect = gameplayBaselineUsable
            ? gameplayBaseline.aspect
            : std::numeric_limits<float>::quiet_NaN();
        const float gameplayHorPlus = gameplayBaselineUsable &&
                gameplayBaseline.horPlusFov.valid
            ? gameplayBaseline.horPlusFov.value
            : std::numeric_limits<float>::quiet_NaN();
        const bool gameplayPairValid = gameplayBaselineUsable &&
            gameplayBaseline.horPlusFov.valid;
        g_matchGameplayPreEnterNativeFov.store(gameplayNative, std::memory_order_release);
        g_matchGameplayPreEnterAspect.store(gameplayAspect, std::memory_order_release);
        g_matchGameplayPreEnterHorPlusFov.store(gameplayHorPlus, std::memory_order_release);
        g_matchGameplayPreEnterPairValid.store(gameplayPairValid, std::memory_order_release);
        float after = before;
        const auto baselineSelection = cinematics::SelectCinematicBaseline(
            selection.fovMode == config::CinematicFovMode::GameplayHorPlus,
            before, gameplayBaseline);
        const float targetBaseline = baselineSelection.targetBaselineFov;
        const bool transformed = cinematicFovEnabled &&
            cinematics::TryTransformCinematicFov(before, before, targetBaseline,
                aspect, kNativeAspect, after);
        if (transformed) {
            context.xmm0.f32[0] = after;
            g_cinematicTransformedFov.store(after, std::memory_order_release);
        }
        if (IsValidFovValue(before) && IsValidFovValue(context.xmm0.f32[0]) &&
            IsValidAspect(aspect)) {
            camera::CameraFovObservation observation{};
            observation.boundary = camera::FovObservationBoundary::CinematicEnter;
            observation.inputFov = {
                before, camera::FovSpace::Native,
                camera::FovProvenance::NativeRegisterInput, true };
            observation.resultFov = {
                context.xmm0.f32[0],
                transformed ? camera::FovSpace::Transformed : camera::FovSpace::Native,
                transformed ? camera::FovProvenance::ModTransformResult :
                    camera::FovProvenance::PassThroughResult,
                true };
            observation.aspect = { aspect, camera::FovProvenance::ResolvedAspect, true };
            const auto committed = g_fovObservationStore.Publish(observation);
            if (diagnostics::Enabled()) {
                Log("FOV_OBSERVATION boundary=", camera::FovObservationBoundaryName(committed.boundary),
                    " input=", committed.inputFov.value,
                    " inputSpace=", camera::FovSpaceName(committed.inputFov.space),
                    " inputProvenance=", camera::FovProvenanceName(committed.inputFov.provenance),
                    " result=", committed.resultFov.value,
                    " resultSpace=", camera::FovSpaceName(committed.resultFov.space),
                    " resultProvenance=", camera::FovProvenanceName(committed.resultFov.provenance),
                    " aspect=", committed.aspect.value,
                    " sequence=", committed.publicationSequence, ".");
            }
        }
        g_coordinator.store(CoordinatorState::CinematicActive, std::memory_order_release);
        Log("Global cinematic ENTER: aspect=", aspect, " authoredFov=", before,
            " transformedFov=", context.xmm0.f32[0],
            " aspectPolicy=", CinematicAspectPolicyName(policy), ".",
            " Gameplay replay suppressed.");
        if (diagnostics::Enabled()) {
            Log("CINEMATIC_BASELINE_SELECTION source=",
                baselineSelection.source == cinematics::CinematicBaselineSource::GameplayBaselineNative
                    ? "GameplayBaselineNative" : "AuthoredEnterFallback",
                " mode=", config::CinematicFovModeName(
                    selection.fovMode),
                " baselineValid=", gameplayBaselineUsable ? "true" : "false",
                " native=", gameplayBaseline.nativeFov.value,
                " nativeValid=", gameplayBaseline.nativeFov.valid ? "true" : "false",
                " horPlus=", gameplayBaseline.horPlusFov.value,
                " horPlusValid=", gameplayBaseline.horPlusFov.valid ? "true" : "false",
                " gameplayAspect=", gameplayBaseline.aspect,
                " sequence=", gameplayBaseline.observationSequence,
                " authoredEnter=", before,
                " targetBaseline=", targetBaseline,
                " effectiveCinematicAspect=", aspect,
                " transformed=", transformed ? "true" : "false", ".");
        }
    }

    void TraceCinematicExit(SafetyHookContext& context)
    {
        if (!g_cinematicHookGate.load(std::memory_order_acquire) ||
            g_stopping.load(std::memory_order_acquire)) return;
#ifdef POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_TEST
        CancelDeferredGameplayReplay("new-cinematic-exit");
#endif
        ResetAtomicExitHandoff("new-cinematic-exit");
        g_exitTargetFov.store(context.xmm0.f32[0], std::memory_order_release);
        ArmPostCinematicDialogueExclusion();
#ifdef POST_EXIT_TRACE_STATE
        g_postExitTraceWriterCount.store(0, std::memory_order_release);
        g_postExitTraceSequence.fetch_add(1, std::memory_order_acq_rel);
        g_postExitTraceStartNs.store(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count(), std::memory_order_release);
        g_postExitTraceArmed.store(true, std::memory_order_release);
#endif
#ifdef POST_EXIT_DELAYED_ASPECT_DIAGNOSTIC
        g_delayedAspectDiagnosticApplied.store(false, std::memory_order_release);
#endif
        g_cinematicFovApplied.store(false, std::memory_order_release);
        g_cinematicTransformedFov.store(std::numeric_limits<float>::quiet_NaN(),
            std::memory_order_release);
#ifdef HORPLUS_CINEMATIC_WRITER_TRACE_DIAGNOSTIC
        g_cinematicEnterAspect.store(std::numeric_limits<float>::quiet_NaN(),
            std::memory_order_release);
#endif
        g_matchGameplayEnterNativeFov.store(std::numeric_limits<float>::quiet_NaN(),
            std::memory_order_release);
        g_matchGameplayPreEnterPairValid.store(false, std::memory_order_release);
        g_cinematicSelectionValid.store(false, std::memory_order_release);
        const auto exitTransition = gameplay::ResolveCinematicExitTransition(
            g_gameplayAvailable.load(std::memory_order_acquire),
            g_runtimeGameplayMode.load(std::memory_order_acquire));
        g_coordinator.store(exitTransition.nextState, std::memory_order_release);
        if (exitTransition.armGameplayHandoff) {
            ArmAtomicExitHandoff();
        } else {
            std::lock_guard dialogueLock(g_dialogueMutex);
            ResetDialogueRuntimeState();
        }
        Log("Global cinematic EXIT: nativeTargetFov=", context.xmm0.f32[0],
#ifdef POST_EXIT_TRACE_STATE
            " postExitTrace=armed sequence=", g_postExitTraceSequence.load(std::memory_order_acquire),
#else
            " postExitTrace=disabled",
#endif
            exitTransition.armGameplayHandoff
                ? ". Gameplay replay suppressed until native recovery."
                : ". Gameplay recovery not owned; coordinator returned to Gameplay.");
    }

    bool InstallCinematicAspectComponent()
    {
        std::uint8_t* cinematicAspectStore = nullptr;
        if (!ResolveCinematicAspectStore(cinematicAspectStore) ||
            !InstallCinematicAspect(cinematicAspectStore)) return false;
        Log("Global cinematic aspect store hooked: policy=",
            CinematicAspectPolicyName(g_config.cinematicAspectPolicy), ".");
        return true;
    }

    bool InstallCinematicFovComponent()
    {
        std::uint8_t* cinematicEnter = nullptr;
        std::uint8_t* cinematicExit = nullptr;
        if (!ResolveCinematicFovCallsites(cinematicEnter, cinematicExit)) return false;
        auto enterHook = safetyhook::MidHook::create(
            cinematicEnter, SafeMidHookEntry<&TraceCinematicEnter>, safetyhook::MidHook::StartDisabled);
        auto exitHook = safetyhook::MidHook::create(
            cinematicExit, SafeMidHookEntry<&TraceCinematicExit>, safetyhook::MidHook::StartDisabled);
        if (!enterHook || !exitHook) return false;
        g_cinematicEnterHook = std::move(*enterHook);
        g_cinematicExitHook = std::move(*exitHook);
        if (!g_cinematicEnterHook || !g_cinematicExitHook) return false;
        return true;
    }

    bool CommitCinematicComponents()
    {
        if (!g_cinematicAspectStoreHook.enable() ||
            !g_cinematicEnterHook.enable() || !g_cinematicExitHook.enable()) {
            static_cast<void>(g_cinematicAspectStoreHook.disable());
            static_cast<void>(g_cinematicEnterHook.disable());
            static_cast<void>(g_cinematicExitHook.disable());
            return false;
        }
        g_cinematicHookGate.store(true, std::memory_order_release);
        return true;
    }

    bool CommitCinematicObservationOnly()
    {
        if (!g_cinematicEnterHook.enable() || !g_cinematicExitHook.enable()) return false;
        g_cinematicHookGate.store(true, std::memory_order_release);
        return true;
    }

    void RollbackCinematicAspectComponent() noexcept
    {
        RestoreCinematicAspect();
    }

    void RollbackCinematicFovComponent() noexcept
    {
        g_cinematicHookGate.store(false, std::memory_order_release);
        static_cast<void>(g_cinematicExitHook.disable());
        static_cast<void>(g_cinematicEnterHook.disable());
        g_cinematicExitHook.reset();
        g_cinematicEnterHook.reset();
    }

#ifdef DIALOGUE_BOUNDARY_DIAGNOSTIC
    std::atomic<std::uint64_t> g_dialogueDiagnosticCallbacks{};
    std::atomic<std::uint64_t> g_dialogueDiagnosticReturn1{};
    std::atomic<std::uint64_t> g_dialogueDiagnosticReturn2{};
    std::atomic<std::uint64_t> g_dialogueDiagnosticReturn3{};
    std::atomic<std::uint64_t> g_dialogueDiagnosticReturn4{};
    std::atomic<std::uint64_t> g_dialogueDiagnosticReturn5{};

    void LogDialogueDiagnosticReturn(const char* reason, SafetyHookContext& context)
    {
        static_cast<void>(context);
        if (std::strcmp(reason, "invalid-input") == 0) {
            g_dialogueDiagnosticReturn1.fetch_add(1, std::memory_order_relaxed);
        } else if (std::strcmp(reason, "non-gameplay") == 0) {
            g_dialogueDiagnosticReturn2.fetch_add(1, std::memory_order_relaxed);
        } else if (std::strcmp(reason, "native-policy") == 0) {
            g_dialogueDiagnosticReturn3.fetch_add(1, std::memory_order_relaxed);
        } else if (std::strcmp(reason, "processed") == 0) {
            g_dialogueDiagnosticReturn4.fetch_add(1, std::memory_order_relaxed);
        } else if (std::strcmp(reason, "post-cinematic-recovery") == 0) {
            g_dialogueDiagnosticReturn5.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void LogDialogueDiagnosticSummary()
    {
        Log("DIALOGUE_CALLBACK_SUMMARY total=", g_dialogueDiagnosticCallbacks.load(std::memory_order_relaxed),
            " return1=", g_dialogueDiagnosticReturn1.load(std::memory_order_relaxed),
            " return2=", g_dialogueDiagnosticReturn2.load(std::memory_order_relaxed),
            " return3=", g_dialogueDiagnosticReturn3.load(std::memory_order_relaxed),
            " return4=", g_dialogueDiagnosticReturn4.load(std::memory_order_relaxed),
            " return5=", g_dialogueDiagnosticReturn5.load(std::memory_order_relaxed), ".");
    }
#endif

#ifdef DIALOGUE_DISCOVERY_DIAGNOSTIC
    void TraceDialogueDiscovery(SafetyHookContext& context);
#endif

#ifdef DIALOGUE_RECOVERY_ENDPOINT_DIAGNOSTIC
    constexpr std::uint32_t kDialogueRecoveryTraceMaxSamples = 256;

    struct DialogueRecoveryEndpointTrace
    {
        bool active{};
        std::uintptr_t source{};
        std::uint32_t samples{};
        bool previousValid{};
        bool readable{};
        DialoguePhase phase{DialoguePhase::Inactive};
        float currentFov{std::numeric_limits<float>::quiet_NaN()};
        float state28{std::numeric_limits<float>::quiet_NaN()};
        float targetFov{std::numeric_limits<float>::quiet_NaN()};
        float pairedFov{std::numeric_limits<float>::quiet_NaN()};
        bool wouldRearm{};
    };

    thread_local DialogueRecoveryEndpointTrace g_dialogueRecoveryEndpointTrace{};

    void ArmDialogueRecoveryEndpointTrace(std::uintptr_t source)
    {
        g_dialogueRecoveryEndpointTrace = {};
        g_dialogueRecoveryEndpointTrace.active = source != 0;
        g_dialogueRecoveryEndpointTrace.source = source;
        if (g_dialogueRecoveryEndpointTrace.active)
            Log("DIALOGUE_RECOVERY_TRACE start source=0x", std::hex, source, std::dec, ".");
    }

    void ObserveDialogueRecoveryEndpointTrace(SafetyHookContext& context,
        DialoguePhase phase, float currentFov, float baseline, float previous,
        std::uint32_t stableSamples, bool wouldRearm)
    {
        auto& trace = g_dialogueRecoveryEndpointTrace;
        if (!trace.active) return;

        const auto source = static_cast<std::uintptr_t>(context.rsi);
        if (!source || source != trace.source) {
            Log("DIALOGUE_RECOVERY_TRACE stop reason=source-change samples=", trace.samples, ".");
            trace = {};
            return;
        }
        if (trace.samples++ >= kDialogueRecoveryTraceMaxSamples) {
            Log("DIALOGUE_RECOVERY_TRACE stop reason=sample-limit samples=", trace.samples, ".");
            trace = {};
            return;
        }

        float state28 = std::numeric_limits<float>::quiet_NaN();
        float targetFov = std::numeric_limits<float>::quiet_NaN();
        float pairedFov = std::numeric_limits<float>::quiet_NaN();
        const bool readable = SafeRead(source + 0x28, state28) &&
            SafeRead(source + 0x2C, targetFov) && SafeRead(source + 0x30, pairedFov);
        const float comparisonPrevious = std::isfinite(previous) ? previous : trace.currentFov;
        const auto direction = !std::isfinite(comparisonPrevious) ||
            !std::isfinite(currentFov) ? "unknown" :
            currentFov > comparisonPrevious + kDialogueTransformEpsilon ? "up" :
            currentFov < comparisonPrevious - kDialogueTransformEpsilon ? "down" : "stable";
        const auto floatChanged = [](float a, float b) {
            return std::isfinite(a) != std::isfinite(b) ||
                (std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) > 0.001f);
        };
        const bool changed = !trace.previousValid || trace.phase != phase ||
            trace.readable != readable || floatChanged(trace.currentFov, currentFov) ||
            floatChanged(trace.state28, state28) || floatChanged(trace.targetFov, targetFov) ||
            floatChanged(trace.pairedFov, pairedFov) || trace.wouldRearm != wouldRearm;
        if (changed) {
            const float deltaTarget = readable && std::isfinite(targetFov)
                ? currentFov - targetFov : std::numeric_limits<float>::quiet_NaN();
            const float deltaBaseline = std::isfinite(baseline)
                ? currentFov - baseline : std::numeric_limits<float>::quiet_NaN();
            Log("DIALOGUE_RECOVERY_TRACE phase=", PhaseName(phase),
                " current=", currentFov, " state28=", state28,
                " target=", targetFov, " paired=", pairedFov,
                " baseline=", baseline, " previous=", comparisonPrevious,
                " deltaTarget=", deltaTarget, " deltaBaseline=", deltaBaseline,
                " direction=", direction, " stableCount=", stableSamples,
                " wouldRearmCurrentPredicate=", wouldRearm ? 1 : 0,
                " readable=", readable ? 1 : 0, " samples=", trace.samples, ".");
        }
        trace.previousValid = true;
        trace.readable = readable;
        trace.phase = phase;
        trace.currentFov = currentFov;
        trace.state28 = state28;
        trace.targetFov = targetFov;
        trace.pairedFov = pairedFov;
        trace.wouldRearm = wouldRearm;
    }
#endif

    void TraceDialogueBoundary(SafetyHookContext& context)
    {
#ifdef DIALOGUE_DISCOVERY_DIAGNOSTIC
        TraceDialogueDiscovery(context);
#endif
#ifdef DIALOGUE_BOUNDARY_DIAGNOSTIC
        g_dialogueDiagnosticCallbacks.fetch_add(1, std::memory_order_relaxed);
#endif
        const float incoming = context.xmm6.f32[0];
        if (!std::isfinite(incoming) || incoming <= 1.0f || incoming >= 179.0f) {
            std::lock_guard lock(g_dialogueMutex);
            if (g_dialoguePhase == DialoguePhase::Candidate) {
                Log("Dialogue candidate cancelled: reason=invalid-sample.");
                ResetDialogueRuntimeState();
            }
#ifdef DIALOGUE_BOUNDARY_DIAGNOSTIC
            LogDialogueDiagnosticReturn("invalid-input", context);
#endif
            return;
        }

        std::lock_guard lock(g_dialogueMutex);
#ifdef CAMERA_STATE_SNAPSHOT_DIAGNOSTIC
        const auto snapshotDialogueSource = static_cast<std::uintptr_t>(context.rsi);
        float snapshotDialogueTarget = std::numeric_limits<float>::quiet_NaN();
        const bool snapshotDialogueTargetReadable = snapshotDialogueSource != 0 &&
            SafeRead(snapshotDialogueSource + 0x2C, snapshotDialogueTarget) &&
            IsValidFovValue(snapshotDialogueTarget);
        g_runtime.snapshotDialogueSource.store(snapshotDialogueSource,
            std::memory_order_release);
        g_runtime.snapshotDialogueTarget.store(snapshotDialogueTarget,
            std::memory_order_release);
        g_runtime.snapshotDialogueTargetValid.store(snapshotDialogueTargetReadable,
            std::memory_order_release);
#endif
#ifdef DIALOGUE_RECOVERY_ENDPOINT_DIAGNOSTIC
        const auto recoveryPhase = g_dialoguePhase;
        const auto stableSamples = g_dialogueRecoveryRearm.StableSamples();
        const auto recoverySource = static_cast<std::uintptr_t>(context.rsi);
        float nativeTarget = std::numeric_limits<float>::quiet_NaN();
        const bool nativeTargetReadable = recoverySource != 0 &&
            SafeRead(recoverySource + 0x2C, nativeTarget) && IsValidFovValue(nativeTarget);
        const bool recoverySampleStable = recoveryPhase == DialoguePhase::RearmPending &&
            nativeTargetReadable && std::isfinite(g_dialoguePrevious) &&
            std::fabs(incoming - nativeTarget) <= kRecoveryEpsilon &&
            std::fabs(incoming - g_dialoguePrevious) <= kDialogueTransformEpsilon;
        const bool wouldRearmCurrentPredicate = recoverySampleStable &&
            stableSamples + 1 >= kDialogueRecoveryStableSamples;
        ObserveDialogueRecoveryEndpointTrace(context, recoveryPhase, incoming,
            g_dialogueBaseline, g_dialoguePrevious, stableSamples,
            wouldRearmCurrentPredicate);
#endif
        if (g_postCinematicDialogueExclusion.IsActive()) {
            ResetDialogueRuntimeState();
#ifdef DIALOGUE_BOUNDARY_DIAGNOSTIC
            LogDialogueDiagnosticReturn("post-cinematic-recovery", context);
#endif
            return;
        }
        if (g_coordinator.load(std::memory_order_acquire) != CoordinatorState::Gameplay) {
            ResetDialogueRuntimeState();
#ifdef DIALOGUE_BOUNDARY_DIAGNOSTIC
            LogDialogueDiagnosticReturn("non-gameplay", context);
#endif
            return;
        }
        const auto selectedPolicy = g_runtimeDialoguePolicy.load(std::memory_order_acquire);
        const bool lifecycleUsesActivePolicy =
            g_dialoguePhase == DialoguePhase::Active ||
            g_dialoguePhase == DialoguePhase::Exiting ||
            g_dialoguePhase == DialoguePhase::RearmPending;
        if (g_dialoguePhase == DialoguePhase::Inactive && selectedPolicy == DialogueZoomPolicy::Native) {
            ResetDialogueRuntimeState();
#ifdef DIALOGUE_BOUNDARY_DIAGNOSTIC
            LogDialogueDiagnosticReturn("native-policy", context);
#endif
            return;
        }
        if (lifecycleUsesActivePolicy && !g_activeDialoguePolicy.IsValid()) {
            ResetDialogueRuntimeState();
#ifdef DIALOGUE_BOUNDARY_DIAGNOSTIC
            LogDialogueDiagnosticReturn("active-policy-unavailable", context);
#endif
            return;
        }
        const auto policy = lifecycleUsesActivePolicy ? g_activeDialoguePolicy.Value() : selectedPolicy;
        const bool ascending = std::isfinite(g_dialoguePrevious) &&
            incoming > g_dialoguePrevious + kDialogueTransformEpsilon;

        if (g_dialoguePhase == DialoguePhase::RearmPending) {
            float nativeTarget = std::numeric_limits<float>::quiet_NaN();
            const auto recoverySource = static_cast<std::uintptr_t>(context.rsi);
            if (recoverySource != 0)
                SafeRead(recoverySource + 0x2C, nativeTarget);
            const auto recoveryDecision = g_dialogueRecoveryRearm.Observe(
                recoverySource, incoming, nativeTarget, kRecoveryEpsilon,
                kDialogueTransformEpsilon, kDialogueRecoveryStableSamples);
            if (recoveryDecision == dialogue::RecoveryRearm::Decision::Complete) {
                Log("Dialogue recovery re-armed after native FOV convergence: targetFov=",
                    nativeTarget, " baselineG=", g_dialogueBaseline, ".");
                ResetDialogueRuntimeState();
            } else if (recoveryDecision == dialogue::RecoveryRearm::Decision::Cancel) {
                Log("Dialogue recovery cancelled: reason=source-changed.");
                ResetDialogueRuntimeState();
            }
        } else if (g_dialoguePhase == DialoguePhase::Inactive) {
            if (!std::isfinite(g_dialogueBaseline)) {
                const auto candidateSource = static_cast<std::uintptr_t>(context.rsi);
                float candidateTarget = std::numeric_limits<float>::quiet_NaN();
                const bool candidateContextValid = candidateSource != 0 &&
                    SafeRead(candidateSource + 0x2C, candidateTarget) &&
                    IsValidFovValue(candidateTarget);
                if (!dialogue::CandidatePromotionAllowed(selectedPolicy,
                    g_coordinator.load(std::memory_order_acquire) == CoordinatorState::Gameplay,
                    !g_postCinematicDialogueExclusion.IsActive()) ||
                    !candidateContextValid) {
                    ResetDialogueRuntimeState();
                    Log("Dialogue candidate ignored: reason=invalid-context policy=",
                        DialogueZoomPolicyName(selectedPolicy), " source=0x", std::hex,
                        candidateSource, std::dec, " target=", candidateTarget,
                        " targetReadable=", candidateContextValid ? 1 : 0, ".");
                } else {
                    g_dialogueBaseline = incoming;
                    g_dialoguePhase = DialoguePhase::Candidate;
                    g_dialogueCandidate.Begin(incoming, candidateSource, candidateTarget);
                    g_dialogueExitIncomingStart = std::numeric_limits<float>::quiet_NaN();
                    Log("Dialogue candidate captured: provisionalBaselineG=", g_dialogueBaseline,
                        " source=0x", std::hex, candidateSource, std::dec,
                        " nativeTarget=", candidateTarget, " selectedPolicy=",
                        DialogueZoomPolicyName(selectedPolicy), ". No transform applied.");
                }
            }
        } else if (g_dialoguePhase == DialoguePhase::Candidate) {
            const auto candidateSource = static_cast<std::uintptr_t>(context.rsi);
            float candidateTarget = std::numeric_limits<float>::quiet_NaN();
            const bool candidateContextValid = candidateSource != 0 &&
                SafeRead(candidateSource + 0x2C, candidateTarget) &&
                IsValidFovValue(candidateTarget);
            if (!dialogue::CandidatePromotionAllowed(selectedPolicy,
                g_coordinator.load(std::memory_order_acquire) == CoordinatorState::Gameplay,
                !g_postCinematicDialogueExclusion.IsActive()) || !candidateContextValid) {
                Log("Dialogue candidate cancelled: reason=invalid-context policy=",
                    DialogueZoomPolicyName(selectedPolicy), " source=0x", std::hex,
                    candidateSource, std::dec, " target=", candidateTarget,
                    " targetReadable=", candidateContextValid ? 1 : 0, ".");
                ResetDialogueRuntimeState();
                g_dialoguePrevious = incoming;
                return;
            }
            const auto decision = g_dialogueCandidate.Observe(incoming, candidateSource,
                candidateTarget, kDialogueTransformEpsilon, kDialogueTransformEpsilon,
                kDialogueTransformEpsilon, kDialogueCandidateStableSamples);
            if (decision == dialogue::CandidateDecision::Activate) {
                g_dialogueBaseline = g_dialogueCandidate.Baseline();
                g_dialoguePhase = DialoguePhase::Active;
                g_activeDialoguePolicy.Capture(selectedPolicy);
                g_dialogueExitIncomingStart = std::numeric_limits<float>::quiet_NaN();
                Log("Dialogue lifecycle started after confirmed native descent: baselineG=",
                    g_dialogueBaseline, " activePolicy=", DialogueZoomPolicyName(g_activeDialoguePolicy.Value()),
                    " selectedPolicy=", DialogueZoomPolicyName(selectedPolicy), ".");
            } else if (decision == dialogue::CandidateDecision::Cancel) {
                Log("Dialogue candidate cancelled: reason=trajectory-not-confirmed.");
                ResetDialogueRuntimeState();
            }
        } else if (g_dialoguePhase == DialoguePhase::Active && ascending) {
            g_dialoguePhase = DialoguePhase::Exiting;
            g_dialogueExitIncomingStart = incoming;
#ifdef DIALOGUE_RECOVERY_ENDPOINT_DIAGNOSTIC
            ArmDialogueRecoveryEndpointTrace(static_cast<std::uintptr_t>(context.rsi));
#endif
            Log("Dialogue lifecycle exiting: incoming=", incoming, " baselineG=", g_dialogueBaseline, ".");
        }

        float output = incoming;
        if (g_dialoguePhase == DialoguePhase::Exiting && ascending &&
            std::isfinite(g_dialogueBaseline) &&
            std::fabs(incoming - g_dialogueBaseline) <= kDialogueRecoveryEpsilon) {
            const auto recoverySource = static_cast<std::uintptr_t>(context.rsi);
            float nativeTarget = std::numeric_limits<float>::quiet_NaN();
            if (recoverySource != 0)
                SafeRead(recoverySource + 0x2C, nativeTarget);
            g_dialoguePhase = DialoguePhase::RearmPending;
            g_dialogueRecoveryRearm.Begin(recoverySource, incoming,
                IsValidFovValue(nativeTarget) ? nativeTarget
                    : std::numeric_limits<float>::quiet_NaN());
            Log("Dialogue recovery entered re-arm pending: baselineG=", g_dialogueBaseline,
                " incoming=", incoming, " nativeTarget=", nativeTarget,
                " targetReadable=", IsValidFovValue(nativeTarget) ? 1 : 0,
                ". Awaiting native FOV convergence.");
            output = incoming;
        }
        else if ((policy == DialogueZoomPolicy::Adaptive || policy == DialogueZoomPolicy::Reduced) &&
            (g_dialoguePhase == DialoguePhase::Active || g_dialoguePhase == DialoguePhase::Exiting) &&
            std::isfinite(g_dialogueBaseline)) {
            const float target = policy == DialogueZoomPolicy::Adaptive
                ? dialogue::AdaptiveTarget(g_dialogueBaseline)
                : dialogue::ReducedTarget(g_dialogueBaseline);
            output = g_dialoguePhase == DialoguePhase::Exiting &&
                std::isfinite(g_dialogueExitIncomingStart)
                ? dialogue::TransformExitSample(incoming, g_dialogueBaseline, target,
                    g_dialogueExitIncomingStart)
                : dialogue::TransformProjectionSample(incoming, g_dialogueBaseline, target);
        }
        else if (policy == DialogueZoomPolicy::Disabled &&
            (g_dialoguePhase == DialoguePhase::Active || g_dialoguePhase == DialoguePhase::Exiting) &&
            std::isfinite(g_dialogueBaseline)) {
            output = g_dialogueBaseline;
        }

        if (std::isfinite(output) && output > 1.0f && output < 179.0f)
            context.xmm1.f32[0] = output;
        g_dialoguePrevious = incoming;
#ifdef DIALOGUE_BOUNDARY_DIAGNOSTIC
        LogDialogueDiagnosticReturn("processed", context);
#endif
    }

    bool InstallDialogueBoundary()
    {
        const auto matches = hooks::FindAll(g_executable, DialogueBoundary);
        if (matches.size() != 1) {
            Log("Dialogue boundary resolver rejected: matches=", matches.size(), ". Native pass-through retained.");
            return false;
        }
        auto* hook = matches.front() + kDialogueBoundaryHookOffset;
        constexpr std::uint8_t expected[] = { 0xFF, 0x90, 0x08, 0x06, 0x00, 0x00 };
        if (std::memcmp(hook, expected, sizeof(expected)) != 0) {
            Log("Dialogue boundary instruction contract rejected. Native pass-through retained.");
            return false;
        }
        g_dialogueBoundaryHook = safetyhook::create_mid(
            hook, SafeMidHookEntry<&TraceDialogueBoundary>);
        if (!g_dialogueBoundaryHook || !g_dialogueBoundaryHook.enable()) {
            g_dialogueBoundaryHook.reset();
            Log("Dialogue boundary hook setup failed. Native pass-through retained.");
            return false;
        }
        Log("Dialogue boundary hook installed: RVA=0x", std::hex,
            reinterpret_cast<std::uintptr_t>(hook) - reinterpret_cast<std::uintptr_t>(g_executable),
            std::dec, " policy=", DialogueZoomPolicyName(g_runtimeDialoguePolicy.load(std::memory_order_acquire)), ".");
        return true;
    }

    bool TryAtomicExitHandoff(SafetyHookContext& context)
    {
        if (!g_atomicExitHandoffPending.load(std::memory_order_acquire)) return false;
        if (g_coordinator.load(std::memory_order_acquire) != CoordinatorState::CinematicExiting) {
            ResetAtomicExitHandoff("coordinator-not-exiting");
            return false;
        }

        {
            std::lock_guard dialogueLock(g_dialogueMutex);
            if (g_dialoguePhase != DialoguePhase::Inactive) {
                ResetAtomicExitHandoff("dialogue-active");
                return false;
            }
        }

        const auto source = static_cast<std::uintptr_t>(context.rsi);
        const float currentFov = context.xmm0.f32[0];
        const float targetFov = g_exitTargetFov.load(std::memory_order_acquire);
        const auto expectedSource = g_atomicExitHandoffSource.load(std::memory_order_acquire);
        if (expectedSource != 0 && expectedSource != source) {
            ResetAtomicExitHandoff("source-changed");
            return false;
        }
        if (!std::isfinite(currentFov) || !std::isfinite(targetFov)) {
            ResetAtomicExitHandoff("fov-unreadable");
            return false;
        }

        const float previousFov = g_atomicExitHandoffPreviousFov.load(std::memory_order_acquire);
        if (expectedSource == 0) {
            g_atomicExitHandoffSource.store(source, std::memory_order_release);
            g_atomicExitHandoffPreviousFov.store(currentFov, std::memory_order_release);
            return false;
        }
        g_atomicExitHandoffPreviousFov.store(currentFov, std::memory_order_release);
        if (!std::isfinite(previousFov) ||
            currentFov >= previousFov - kDialogueTransformEpsilon ||
            currentFov <= targetFov + kRecoveryEpsilon) return false;

        float aspect = 0.0f;
        std::uint8_t flags = 0;
        if (!SafeRead(source + kAspectOffset, aspect) ||
            !SafeRead(source + kFlagsOffset, flags)) {
            ResetAtomicExitHandoff("camera-state-unreadable");
            return false;
        }

        bool expectedPending = true;
        if (!g_atomicExitHandoffPending.compare_exchange_strong(expectedPending, false,
            std::memory_order_acq_rel)) return false;
        ApplyGameplayAspectFixAtomic(source, "RecoveryStart", currentFov, aspect, flags);
        return true;
    }

#ifdef HORPLUS_CINEMATIC_WRITER_TRACE_DIAGNOSTIC
    struct HorPlusCinematicWriterTraceSnapshot
    {
        bool valid{};
        std::uintptr_t source{};
        std::uintptr_t output{};
        float writerFov{};
        float cameraWorldFov{};
        float cameraFirstPersonFov{};
        float aspect{};
        std::uint8_t flags{};
        float outputFov{};
        float outputAspect{};
        bool eligible{};
        float cachedEnterFov{std::numeric_limits<float>::quiet_NaN()};
        float enterAspect{std::numeric_limits<float>::quiet_NaN()};
        bool cacheValid{};
        bool numericGuardMatched{};
    };

    thread_local HorPlusCinematicWriterTraceSnapshot g_previousHorPlusCinematicWriter{};
    thread_local bool g_logHorPlusCinematicWriterInvocation{};

    bool ReadHorPlusCinematicWriterState(SafetyHookContext& context, float writerFov,
        HorPlusCinematicWriterTraceSnapshot& snapshot)
    {
        snapshot = {};
        snapshot.source = static_cast<std::uintptr_t>(context.rsi);
        snapshot.output = static_cast<std::uintptr_t>(context.rbx);
        snapshot.writerFov = writerFov;
        snapshot.cachedEnterFov = g_cinematicTransformedFov.load(std::memory_order_acquire);
        snapshot.enterAspect = g_cinematicEnterAspect.load(std::memory_order_acquire);
        snapshot.cacheValid = std::isfinite(snapshot.cachedEnterFov);
        snapshot.numericGuardMatched = snapshot.cacheValid &&
            std::fabs(writerFov - snapshot.cachedEnterFov) <= kDialogueTransformEpsilon;
        if (!snapshot.source || !snapshot.output ||
            !SafeRead(snapshot.source + kAspectOffset, snapshot.aspect) ||
            !SafeRead(snapshot.source + kFlagsOffset, snapshot.flags) ||
            !SafeRead(snapshot.source + 0x230, snapshot.cameraWorldFov) ||
            !SafeRead(snapshot.source + 0x234, snapshot.cameraFirstPersonFov) ||
            !SafeRead(snapshot.output + 0x30, snapshot.outputFov) ||
            !SafeRead(snapshot.output + 0x5C, snapshot.outputAspect)) return false;
        float candidate{};
        snapshot.eligible =
            g_coordinator.load(std::memory_order_acquire) == CoordinatorState::CinematicActive &&
            gameplay::TryTransformHorPlus(writerFov, snapshot.aspect, snapshot.flags,
                kNativeAspect, candidate);
        snapshot.valid = true;
        return true;
    }

    bool HorPlusCinematicWriterSnapshotChanged(
        const HorPlusCinematicWriterTraceSnapshot& current)
    {
        const auto floatChanged = [](float left, float right) {
            return std::isfinite(left) != std::isfinite(right) ||
                (std::isfinite(left) && std::isfinite(right) && std::fabs(left - right) > 0.01f);
        };
        const auto& previous = g_previousHorPlusCinematicWriter;
        return !previous.valid || current.source != previous.source || current.output != previous.output ||
            floatChanged(current.writerFov, previous.writerFov) ||
            floatChanged(current.cameraWorldFov, previous.cameraWorldFov) ||
            floatChanged(current.cameraFirstPersonFov, previous.cameraFirstPersonFov) ||
            floatChanged(current.aspect, previous.aspect) || current.flags != previous.flags ||
            floatChanged(current.outputFov, previous.outputFov) ||
            floatChanged(current.outputAspect, previous.outputAspect) ||
            current.eligible != previous.eligible ||
            floatChanged(current.cachedEnterFov, previous.cachedEnterFov) ||
            floatChanged(current.enterAspect, previous.enterAspect) ||
            current.cacheValid != previous.cacheValid ||
            current.numericGuardMatched != previous.numericGuardMatched;
    }

    void LogHorPlusCinematicWriterTrace(const char* phase,
        const HorPlusCinematicWriterTraceSnapshot& snapshot, float beforeFov, float afterFov,
        float candidateNativeFov)
    {
#ifdef MATCHGAMEPLAY_DIAGNOSTIC
        const float gameplayNative = g_matchGameplayPreEnterNativeFov.load(std::memory_order_acquire);
        const float gameplayHorPlus = g_matchGameplayPreEnterHorPlusFov.load(std::memory_order_acquire);
        const float gameplayAspect = g_matchGameplayPreEnterAspect.load(std::memory_order_acquire);
        const float enterObservation = g_matchGameplayEnterNativeFov.load(std::memory_order_acquire);
        const bool pairValid = g_matchGameplayPreEnterPairValid.load(std::memory_order_acquire);
        const diagnostics::matchgameplay::PredictionInput input{
            snapshot.numericGuardMatched, pairValid, gameplayNative, gameplayHorPlus,
            gameplayAspect, enterObservation, candidateNativeFov, snapshot.aspect};
        const auto prediction = diagnostics::matchgameplay::Evaluate(input, kNativeAspect);
        const char* sampleSpace = prediction.sampleSpace ==
                diagnostics::matchgameplay::SampleSpace::CachedTransformedEnter
            ? "CACHED_TRANSFORMED_ENTER"
            : prediction.sampleSpace == diagnostics::matchgameplay::SampleSpace::Native
                ? "NATIVE"
                : "UNAVAILABLE";
#endif
        Log("HORPLUS_CINEMATIC_WRITER ", phase,
            " coordinator=", CoordinatorStateName(g_coordinator.load(std::memory_order_acquire)),
            " source=0x", std::hex, snapshot.source, " output=0x", snapshot.output, std::dec,
            " xmm0Before=", beforeFov, " xmm0After=", afterFov,
            " eligible=", snapshot.eligible ? "YES" : "NO",
            " applied=", std::fabs(afterFov - beforeFov) > 0.001f ? "YES" : "NO",
            " cameraWorldFov=", snapshot.cameraWorldFov,
            " cameraFirstPersonFov=", snapshot.cameraFirstPersonFov,
            " aspect=", snapshot.aspect, " flags=0x", std::hex,
            static_cast<unsigned>(snapshot.flags), std::dec,
            " outputFOV=", snapshot.outputFov, " outputAspect=", snapshot.outputAspect,
            " cachedEnterFov=", snapshot.cachedEnterFov,
            " enterAspect=", snapshot.enterAspect,
            " cacheValid=", snapshot.cacheValid ? "true" : "false",
            " numericGuardMatched=", snapshot.numericGuardMatched ? "true" : "false"
#ifdef MATCHGAMEPLAY_DIAGNOSTIC
            , " matchGameplayCandidateAvailable=", prediction.candidateAvailable ? "true" : "false",
            " matchGameplaySampleSpace=", sampleSpace,
            " matchGameplayReferenceSource=ENTER_OBSERVATION",
            " matchGameplayEnterObservation=", enterObservation,
            " matchGameplayNativeBaseline=", gameplayNative,
            " matchGameplayHorPlusBaseline=", gameplayHorPlus,
            " matchGameplayBaselineAspect=", gameplayAspect,
            " matchGameplaySampleNative=", candidateNativeFov,
            " matchGameplayNativeMatched=", prediction.nativeMatched,
            " matchGameplayHorPlus=", prediction.horPlusMatched
#endif
            , ".");
    }

    void TraceHorPlusCinematicWriterBefore(SafetyHookContext& context)
    {
        g_logHorPlusCinematicWriterInvocation = false;
        if (!diagnostics::Enabled()) return;
        if (g_coordinator.load(std::memory_order_acquire) != CoordinatorState::CinematicActive) return;
        HorPlusCinematicWriterTraceSnapshot snapshot{};
        if (!ReadHorPlusCinematicWriterState(context, context.xmm0.f32[0], snapshot)) return;
        if (HorPlusCinematicWriterSnapshotChanged(snapshot)) {
            LogHorPlusCinematicWriterTrace("BEFORE", snapshot, context.xmm0.f32[0], context.xmm0.f32[0],
                context.xmm0.f32[0]);
            g_logHorPlusCinematicWriterInvocation = true;
        }
    }

    void TraceHorPlusCinematicWriterAfter(SafetyHookContext& context, float beforeFov)
    {
        if (!diagnostics::Enabled()) return;
        if (g_coordinator.load(std::memory_order_acquire) != CoordinatorState::CinematicActive &&
            !g_logHorPlusCinematicWriterInvocation) return;
        HorPlusCinematicWriterTraceSnapshot snapshot{};
        if (!ReadHorPlusCinematicWriterState(context, context.xmm0.f32[0], snapshot)) return;
        if (g_logHorPlusCinematicWriterInvocation || HorPlusCinematicWriterSnapshotChanged(snapshot))
            LogHorPlusCinematicWriterTrace("AFTER", snapshot, beforeFov, context.xmm0.f32[0], beforeFov);
        g_previousHorPlusCinematicWriter = snapshot;
        g_logHorPlusCinematicWriterInvocation = false;
    }
#endif

    struct HorPlusGameplayApplyResult
    {
        std::uintptr_t source{};
        float inputFov{};
        float outputFov{};
        float aspect{std::numeric_limits<float>::quiet_NaN()};
        std::uint8_t flags{};
        bool sourceReadable{};
        bool eligible{};
        bool applied{};
        bool gameplayStateEstablished{};
        const char* reason{"NOT_ATTEMPTED"};
        bool observationPublished{};
        camera::CameraFovObservation observation{};
        camera::GameplayBaselineProjectionResult baselineProjection{};
    };

    HorPlusGameplayApplyResult ApplyHorPlusGameplay(SafetyHookContext& context);

#ifdef HORPLUS_FOV_STATE_DIAGNOSTIC
    enum class HorPlusTelemetryOwner : std::uint8_t
    {
        Gameplay,
        Dialogue,
        Cinematic,
        Recovery,
        Unknown,
    };

    const char* HorPlusTelemetryOwnerName(HorPlusTelemetryOwner owner)
    {
        switch (owner) {
        case HorPlusTelemetryOwner::Gameplay: return "Gameplay";
        case HorPlusTelemetryOwner::Dialogue: return "Dialogue";
        case HorPlusTelemetryOwner::Cinematic: return "Cinematic";
        case HorPlusTelemetryOwner::Recovery: return "Recovery";
        case HorPlusTelemetryOwner::Unknown: return "Unknown";
        }
        return "Unknown";
    }

    HorPlusTelemetryOwner ResolveHorPlusTelemetryOwner()
    {
        if (g_coordinator.load(std::memory_order_acquire) == CoordinatorState::CinematicActive)
            return HorPlusTelemetryOwner::Cinematic;
        if (g_postCinematicDialogueExclusion.IsActive())
            return HorPlusTelemetryOwner::Recovery;
        {
            std::lock_guard dialogueLock(g_dialogueMutex);
            if (g_dialoguePhase == DialoguePhase::Active ||
                g_dialoguePhase == DialoguePhase::Exiting ||
                g_dialoguePhase == DialoguePhase::RearmPending)
                return HorPlusTelemetryOwner::Dialogue;
        }
        if (g_coordinator.load(std::memory_order_acquire) == CoordinatorState::Gameplay)
            return HorPlusTelemetryOwner::Gameplay;
        return HorPlusTelemetryOwner::Unknown;
    }

    void TraceHorPlusFovState(const HorPlusGameplayApplyResult& applyResult)
    {
        if (!diagnostics::Enabled()) return;
        const auto owner = ResolveHorPlusTelemetryOwner();
        const auto mode = g_runtimeGameplayMode.load(std::memory_order_acquire);
        const bool observationAvailable = applyResult.observationPublished;
        const auto& observation = applyResult.observation;
        const float nativeFov = applyResult.inputFov;
        const float aspect = applyResult.aspect;
        const auto flags = applyResult.flags;
        const float transformedFov = applyResult.applied ? applyResult.outputFov :
            std::numeric_limits<float>::quiet_NaN();
        const bool transformed = applyResult.applied;
        const auto ownerValue = static_cast<std::uint8_t>(owner);

        const auto currentCoordinator = g_coordinator.load(std::memory_order_acquire);
        DialoguePhase dialoguePhase = DialoguePhase::Inactive;
        {
            std::lock_guard dialogueLock(g_dialogueMutex);
            dialoguePhase = g_dialoguePhase;
        }
        const bool stableContext = owner == HorPlusTelemetryOwner::Gameplay &&
            currentCoordinator == CoordinatorState::Gameplay &&
            dialoguePhase == DialoguePhase::Inactive &&
            !g_postCinematicDialogueExclusion.IsActive() &&
            applyResult.sourceReadable && std::isfinite(nativeFov);

        std::lock_guard telemetryLock(g_horPlusFovTelemetryMutex);
        const bool ownerChanged = !g_runtime.horPlusFovTelemetryValid ||
            g_runtime.horPlusTelemetryOwner != ownerValue;
        if (ownerChanged) {
            const auto previous = g_runtime.horPlusFovTelemetryValid
                ? HorPlusTelemetryOwnerName(static_cast<HorPlusTelemetryOwner>(g_runtime.horPlusTelemetryOwner))
                : "None";
            Log("CAMERA_OWNER from=", previous, " to=", HorPlusTelemetryOwnerName(owner), ".");
        }

        const auto floatChanged = [](float a, float b) {
            return std::isfinite(a) != std::isfinite(b) ||
                (std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) > 0.01f);
        };
        const bool nativeChanged = !g_runtime.horPlusFovTelemetryValid ||
            floatChanged(nativeFov, g_runtime.horPlusTelemetryNativeFov);
        const bool aspectChanged = !g_runtime.horPlusFovTelemetryValid ||
            floatChanged(aspect, g_runtime.horPlusTelemetryAspect);
        const bool modeChanged = !g_runtime.horPlusFovTelemetryValid ||
            mode != g_runtime.horPlusTelemetryMode;

        if (aspectChanged || modeChanged) {
            g_runtime.horPlusGameplayCacheValid = false;
            Log("HORPLUS_STATE valid=false reason=", modeChanged ? "MODE_CHANGED" : "ASPECT_CHANGED",
                " configured=UNKNOWN nativeGameplay=", nativeFov, " aspect=", aspect,
                " mode=", config::GameplayModeName(mode), " owner=", HorPlusTelemetryOwnerName(owner), ".");
        }
        if (applyResult.gameplayStateEstablished && applyResult.applied) {
            if (!g_runtime.horPlusGameplayCacheValid) {
                g_runtime.horPlusGameplayCacheValid = true;
                Log("HORPLUS_STATE valid=true reason=GAMEPLAY_STATE_ESTABLISHED configured=UNKNOWN nativeGameplay=",
                    nativeFov, " horPlusGameplay=", transformedFov, " aspect=", aspect,
                    " mode=", config::GameplayModeName(mode), " owner=", HorPlusTelemetryOwnerName(owner), ".");
            }
        }
        if (nativeChanged) {
            const char* event = owner == HorPlusTelemetryOwner::Cinematic ||
                owner == HorPlusTelemetryOwner::Recovery ? "CINEMATIC_FOV_CHANGE" :
                owner == HorPlusTelemetryOwner::Dialogue ? "DIALOGUE_FOV_CHANGE" :
                owner == HorPlusTelemetryOwner::Unknown ? "UNKNOWN_FOV_CHANGE" : "GAMEPLAY_FOV_CHANGE";
            Log(event, " configured=UNKNOWN nativeOld=", g_runtime.horPlusTelemetryNativeFov,
                " nativeNew=", nativeFov, " modifier=UNKNOWN horPlusOld=",
                g_runtime.horPlusTelemetryResult, " horPlusNew=", transformedFov,
                " nativeBeforeHorPlus=", nativeFov,
                " horPlusResult=", transformedFov,
                " aspect=", aspect, " owner=", HorPlusTelemetryOwnerName(owner),
                " cacheValid=", g_runtime.horPlusGameplayCacheValid ? "true" : "false",
                " reason=", applyResult.reason, " handled=", transformed ? "true" : "false", ".");
            if (observationAvailable) {
                Log("FOV_OBSERVATION boundary=", camera::FovObservationBoundaryName(observation.boundary),
                    " input=", observation.inputFov.value,
                    " inputSpace=", camera::FovSpaceName(observation.inputFov.space),
                    " inputProvenance=", camera::FovProvenanceName(observation.inputFov.provenance),
                    " result=", observation.resultFov.value,
                    " resultSpace=", camera::FovSpaceName(observation.resultFov.space),
                    " resultProvenance=", camera::FovProvenanceName(observation.resultFov.provenance),
                    " aspect=", observation.aspect.value,
                    " writerSource=0x", std::hex, observation.writerSource.value, std::dec,
                    " writerSourceValid=", observation.writerSource.valid ? "true" : "false",
                    " sequence=", observation.publicationSequence, ".");
                Log("GAMEPLAY_BASELINE_PROJECTION disposition=",
                    camera::GameplayBaselineProjectionDispositionName(
                        applyResult.baselineProjection.disposition),
                    " eligible=", applyResult.baselineProjection.eligible ? "true" : "false",
                    " valid=", applyResult.baselineProjection.baseline.valid ? "true" : "false",
                    " native=", applyResult.baselineProjection.baseline.nativeFov.value,
                    " nativeValid=", applyResult.baselineProjection.baseline.nativeFov.valid ? "true" : "false",
                    " horPlus=", applyResult.baselineProjection.baseline.horPlusFov.value,
                    " horPlusValid=", applyResult.baselineProjection.baseline.horPlusFov.valid ? "true" : "false",
                    " aspect=", applyResult.baselineProjection.baseline.aspect,
                    " source=0x", std::hex,
                    applyResult.baselineProjection.baseline.source.value, std::dec,
                    " sequence=", applyResult.baselineProjection.baseline.observationSequence, ".");
            }
        }

        if (!stableContext) {
            g_runtime.stableObservationSource = 0;
            g_runtime.stableObservationFov = std::numeric_limits<float>::quiet_NaN();
            g_runtime.stableObservationSamples = 0;
            g_runtime.stableObservationReported = false;
        } else if (g_runtime.stableObservationSource == applyResult.source &&
            std::isfinite(g_runtime.stableObservationFov) &&
            std::fabs(g_runtime.stableObservationFov - nativeFov) <= 0.01f) {
            ++g_runtime.stableObservationSamples;
        } else {
            g_runtime.stableObservationSource = applyResult.source;
            g_runtime.stableObservationFov = nativeFov;
            g_runtime.stableObservationSamples = 1;
            g_runtime.stableObservationReported = false;
        }
        if (stableContext && g_runtime.stableObservationSamples >= 3 &&
            !g_runtime.stableObservationReported) {
            g_runtime.stableObservationReported = true;
            Log("STABLE_GAMEPLAY_ENDPOINT_OBSERVATION fov=", nativeFov,
                " source=0x", std::hex, applyResult.source, std::dec,
                " stableSamples=", g_runtime.stableObservationSamples,
                " coordinator=", CoordinatorStateName(currentCoordinator),
                " dialoguePhase=", DialoguePhaseName(dialoguePhase),
                " recoveryExclusion=false",
                " lastZoomDirection=observation-only",
                " lastZoomSequence=", g_runtime.snapshotZoomSequence.load(std::memory_order_acquire),
                " hypothesis=stable-endpoint-only.");
        }
        g_runtime.horPlusFovTelemetryValid = true;
        g_runtime.horPlusTelemetryNativeFov = nativeFov;
        g_runtime.horPlusTelemetryAspect = aspect;
        g_runtime.horPlusTelemetryResult = transformedFov;
        g_runtime.horPlusTelemetryFlags = flags;
        g_runtime.horPlusTelemetryMode = mode;
        g_runtime.horPlusTelemetryOwner = ownerValue;

#ifdef CAMERA_STATE_SNAPSHOT_DIAGNOSTIC
        thread_local camera::CameraStateSnapshot previousSnapshot{};
        thread_local bool previousSnapshotValid = false;
        camera::CameraStateInput snapshotInput{};
        const auto coordinator = g_coordinator.load(std::memory_order_acquire);
        snapshotInput.presentation.state = coordinator == CoordinatorState::CinematicActive
            ? camera::PresentationState::CinematicActive
            : coordinator == CoordinatorState::CinematicExiting
                ? camera::PresentationState::CinematicExiting
                : coordinator == CoordinatorState::Gameplay
                    ? camera::PresentationState::Gameplay
                    : camera::PresentationState::Unknown;
        snapshotInput.presentation.provenance = camera::EvidenceProvenance::ModDerivedState;
        {
            std::lock_guard dialogueLock(g_dialogueMutex);
            switch (g_dialoguePhase) {
            case DialoguePhase::Inactive:
                snapshotInput.dialogue.state = camera::DialogueState::Inactive;
                snapshotInput.dialogue.provenance = camera::EvidenceProvenance::ConfirmedModLifecycle;
                break;
            case DialoguePhase::Candidate:
                snapshotInput.dialogue.state = camera::DialogueState::Candidate;
                snapshotInput.dialogue.provenance = camera::EvidenceProvenance::ClassifierHypothesis;
                break;
            case DialoguePhase::Active:
                snapshotInput.dialogue.state = camera::DialogueState::Active;
                snapshotInput.dialogue.provenance = camera::EvidenceProvenance::ConfirmedModLifecycle;
                snapshotInput.dialogue.activePolicyValid = g_activeDialoguePolicy.IsValid();
                break;
            case DialoguePhase::Exiting:
                snapshotInput.dialogue.state = camera::DialogueState::Exiting;
                snapshotInput.dialogue.provenance = camera::EvidenceProvenance::ConfirmedModLifecycle;
                snapshotInput.dialogue.activePolicyValid = g_activeDialoguePolicy.IsValid();
                break;
            case DialoguePhase::RearmPending:
                snapshotInput.dialogue.state = camera::DialogueState::Recovery;
                snapshotInput.dialogue.provenance = camera::EvidenceProvenance::ConfirmedModLifecycle;
                snapshotInput.dialogue.activePolicyValid = g_activeDialoguePolicy.IsValid();
                break;
            }
        }
        snapshotInput.dialogue.recoveryExclusionActive =
            g_postCinematicDialogueExclusion.IsActive();
        const auto dialogueSource = g_runtime.snapshotDialogueSource.load(std::memory_order_acquire);
        snapshotInput.dialogue.source = { dialogueSource, dialogueSource != 0 };
        snapshotInput.dialogue.nativeTarget = g_runtime.snapshotDialogueTarget.load(
            std::memory_order_acquire);
        snapshotInput.dialogue.nativeTargetValid =
            g_runtime.snapshotDialogueTargetValid.load(std::memory_order_acquire);
        snapshotInput.gameplayMode = {
            static_cast<std::uint8_t>(mode),
            camera::EvidenceProvenance::ModDerivedState, true };
        snapshotInput.evidence.nativeWriterFov = nativeFov;
        snapshotInput.evidence.aspect = aspect;
        snapshotInput.evidence.flags = flags;
        snapshotInput.evidence.transformedFov = transformedFov;
        snapshotInput.evidence.nativeWriterFovValid = std::isfinite(nativeFov);
        snapshotInput.evidence.aspectValid = std::isfinite(aspect);
        snapshotInput.evidence.transformedFovValid = std::isfinite(transformedFov);
        snapshotInput.evidence.nativeWriterFovProvenance =
            camera::EvidenceProvenance::NativeNumericObservation;
        snapshotInput.evidence.aspectProvenance =
            camera::EvidenceProvenance::NativeNumericObservation;
        snapshotInput.evidence.transformedFovProvenance =
            camera::EvidenceProvenance::ModDerivedState;
        snapshotInput.evidence.configuredGameplayFovKnown = false;
#ifdef ZOOM_TRANSITION_DIAGNOSTIC
        const auto zoomDirection = g_runtime.snapshotZoomDirection.load(std::memory_order_acquire);
        snapshotInput.zoom.direction = zoomDirection == 1 ? camera::ZoomDirection::In :
            zoomDirection == 2 ? camera::ZoomDirection::Out : camera::ZoomDirection::None;
        snapshotInput.zoom.provenance = zoomDirection == 0
            ? camera::EvidenceProvenance::Unavailable : camera::EvidenceProvenance::NativeEvent;
        snapshotInput.zoom.sequence = g_runtime.snapshotZoomSequence.load(std::memory_order_acquire);
        snapshotInput.zoom.valid = zoomDirection != 0;
        snapshotInput.zoom.primaryWeight = g_runtime.snapshotZoomPrimary.load(std::memory_order_acquire);
        snapshotInput.zoom.secondaryWeight = g_runtime.snapshotZoomSecondary.load(std::memory_order_acquire);
        const auto zoomSource = g_runtime.snapshotZoomSource.load(std::memory_order_acquire);
        snapshotInput.zoom.source = { zoomSource, zoomSource != 0 };
#endif
        const auto gameplayWriterSource = applyResult.source;
        snapshotInput.source.gameplayWriter = {
            gameplayWriterSource, gameplayWriterSource != 0 };
        const auto snapshot = camera::BuildCameraStateSnapshot(snapshotInput);
        if (!previousSnapshotValid || camera::SnapshotSemanticsChanged(snapshot, previousSnapshot)) {
            auto semanticSnapshot = snapshot;
            semanticSnapshot.generation.eventSequence =
                g_runtime.snapshotEventSequence.fetch_add(1, std::memory_order_acq_rel) + 1;
            semanticSnapshot.generation.eventSequenceValid = true;
            Log("CAMERA_STATE_SNAPSHOT presentation=",
                camera::PresentationStateName(semanticSnapshot.presentation.state),
                " dialogue=", camera::DialogueStateName(semanticSnapshot.dialogue.state),
                " dialogueProvenance=", camera::EvidenceProvenanceName(
                    semanticSnapshot.dialogue.provenance),
                " recoveryExclusion=", semanticSnapshot.dialogue.recoveryExclusionActive ? "true" : "false",
                " lastZoomDirection=", camera::ZoomDirectionName(semanticSnapshot.zoom.direction),
                " lastZoomProvenance=", camera::EvidenceProvenanceName(semanticSnapshot.zoom.provenance),
                " lastZoomSequence=", semanticSnapshot.zoom.sequence,
                " zoomObservationAvailable=", semanticSnapshot.zoom.valid ? "true" : "false",
                " mode=", config::GameplayModeName(mode),
                " nativeFov=", nativeFov, " aspect=", aspect, " flags=0x", std::hex,
                static_cast<unsigned>(flags), std::dec, " horPlusFov=", transformedFov,
                " gameplaySource=0x", std::hex, semanticSnapshot.source.gameplayWriter.value,
                " dialogueSource=0x", semanticSnapshot.dialogue.source.value, std::dec,
                " dialogueSourceAvailable=", semanticSnapshot.dialogue.source.valid ? "true" : "false",
                " zoomSource=0x", std::hex, semanticSnapshot.zoom.source.value, std::dec,
                " zoomSourceAvailable=", semanticSnapshot.zoom.source.valid ? "true" : "false",
                " nativeTarget=", semanticSnapshot.dialogue.nativeTarget,
                " eventSequence=", semanticSnapshot.generation.eventSequence, ".");
            previousSnapshot = semanticSnapshot;
        } else {
            previousSnapshot = snapshot;
        }
        previousSnapshotValid = true;
#endif
    }
#endif

    void ReplayManualTransition(SafetyHookContext& context)
    {
        if (!g_gameplayHookGate.load(std::memory_order_acquire) ||
            g_stopping.load(std::memory_order_acquire)) return;
#ifdef HORPLUS_CINEMATIC_WRITER_TRACE_DIAGNOSTIC
        const float horPlusTraceBeforeFov = context.xmm0.f32[0];
        TraceHorPlusCinematicWriterBefore(context);
#endif
#if defined(POST_EXIT_RAW_TRACE_DIAGNOSTIC) || defined(POST_EXIT_INTERPOLATED_FOV_PRODUCER_TRACE)
        ObservePostExitRawWriterEntry(context);
#endif
        ObservePostCinematicDialogueRecovery(context);
        if (!g_config.gameplayEnabled) {
            float aspect = 0.0f;
            const auto source = static_cast<std::uintptr_t>(context.rsi);
            if (SafeRead(source + kAspectOffset, aspect) && IsValidAspect(aspect)) {
                UpdateGameplayAspectRestorationState(aspect, source);
            }
            return;
        }
        if (g_runtimeGameplayMode.load(std::memory_order_acquire) ==
            config::GameplayMode::HorPlus) {
            ApplyPendingGameplayModeTransition(context);
            const auto applyResult = ApplyHorPlusGameplay(context);
#ifdef HORPLUS_FOV_STATE_DIAGNOSTIC
            TraceHorPlusFovState(applyResult);
#endif
#ifdef HORPLUS_CINEMATIC_WRITER_TRACE_DIAGNOSTIC
            TraceHorPlusCinematicWriterAfter(context, horPlusTraceBeforeFov);
#endif
            return;
        }
#ifndef POST_EXIT_DELAYED_ASPECT_DIAGNOSTIC
        if (TryAtomicExitHandoff(context)) return;
#endif
        const auto state = g_coordinator.load(std::memory_order_acquire);
        if (state == CoordinatorState::CinematicActive) {
#ifdef HORPLUS_CINEMATIC_WRITER_TRACE_DIAGNOSTIC
            TraceHorPlusCinematicWriterAfter(context, horPlusTraceBeforeFov);
#endif
            return;
        }
        if (state == CoordinatorState::CinematicExiting) {
            const auto source = static_cast<std::uintptr_t>(context.rsi);
            std::uint8_t flags{};
            float currentAspect = 0.0f;
            SafeRead(source + kFlagsOffset, flags);
            SafeRead(source + kAspectOffset, currentAspect);
            const float currentFov = context.xmm0.f32[0];
            const float targetFov = g_exitTargetFov.load(std::memory_order_acquire);
            const float delta = std::fabs(currentFov - targetFov);
            if (flags == 0x04 && std::isfinite(currentFov) && std::isfinite(targetFov) && delta <= kRecoveryEpsilon) {
                g_coordinator.store(CoordinatorState::Gameplay, std::memory_order_release);
                {
                    std::lock_guard dialogueLock(g_dialogueMutex);
                    ResetDialogueRuntimeState();
                }
                Log("Global coordinator: native recovery complete; delta=", delta,
                    " target=", targetFov, " current=", currentFov,
                    ". Same writer invocation returned without replay.");
#ifdef POST_EXIT_DELAYED_ASPECT_DIAGNOSTIC
                ApplyDelayedAspectDiagnostic(source, currentFov, currentAspect, flags);
#endif
#ifdef POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_TEST
#if defined(POST_CINEMATIC_GAMEPLAY_REPLAY_AT_RECOVERY_TEST) && \
    !defined(POST_CINEMATIC_GAMEPLAY_REPLAY_ATOMIC_EXIT_HANDOFF_TEST)
                if (g_gameplayAvailable.load(std::memory_order_acquire)) {
                    const auto source = static_cast<std::uintptr_t>(context.rsi);
                    std::uint8_t currentFlags{};
                    float currentAspect = 0.0f;
                    if (SafeRead(source + kAspectOffset, currentAspect) &&
                        SafeRead(source + kFlagsOffset, currentFlags) &&
                        WriteAspectAndFlags(source, kNativeAspect, 0x4)) {
                        g_state.store(ReplayState::Complete, std::memory_order_release);
                        g_lastAutoRestoreSource.store(source, std::memory_order_release);
                        Log("AtomicReplayApplied: phase=RecoveryComplete source=0x", std::hex,
                            source, std::dec, " fov=", currentFov, " aspect=", kNativeAspect,
                            " flags=0x4 previousAspect=", currentAspect, " previousFlags=0x",
                            std::hex, static_cast<unsigned>(currentFlags), std::dec, ".");
                    } else {
                        Log("AtomicReplayRefused: phase=RecoveryComplete final aspect/flags fields were not writable.");
                    }
                }
#elif !defined(POST_CINEMATIC_GAMEPLAY_REPLAY_ATOMIC_EXIT_HANDOFF_TEST)
                if (g_gameplayAvailable.load(std::memory_order_acquire)) ArmDeferredGameplayReplay(targetFov);
#endif
#endif
            }
            return;
        }
#ifdef POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_TEST
        if (TryDeferredGameplayReplay(context)) return;
#endif
        ReplayManualTransitionOriginal(context);
#ifdef HORPLUS_CINEMATIC_WRITER_TRACE_DIAGNOSTIC
        TraceHorPlusCinematicWriterAfter(context, horPlusTraceBeforeFov);
#endif
    }

#ifdef ZOOM_TRANSITION_DIAGNOSTIC
    constexpr char kZoomTransitionGameSha256[] =
        "61BC1E030740CEBC30CF1DAD0C86CF65E39E12FF0500225821D684181E08D56B";
    constexpr std::size_t kZoomTransitionPostLoadOffset = 13;

    struct ZoomTransitionSnapshot
    {
        bool valid{};
        std::uintptr_t rcx{};
        std::uintptr_t rsi{};
        std::uintptr_t rax{};
        float rax4c{};
        float rax50{};
        float rsi138{};
        float rsi13c{};
        float xmm0{};
        float xmm1{};
        float xmm6{};
    };

    thread_local ZoomTransitionSnapshot g_previousZoomIn{};
    thread_local ZoomTransitionSnapshot g_previousZoomOut{};

    bool ZoomTransitionFloatChanged(float current, float previous)
    {
        return std::isfinite(current) != std::isfinite(previous) ||
            (std::isfinite(current) && std::isfinite(previous) &&
                std::fabs(current - previous) > 0.01f);
    }

    void TraceZoomTransition(SafetyHookContext& context, bool entering)
    {
        if (!diagnostics::Enabled()) return;
#ifdef ZOOM_TRANSITION_DIAGNOSTIC
        auto& hitCounter = entering ? g_zoomInHits : g_zoomOutHits;
        hitCounter.fetch_add(1, std::memory_order_relaxed);
#endif

        const auto rax = static_cast<std::uintptr_t>(context.rax);
        const auto rsi = static_cast<std::uintptr_t>(context.rsi);
        ZoomTransitionSnapshot current{
            true,
            static_cast<std::uintptr_t>(context.rcx),
            rsi,
            rax,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            context.xmm0.f32[0],
            context.xmm1.f32[0],
            context.xmm6.f32[0] };
        if (!rax || !rsi ||
            !SafeRead(rax + 0x4C, current.rax4c) ||
            !SafeRead(rax + 0x50, current.rax50) ||
            !SafeRead(rsi + 0x138, current.rsi138) ||
            !SafeRead(rsi + 0x13C, current.rsi13c))
            return;

#ifdef ZOOM_TRANSITION_DIAGNOSTIC
        auto& previous = entering ? g_previousZoomIn : g_previousZoomOut;
        const bool changed = !previous.valid || previous.rcx != current.rcx ||
            previous.rsi != current.rsi || previous.rax != current.rax ||
            ZoomTransitionFloatChanged(current.rax4c, previous.rax4c) ||
            ZoomTransitionFloatChanged(current.rax50, previous.rax50) ||
            ZoomTransitionFloatChanged(current.rsi138, previous.rsi138) ||
            ZoomTransitionFloatChanged(current.rsi13c, previous.rsi13c) ||
            ZoomTransitionFloatChanged(current.xmm0, previous.xmm0) ||
            ZoomTransitionFloatChanged(current.xmm1, previous.xmm1) ||
            ZoomTransitionFloatChanged(current.xmm6, previous.xmm6);
        if (changed) {
            const auto sequence = g_zoomLoggedEdges.fetch_add(1, std::memory_order_relaxed) + 1;
#ifdef CAMERA_STATE_SNAPSHOT_DIAGNOSTIC
            g_runtime.snapshotZoomDirection.store(entering ? 1 : 2, std::memory_order_release);
            g_runtime.snapshotZoomSequence.store(sequence, std::memory_order_release);
            g_runtime.snapshotZoomSource.store(rsi, std::memory_order_release);
            g_runtime.snapshotZoomPrimary.store(current.rax4c, std::memory_order_release);
            g_runtime.snapshotZoomSecondary.store(current.rax50, std::memory_order_release);
#endif
            Log(entering ? "ZOOM_IN" : "ZOOM_OUT",
                " seq=", sequence,
                " thread=", GetCurrentThreadId(),
                " rcx=0x", std::hex, current.rcx,
                " rsi=0x", current.rsi, " rax=0x", current.rax, std::dec,
                " rax4c=", current.rax4c, " rax50=", current.rax50,
                " rsi138=", current.rsi138, " rsi13c=", current.rsi13c,
                " xmm0=", current.xmm0, " xmm1=", current.xmm1,
                " xmm6=", current.xmm6,
                " coordinator=", CoordinatorStateName(g_coordinator.load(std::memory_order_acquire)),
                " cinematicRecovery=", ReplayStateName(g_state.load(std::memory_order_acquire)), ".");
        }
        previous = current;
#endif
    }

    void TraceZoomIn(SafetyHookContext& context) { TraceZoomTransition(context, true); }
    void TraceZoomOut(SafetyHookContext& context) { TraceZoomTransition(context, false); }

    bool InstallZoomTransitionDiagnostic(const std::string& gameHash)
    {
        if (!diagnostics::Enabled()) return false;
        if (_stricmp(gameHash.c_str(), kZoomTransitionGameSha256) != 0) {
            Log("Zoom transition diagnostic rejected: executable hash mismatch.");
            return false;
        }
        const auto inMatches = hooks::FindAll(g_executable, ZoomIn);
        const auto outMatches = hooks::FindAll(g_executable, ZoomOut);
        if (inMatches.size() != 1 || outMatches.size() != 1) {
            Log("Zoom transition diagnostic rejected: inMatches=", inMatches.size(),
                " outMatches=", outMatches.size(), ".");
            return false;
        }
        const auto validatePostLoad = [](std::uint8_t* match) {
            constexpr std::uint8_t expected[] = { 0x0F, 0x2E, 0xC8 };
            return std::memcmp(match + kZoomTransitionPostLoadOffset, expected, sizeof(expected)) == 0;
        };
        if (!validatePostLoad(inMatches.front()) || !validatePostLoad(outMatches.front())) {
            Log("Zoom transition diagnostic rejected: post-load instruction contract mismatch.");
            return false;
        }
        g_zoomInHook = safetyhook::create_mid(
            inMatches.front() + kZoomTransitionPostLoadOffset, TraceZoomIn);
        if (!g_zoomInHook || !g_zoomInHook.enable()) {
            g_zoomInHook.reset();
            Log("ZOOM_IN diagnostic hook setup failed.");
            return false;
        }
        g_zoomOutHook = safetyhook::create_mid(
            outMatches.front() + kZoomTransitionPostLoadOffset, TraceZoomOut);
        if (!g_zoomOutHook || !g_zoomOutHook.enable()) {
            g_zoomOutHook.reset();
            g_zoomInHook.reset();
            Log("ZOOM_OUT diagnostic hook setup failed; ZOOM_IN hook rolled back.");
            return false;
        }
        Log("Zoom transition diagnostic hooks installed: ZOOM_IN RVA=0x", std::hex,
            reinterpret_cast<std::uintptr_t>(inMatches.front()) + kZoomTransitionPostLoadOffset -
                reinterpret_cast<std::uintptr_t>(g_executable),
            " ZOOM_OUT RVA=0x", reinterpret_cast<std::uintptr_t>(outMatches.front()) +
                kZoomTransitionPostLoadOffset - reinterpret_cast<std::uintptr_t>(g_executable),
            std::dec, ". Read-only edge telemetry enabled.");
        return true;
    }
#endif

    bool VerifyExecutableAndInstruction()
    {
        const auto resolution = gameplay::ResolveCameraWriter(g_executable);
        if (!resolution.imageValid) return false;
        if (resolution.matches != 1) {
            Log("Camera-writer signature rejected: matches=", resolution.matches, ".");
            return false;
        }
        if (!resolution.writeAddress) return false;
        g_fovWriteAddress = resolution.writeAddress;
        Log("Camera-writer signature validated at RVA=0x", std::hex,
            reinterpret_cast<std::uintptr_t>(g_fovWriteAddress) -
                reinterpret_cast<std::uintptr_t>(g_executable), std::dec, ".");
        return true;
    }

    HorPlusGameplayApplyResult ApplyHorPlusGameplay(SafetyHookContext& context)
    {
        HorPlusGameplayApplyResult result{};
        result.inputFov = context.xmm0.f32[0];
        result.outputFov = result.inputFov;
        const auto coordinator = g_coordinator.load(std::memory_order_acquire);
        if (coordinator != CoordinatorState::Gameplay &&
            coordinator != CoordinatorState::CinematicActive) {
            result.reason = "COORDINATOR_GUARD";
            return result;
        }

        const auto source = static_cast<std::uintptr_t>(context.rsi);
        result.source = source;
        float aspect = 0.0f;
        std::uint8_t flags = 0;
        if (!source || !SafeRead(source + kAspectOffset, aspect) ||
            !SafeRead(source + kFlagsOffset, flags)) {
            result.reason = "SOURCE_READ_FAILED";
            return result;
        }
        result.sourceReadable = true;
        result.aspect = aspect;
        result.flags = flags;

        const auto previousSource = g_lastGameplayCameraSource.exchange(
            source, std::memory_order_acq_rel);
        const auto previousFov = g_lastGameplayCameraFov.exchange(
            result.inputFov, std::memory_order_acq_rel);
        const auto contextChange = gameplay::EvaluateGameplayContextChange(
            previousSource, previousFov, source, result.inputFov, kDialogueContextFovJump);
        if (contextChange.InvalidatesDialogue()) {
            std::lock_guard dialogueLock(g_dialogueMutex);
            if (g_dialoguePhase != DialoguePhase::Inactive) {
                Log("Dialogue runtime invalidated by gameplay camera context change: sourceChanged=",
                    contextChange.sourceChanged, " previousSource=0x", std::hex, previousSource,
                    " source=0x", source, std::dec, " previousFOV=", previousFov,
                    " currentFOV=", result.inputFov, ". Native pass-through until a new descent.");
                ResetDialogueRuntimeState();
            }
        }

        // Gameplay.Mode=HorPlus returns through this path without reaching
        // ReplayManualTransitionOriginal. Retain the latest valid native
        // Gameplay observation here so a later cinematic ENTER can consume
        // the established baseline instead of falling back to NativeHorPlus.
        if (coordinator == CoordinatorState::Gameplay &&
            IsValidFovValue(result.inputFov) && IsValidAspect(aspect)) {
            UpdateGameplayAspectRestorationState(aspect, source);
        }

        bool cachedEnter = false;
        if (coordinator == CoordinatorState::CinematicActive) {
            const auto cinematicFov = g_cinematicTransformedFov.load(std::memory_order_acquire);
            if (std::isfinite(cinematicFov) &&
                std::fabs(context.xmm0.f32[0] - cinematicFov) <= kDialogueTransformEpsilon) {
                result.reason = "CINEMATIC_CACHED_VALUE";
                cachedEnter = true;
            }
        }

        if (!cachedEnter) {
            auto transform = gameplay::EvaluateHorPlus(result.inputFov, aspect, flags, kNativeAspect);
            result.eligible = transform.eligible;
            result.applied = transform.applied;
            result.gameplayStateEstablished = coordinator == CoordinatorState::Gameplay;
            result.outputFov = transform.outputFov;
            result.reason = result.applied ? "GAMEPLAY_FOV_CHANGED" :
                (result.eligible ? "TRANSFORM_RESULT_REJECTED" : "INPUT_BYPASS");
            if (result.applied) context.xmm0.f32[0] = result.outputFov;
        }

        if (IsValidFovValue(result.inputFov) && IsValidFovValue(result.outputFov) &&
            IsValidAspect(result.aspect)) {
            camera::CameraFovObservation observation{};
            observation.boundary = camera::FovObservationBoundary::CameraWriter;
            observation.inputFov = {
                result.inputFov,
                cachedEnter ? camera::FovSpace::Transformed : camera::FovSpace::Native,
                cachedEnter ? camera::FovProvenance::CachedCinematicEnter :
                    camera::FovProvenance::NativeRegisterInput,
                true };
            observation.resultFov = {
                result.outputFov,
                cachedEnter ? camera::FovSpace::Transformed :
                    result.applied ? camera::FovSpace::Transformed : camera::FovSpace::Native,
                cachedEnter ? camera::FovProvenance::PassThroughResult :
                    result.applied ? camera::FovProvenance::ModTransformResult :
                        camera::FovProvenance::PassThroughResult,
                true };
            observation.aspect = { result.aspect, camera::FovProvenance::ResolvedAspect, true };
            observation.writerSource = { result.source, result.source != 0 };
            observation.writerFlags = result.flags;
            observation.writerFlagsValid = true;
            result.observation = g_fovObservationStore.Publish(observation);
            result.observationPublished = true;
            result.baselineProjection = g_gameplayBaselineStore.Project(
                result.observation, coordinator == CoordinatorState::Gameplay);
        }
        return result;
    }

#ifdef POST_EXIT_GAMEPLAY_OBSERVER_DIAGNOSTIC
#ifdef HORPLUS_GAMEPLAY_DIAGNOSTIC
    struct HorPlusDiagnosticSnapshot
    {
        bool valid{};
        std::uintptr_t source{};
        float inputFov{};
        float transformedFov{};
        float aspect{};
        std::uint8_t flags{};
        bool eligible{};
        bool applied{};
    };

    thread_local HorPlusDiagnosticSnapshot g_previousHorPlusDiagnostic{};

    void ApplyHorPlusGameplayDiagnostic(SafetyHookContext& context)
    {
        g_horPlusDiagnosticWriterCallbacks.fetch_add(1, std::memory_order_relaxed);
        const auto source = static_cast<std::uintptr_t>(context.rsi);
        float aspect = 0.0f;
        std::uint8_t flags = 0;
        if (!source || !SafeRead(source + kAspectOffset, aspect) ||
            !SafeRead(source + kFlagsOffset, flags) ||
            !std::isfinite(aspect))
            return;

        const float inputFov = context.xmm0.f32[0];
        const auto floatChanged = [](float current, float previous) {
            return std::isfinite(current) != std::isfinite(previous) ||
                (std::isfinite(current) && std::isfinite(previous) &&
                    std::fabs(current - previous) > 0.01f);
        };
        const bool inputFovValid = std::isfinite(inputFov) && inputFov > 1.0f && inputFov < 179.0f;
        const bool isUltrawide = gameplay::IsUltrawideAspect(aspect, kNativeAspect);
        const bool validHorPlusFlags = flags == 0x4 || flags == 0x5;
        const bool eligible =
            g_coordinator.load(std::memory_order_acquire) == CoordinatorState::Gameplay &&
            isUltrawide && validHorPlusFlags && inputFovValid;
        float transformedFov = std::numeric_limits<float>::quiet_NaN();
        bool applied = false;
        if (eligible) {
            transformedFov = camera::HorPlus(inputFov, aspect, kNativeAspect);
            if (std::isfinite(transformedFov) && transformedFov > 1.0f && transformedFov < 179.0f) {
                context.xmm0.f32[0] = transformedFov;
                g_horPlusDiagnosticApplications.fetch_add(1, std::memory_order_relaxed);
                applied = true;
            }
        }

        const bool changed = !g_previousHorPlusDiagnostic.valid ||
            g_previousHorPlusDiagnostic.source != source ||
            floatChanged(inputFov, g_previousHorPlusDiagnostic.inputFov) ||
            floatChanged(transformedFov, g_previousHorPlusDiagnostic.transformedFov) ||
            floatChanged(aspect, g_previousHorPlusDiagnostic.aspect) ||
            flags != g_previousHorPlusDiagnostic.flags ||
            eligible != g_previousHorPlusDiagnostic.eligible ||
            applied != g_previousHorPlusDiagnostic.applied;
        if (changed) {
            const auto changes = g_horPlusDiagnosticChanges.fetch_add(
                1, std::memory_order_relaxed) + 1;
            Log("HORPLUS_DIAG change: source=0x", std::hex, source, std::dec,
                " inputFov=", inputFov, " transformedFov=", transformedFov,
                " aspect=", aspect, " flags=0x", std::hex, static_cast<int>(flags),
                std::dec, " eligible=", eligible ? "YES" : "NO",
                " applied=", applied ? "YES" : "NO",
                std::dec, " callbacks=", g_horPlusDiagnosticWriterCallbacks.load(std::memory_order_relaxed),
                " applications=", g_horPlusDiagnosticApplications.load(std::memory_order_relaxed),
                " changes=", changes, ".");
        }
        g_previousHorPlusDiagnostic = HorPlusDiagnosticSnapshot{
            true, source, inputFov, transformedFov, aspect, flags, eligible, applied };
    }
#endif

    void ObservePostExitDiagnosticWriter(SafetyHookContext& context)
    {
        // Diagnostic-only observer. It never calls the production
        // replay/correction path or writes aspect/flags. The optional HorPlus
        // POC changes only the diagnostic hook's XMM0 input.
#if defined(POST_EXIT_RAW_TRACE_DIAGNOSTIC) || defined(POST_EXIT_INTERPOLATED_FOV_PRODUCER_TRACE)
        ObservePostExitRawWriterEntry(context);
#endif
#ifdef HORPLUS_GAMEPLAY_DIAGNOSTIC
        ApplyHorPlusGameplayDiagnostic(context);
#endif
    }

    bool InstallPostExitGameplayObserverDiagnostic()
    {
        if (!VerifyExecutableAndInstruction()) {
            Log("Post-EXIT diagnostic gameplay observer refused: validated writer resolution failed.");
            return false;
        }
        g_postExitGameplayObserverHook = safetyhook::create_mid(
            g_fovWriteAddress, ObservePostExitDiagnosticWriter);
        if (!g_postExitGameplayObserverHook) {
            Log("Post-EXIT diagnostic gameplay observer refused: hook creation failed.");
            g_fovWriteAddress = nullptr;
            return false;
        }
        Log("Post-EXIT diagnostic gameplay observer installed: read-only; production Gameplay remains disabled.");
        return true;
    }
#endif

#ifdef DIALOGUE_DISCOVERY_DIAGNOSTIC
    struct DialogueDiscoverySnapshot
    {
        bool valid{};
        std::uintptr_t context{};
        std::uintptr_t receiver{};
        std::uintptr_t receiverVtable{};
        std::uintptr_t rdx{};
        float xmm6{};
        float xmm1{};
        float fov28{};
        float fov2c{};
        float fov30{};
        std::array<std::uint32_t, 33> words{};
        std::array<bool, 33> readable{};
        CoordinatorState coordinator{CoordinatorState::Gameplay};
        ReplayState replayState{ReplayState::WaitingForAutomaticUpdate};
        DialoguePhase dialoguePhase{DialoguePhase::Inactive};
        DialogueZoomPolicy policy{DialogueZoomPolicy::Native};
        DialogueZoomPolicy activePolicy{DialogueZoomPolicy::Native};
        bool activePolicyValid{};
    };

    thread_local DialogueDiscoverySnapshot g_previousDialogueDiscovery{};
    thread_local std::uint64_t g_dialogueDiscoverySeenResetGeneration{};

    std::int64_t DialogueDiscoveryNowNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void LogDialogueDiscoveryEnd(const char* reason)
    {
        const bool wasActive = g_dialogueDiscoveryActive.exchange(false, std::memory_order_acq_rel);
        if (!wasActive) return;
        const auto startNs = g_dialogueDiscoveryStartNs.load(std::memory_order_acquire);
        const auto elapsedMs = startNs > 0 ? (DialogueDiscoveryNowNs() - startNs) / 1'000'000 : -1;
        Log("DIALOG_DISCOVERY END reason=", reason, " durationMs=", elapsedMs,
            " samples=", g_dialogueDiscoverySamples.load(std::memory_order_relaxed),
            " changes=", g_dialogueDiscoveryChanges.load(std::memory_order_relaxed), ".");
        g_runtime.dialogueDiscoveryResetGeneration.fetch_add(1, std::memory_order_acq_rel);
    }

    void LogDialogueDiscoveryChanges(const DialogueDiscoverySnapshot& current,
        const DialogueDiscoverySnapshot& previous, std::int64_t elapsedUs)
    {
        std::ostringstream changes;
        bool hasChange = false;
        const auto add = [&changes, &hasChange](const char* name) {
            if (hasChange) changes << '|';
            changes << name;
            hasChange = true;
        };
        const auto floatChanged = [](float a, float b) {
            return std::isfinite(a) != std::isfinite(b) ||
                (std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) > 0.01f);
        };
        if (current.context != previous.context) add("context");
        if (current.receiver != previous.receiver) add("receiver");
        if (current.receiverVtable != previous.receiverVtable) add("vtable");
        if (current.rdx != previous.rdx) add("rdx");
        if (floatChanged(current.xmm6, previous.xmm6)) add("xmm6");
        if (floatChanged(current.xmm1, previous.xmm1)) add("xmm1");
        if (floatChanged(current.fov28, previous.fov28)) add("fov28");
        if (floatChanged(current.fov2c, previous.fov2c)) add("fov2c");
        if (floatChanged(current.fov30, previous.fov30)) add("fov30");
        if (current.coordinator != previous.coordinator) add("coordinator");
        if (current.replayState != previous.replayState) add("replay");
        if (current.dialoguePhase != previous.dialoguePhase) add("dialoguePhase");
        if (current.policy != previous.policy) add("policy");
        if (current.activePolicy != previous.activePolicy ||
            current.activePolicyValid != previous.activePolicyValid) add("activePolicy");
        for (std::size_t i = 0; i < current.words.size(); ++i) {
            if (current.readable[i] && previous.readable[i] && current.words[i] != previous.words[i]) {
                add("memory");
                break;
            }
        }
        if (!hasChange) return;

        Log("DIALOG_DISCOVERY SAMPLE elapsedUs=", elapsedUs,
            " changed=", changes.str(), " context=0x", std::hex, current.context,
            " receiver=0x", current.receiver, " vtable=0x", current.receiverVtable,
            " rdx=0x", current.rdx, std::dec, " xmm6=", current.xmm6,
            " xmm1=", current.xmm1, " fov28=", current.fov28, " fov2c=", current.fov2c,
            " fov30=", current.fov30, " coordinator=", CoordinatorStateName(current.coordinator),
            " replay=", ReplayStateName(current.replayState),
            " dialoguePhase=", DialoguePhaseName(current.dialoguePhase),
            " selectedPolicy=", DialogueZoomPolicyName(current.policy),
            " activePolicy=", DialogueZoomPolicyName(current.activePolicy),
            " activePolicyValid=", current.activePolicyValid, ".");
        for (std::size_t i = 0; i < current.words.size(); ++i) {
            if (current.readable[i] && previous.readable[i] && current.words[i] != previous.words[i])
                Log("DIALOG_DISCOVERY MEM elapsedUs=", elapsedUs, " context=0x", std::hex,
                    current.context, " offset=0x", i * sizeof(std::uint32_t), " old=0x",
                    previous.words[i], " new=0x", current.words[i], std::dec, ".");
        }
    }

    void TraceDialogueDiscovery(SafetyHookContext& context)
    {
        if (!g_dialogueDiscoveryActive.load(std::memory_order_acquire)) return;
        const auto resetGeneration = g_runtime.dialogueDiscoveryResetGeneration.load(
            std::memory_order_acquire);
        if (resetGeneration != g_dialogueDiscoverySeenResetGeneration) {
            g_previousDialogueDiscovery = {};
            g_dialogueDiscoverySeenResetGeneration = resetGeneration;
        }
        const auto startNs = g_dialogueDiscoveryStartNs.load(std::memory_order_acquire);
        const auto nowNs = DialogueDiscoveryNowNs();
        if (startNs <= 0 || nowNs - startNs >= kDialogueDiscoveryDurationNs) return;

        const auto source = static_cast<std::uintptr_t>(context.rsi);
        if (!source || source > (std::numeric_limits<std::uintptr_t>::max)() - 0x80) return;
        DialogueDiscoverySnapshot current{};
        current.valid = true;
        current.context = source;
        current.receiver = static_cast<std::uintptr_t>(context.rcx);
        current.rdx = static_cast<std::uintptr_t>(context.rdx);
        SafeRead(current.receiver, current.receiverVtable);
        current.xmm6 = context.xmm6.f32[0];
        current.xmm1 = context.xmm1.f32[0];
        SafeRead(source + 0x28, current.fov28);
        SafeRead(source + 0x2C, current.fov2c);
        SafeRead(source + 0x30, current.fov30);
        for (std::size_t i = 0; i < current.words.size(); ++i)
            current.readable[i] = SafeRead(source + i * sizeof(std::uint32_t), current.words[i]);
        current.coordinator = g_coordinator.load(std::memory_order_acquire);
        current.replayState = g_state.load(std::memory_order_acquire);
        current.policy = g_runtimeDialoguePolicy.load(std::memory_order_acquire);
        {
            std::lock_guard dialogueLock(g_dialogueMutex);
            current.dialoguePhase = g_dialoguePhase;
            current.activePolicy = g_activeDialoguePolicy.Value();
            current.activePolicyValid = g_activeDialoguePolicy.IsValid();
        }
        g_dialogueDiscoverySamples.fetch_add(1, std::memory_order_relaxed);
        if (!g_previousDialogueDiscovery.valid || g_previousDialogueDiscovery.context != current.context) {
            Log("DIALOG_DISCOVERY CONTEXT elapsedUs=", (nowNs - startNs) / 1000,
                " old=0x", std::hex, g_previousDialogueDiscovery.context,
                " new=0x", current.context, std::dec, ". Baseline reset.");
            g_previousDialogueDiscovery = current;
            return;
        }
        const auto previousChanges = g_dialogueDiscoveryChanges.load(std::memory_order_relaxed);
        LogDialogueDiscoveryChanges(current, g_previousDialogueDiscovery, (nowNs - startNs) / 1000);
        if (g_dialogueDiscoveryChanges.load(std::memory_order_relaxed) == previousChanges) {
            bool changed = false;
            for (std::size_t i = 0; i < current.words.size(); ++i)
                changed = changed || (current.readable[i] && g_previousDialogueDiscovery.readable[i] &&
                    current.words[i] != g_previousDialogueDiscovery.words[i]);
            changed = changed || current.context != g_previousDialogueDiscovery.context ||
                current.receiver != g_previousDialogueDiscovery.receiver ||
                current.receiverVtable != g_previousDialogueDiscovery.receiverVtable ||
                current.rdx != g_previousDialogueDiscovery.rdx ||
                current.coordinator != g_previousDialogueDiscovery.coordinator ||
                current.replayState != g_previousDialogueDiscovery.replayState ||
                current.dialoguePhase != g_previousDialogueDiscovery.dialoguePhase ||
                current.policy != g_previousDialogueDiscovery.policy ||
                std::fabs(current.xmm6 - g_previousDialogueDiscovery.xmm6) > 0.01f ||
                std::fabs(current.xmm1 - g_previousDialogueDiscovery.xmm1) > 0.01f ||
                std::fabs(current.fov28 - g_previousDialogueDiscovery.fov28) > 0.01f ||
                std::fabs(current.fov2c - g_previousDialogueDiscovery.fov2c) > 0.01f ||
                std::fabs(current.fov30 - g_previousDialogueDiscovery.fov30) > 0.01f;
            if (changed) g_dialogueDiscoveryChanges.fetch_add(1, std::memory_order_relaxed);
        }
        g_previousDialogueDiscovery = current;
    }

    DWORD WINAPI DialogueDiscoveryLoop(void*)
    {
        bool previousF12 = false;
        while (!WaitForWorkerStop(25)) {
            const bool f12 = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
            if (f12 && !previousF12 && !g_dialogueDiscoveryActive.exchange(true, std::memory_order_acq_rel)) {
                g_dialogueDiscoverySamples.store(0, std::memory_order_release);
                g_dialogueDiscoveryChanges.store(0, std::memory_order_release);
                g_dialogueDiscoveryStartNs.store(DialogueDiscoveryNowNs(), std::memory_order_release);
                g_previousDialogueDiscovery = {};
                Log("DIALOG_DISCOVERY START durationMs=10000 trigger=F12 region=0x00..0x80 granularity=4.");
            }
            previousF12 = f12;
            if (g_dialogueDiscoveryActive.load(std::memory_order_acquire)) {
                const auto startNs = g_dialogueDiscoveryStartNs.load(std::memory_order_acquire);
                if (startNs > 0 && DialogueDiscoveryNowNs() - startNs >= kDialogueDiscoveryDurationNs)
                    LogDialogueDiscoveryEnd("timeout");
            }
        }
        LogDialogueDiscoveryEnd("worker-stop");
        return 0;
    }
#endif

    void SelectGameplayMode(config::GameplayMode newMode)
    {
        const auto oldMode = g_runtimeGameplayMode.load(std::memory_order_acquire);
        const auto coordinator = g_coordinator.load(std::memory_order_acquire);
        const auto plan = gameplay::ResolveGameplayModeTransition(oldMode, newMode, coordinator);
        if (!plan.changed) {
            Log("Gameplay mode selection ignored: mode=", config::GameplayModeName(newMode),
                " reason=no-op.");
            return;
        }

        if (oldMode == config::GameplayMode::AspectRecalculation &&
            newMode == config::GameplayMode::HorPlus) {
            g_state.store(ReplayState::WaitingForAutomaticUpdate, std::memory_order_release);
            g_lastAutoRestoreSource.store(0, std::memory_order_release);
            g_gameplayModeTransitionTarget.store(newMode, std::memory_order_release);
            g_gameplayModeTransitionDeferralLogged.store(false, std::memory_order_release);
            g_gameplayModeTransitionPending.store(true, std::memory_order_release);
        } else if (oldMode == config::GameplayMode::HorPlus &&
            newMode == config::GameplayMode::AspectRecalculation) {
            g_gameplayModeTransitionPending.store(false, std::memory_order_release);
            g_gameplayModeTransitionDeferralLogged.store(false, std::memory_order_release);
            g_state.store(ReplayState::WaitingForAutomaticUpdate, std::memory_order_release);
            g_lastAutoRestoreSource.store(0, std::memory_order_release);
            InvalidateGameplayAspectRestorationState();
            g_lastGameplayCameraSource.store(0, std::memory_order_release);
            g_lastGameplayCameraFov.store(std::numeric_limits<float>::quiet_NaN(),
                std::memory_order_release);
            g_gameplayBaselineStore.Invalidate();
#ifdef HORPLUS_FOV_STATE_DIAGNOSTIC
            {
                std::lock_guard telemetryLock(g_horPlusFovTelemetryMutex);
                g_runtime.horPlusFovTelemetryValid = false;
                g_runtime.horPlusGameplayCacheValid = false;
            }
#endif
        }

        g_runtimeGameplayMode.store(newMode, std::memory_order_release);
        Log("Gameplay mode transition ", config::GameplayModeName(oldMode), " -> ",
            config::GameplayModeName(newMode), " coordinator=", CoordinatorStateName(coordinator),
            " physicalBoundary=", plan.deferPhysicalTransition ? "deferred" : "Gameplay", ".");
    }

    void HotkeyLoop()
    {
        bool previousDialogueKey = false;
        bool previousCinematicKey = false;
        bool previousCinematicFovKey = false;
        bool previousGameplayKey = false;
        while (!WaitForWorkerStop(50)) {
            const bool dialogueKey = (GetAsyncKeyState(g_config.dialogueCycleKey) & 0x8000) != 0;
            const bool cinematicKey = (GetAsyncKeyState(g_config.cinematicCycleKey) & 0x8000) != 0;
            const bool cinematicFovKey = (GetAsyncKeyState(g_config.cinematicFovCycleKey) & 0x8000) != 0;
            const bool gameplayKey = (GetAsyncKeyState(g_config.gameplayCycleKey) & 0x8000) != 0;
            if (g_config.hotkeysEnabled) {
                if (gameplayKey && !previousGameplayKey) {
                    const auto mode = config::NextGameplayMode(
                        g_runtimeGameplayMode.load(std::memory_order_acquire));
                    SelectGameplayMode(mode);
                    if (config::PersistConfigValue(g_configPath, "Gameplay", "Mode", config::GameplayModeName(mode),
                        [](std::string message) { Log(message); }))
                        Log("Hotkey ", HotkeyName(g_config.gameplayCycleKey), ": Gameplay.Mode=",
                            config::GameplayModeName(mode), " persisted.");
                    else
                        Log("Hotkey ", HotkeyName(g_config.gameplayCycleKey), ": Gameplay.Mode=",
                            config::GameplayModeName(mode), " active; persistence failed.");
                }
                if (dialogueKey && !previousDialogueKey) {
                    const auto policy = NextDialogueZoomPolicy(
                        g_runtimeDialoguePolicy.load(std::memory_order_acquire));
                    if (policy != DialogueZoomPolicy::Native &&
                        !g_dialogueNonNativeCapabilityAvailable.load(std::memory_order_acquire)) {
                        Log("Hotkey ", HotkeyName(g_config.dialogueCycleKey),
                            ": Dialogue.Zoom=", DialogueZoomPolicyName(policy),
                            " rejected; required lifecycle/recovery capability unavailable.");
                        previousDialogueKey = dialogueKey;
                        previousCinematicKey = cinematicKey;
                        previousCinematicFovKey = cinematicFovKey;
                        previousGameplayKey = gameplayKey;
                        continue;
                    }
                    g_runtimeDialoguePolicy.store(policy, std::memory_order_release);
                    DialogueZoomPolicy activePolicy = DialogueZoomPolicy::Native;
                    bool activePolicyValid = false;
                    {
                        std::lock_guard dialogueLock(g_dialogueMutex);
                        activePolicy = g_activeDialoguePolicy.Value();
                        activePolicyValid = g_activeDialoguePolicy.IsValid();
                    }
                    if (config::PersistConfigValue(g_configPath, "Dialogue", "Zoom", DialogueZoomPolicyName(policy),
                        [](std::string message) { Log(message); }))
                        Log("Hotkey ", HotkeyName(g_config.dialogueCycleKey), ": Dialogue.Zoom selected=",
                            DialogueZoomPolicyName(policy), " active=", DialogueZoomPolicyName(activePolicy),
                            " activeValid=", activePolicyValid, " persisted.");
                    else
                        Log("Hotkey ", HotkeyName(g_config.dialogueCycleKey), ": Dialogue.Zoom selected=",
                            DialogueZoomPolicyName(policy), " active=", DialogueZoomPolicyName(activePolicy),
                            " activeValid=", activePolicyValid, " active; persistence failed.");
                }
                if (cinematicKey && !previousCinematicKey) {
                    const auto policy = NextCinematicAspectPolicy(
                        g_runtimeCinematicPolicy.load(std::memory_order_acquire));
                    g_runtimeCinematicPolicy.store(policy, std::memory_order_release);
                    if (config::PersistConfigValue(g_configPath, "Cinematics", "AspectRatio", CinematicAspectPolicyName(policy),
                        [](std::string message) { Log(message); }))
                        Log("Hotkey ", HotkeyName(g_config.cinematicCycleKey), ": Cinematics.AspectRatio=", CinematicAspectPolicyName(policy),
                            " persisted for next cinematic.");
                    else
                        Log("Hotkey ", HotkeyName(g_config.cinematicCycleKey), ": Cinematics.AspectRatio=", CinematicAspectPolicyName(policy),
                            " active for next cinematic; persistence failed.");
                }
                if (cinematicFovKey && !previousCinematicFovKey) {
                    const auto mode = config::NextCinematicFovMode(
                        g_runtimeCinematicFovMode.load(std::memory_order_acquire));
                    g_runtimeCinematicFovMode.store(mode, std::memory_order_release);
                    if (config::PersistConfigValue(g_configPath, "Cinematics", "FovMode",
                        config::CinematicFovModeName(mode), [](std::string message) { Log(message); }))
                        Log("Hotkey ", HotkeyName(g_config.cinematicFovCycleKey),
                            ": Cinematics.FovMode=", config::CinematicFovModeName(mode),
                            " persisted for next cinematic.");
                    else
                        Log("Hotkey ", HotkeyName(g_config.cinematicFovCycleKey),
                            ": Cinematics.FovMode=", config::CinematicFovModeName(mode),
                            " active for next cinematic; persistence failed.");
                }
            }
            previousDialogueKey = dialogueKey;
            previousCinematicKey = cinematicKey;
            previousCinematicFovKey = cinematicFovKey;
            previousGameplayKey = gameplayKey;
        }
    }

    DWORD WINAPI Initialize(void*)
    {
        WCHAR modulePath[MAX_PATH]{};
        GetModuleFileNameW(g_module, modulePath, MAX_PATH);
        const auto moduleDirectory = std::filesystem::path(modulePath).remove_filename();
        const auto logPath = moduleDirectory / "STALKER2CameraTweaks.log";
        const auto configPath = config::DefaultConfigPath(moduleDirectory);
        g_configPath = configPath;
        std::ofstream(logPath, std::ios::out | std::ios::trunc).close();
        try {
            g_logger = spdlog::basic_logger_mt("STALKER2CameraTweaks", logPath.string(), true);
            g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] %v");
            g_logger->flush_on(spdlog::level::err);
        } catch (...) { return 0; }

        try {
            if (!CreateWorkerStopEvent())
                throw std::runtime_error("worker stop event could not be created");
            g_stopping.store(false, std::memory_order_release);
            std::string modHash;
            std::string gameHash;
            WCHAR executablePath[MAX_PATH]{};
            GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
            const bool modHashAvailable = ComputeSha256(modulePath, modHash);
            const bool gameHashAvailable = ComputeSha256(executablePath, gameHash);
            Log("Runtime identity: modSha256=", modHashAvailable ? modHash : "unavailable",
                " gameSha256=", gameHashAvailable ? gameHash : "unavailable", ".");
#ifdef POST_EXIT_CAMERA_FOV_WRITE_OWNER_TRACE
            if (!gameHashAvailable || !InstallPostExitFovWriteOwnerTrace(gameHash))
                Log("Post-EXIT FOV physical-write trace: NOT_INSTALLED; production behavior unchanged.");
#endif
#ifdef POST_EXIT_FOV_PRODUCER_STORE_TRACE
            if (!gameHashAvailable || !InstallPostExitProducerStoreTrace(gameHash))
                Log("Post-EXIT FOV producer store trace: NOT_INSTALLED; production behavior unchanged.");
#endif
#ifdef POST_EXIT_FOV_STATE_CONSUMER_TRACE
            if (!gameHashAvailable || !InstallPostExitFovConsumerTrace(gameHash))
                Log("Post-EXIT FOV consumer trace: NOT_INSTALLED; production behavior unchanged.");
#endif
            if (!config::LoadFeatureConfig(configPath, g_config,
                [](const std::filesystem::path& path) {
                    return config::SynchronizeManagedConfigTemplate(path,
                        [](std::string message) { Log(message); });
                },
                [](std::string message) { Log(message); }))
                Log("Configuration unavailable; using defaults with all fixes enabled.");
            diagnostics::SetEnabled(g_config.diagnosticsEnabled);
            g_runtimeCinematicPolicy.store(g_config.cinematicAspectPolicy, std::memory_order_release);
            g_runtimeCinematicFovMode.store(g_config.cinematicFovMode, std::memory_order_release);
            g_runtimeDialoguePolicy.store(g_config.dialogueZoomPolicy, std::memory_order_release);
            g_runtimeGameplayMode.store(g_config.gameplayMode, std::memory_order_release);
            const bool gameplayRequested = g_config.gameplayEnabled;
#ifdef NATIVE_GAMEPLAY_BASELINE_DIAGNOSTIC
            if (!gameplayRequested) {
                g_postExitTraceWriterCount.store(0, std::memory_order_release);
                g_postExitTraceSequence.fetch_add(1, std::memory_order_acq_rel);
                g_postExitTraceStartNs.store(std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count(),
                    std::memory_order_release);
                g_postExitTraceArmed.store(true, std::memory_order_release);
                Log("Native gameplay baseline diagnostic armed: read-only steady-state trace.");
            }
#endif
            const bool cinematicRequested = CinematicAspectOverrideEnabled() ||
                g_config.cinematicFovMode == config::CinematicFovMode::GameplayHorPlus ||
                g_config.hotkeysEnabled;
            const bool dialogueRequested = g_config.dialogueZoomPolicy != DialogueZoomPolicy::Native ||
                g_config.hotkeysEnabled;
            const bool nonNativeDialogueRequested =
                g_config.dialogueZoomPolicy != DialogueZoomPolicy::Native;
            auto gameplayStatus = gameplayRequested ? FeatureStatus::Failed : FeatureStatus::Disabled;
            auto cinematicStatus = cinematicRequested ? FeatureStatus::Failed : FeatureStatus::Disabled;
            auto dialogueStatus = dialogueRequested ? FeatureStatus::Failed : FeatureStatus::Disabled;
            auto hotkeyStatus = g_config.hotkeysEnabled ? FeatureStatus::Failed : FeatureStatus::Disabled;
            auto diagnosticStatus = FeatureStatus::Disabled;
#ifdef ZOOM_TRANSITION_DIAGNOSTIC
            const bool zoomTransitionInstalled = g_config.diagnosticsEnabled && gameHashAvailable &&
                InstallZoomTransitionDiagnostic(gameHash);
            if (!g_config.diagnosticsEnabled) {
                Log("Diagnostics disabled; supported telemetry hooks not installed.");
            } else if (!zoomTransitionInstalled) {
                Log("Zoom transition diagnostic: NOT_INSTALLED; production behavior unchanged.");
            } else {
                diagnosticStatus = FeatureStatus::Available;
            }
#endif
            Log("Configuration: Gameplay.Enabled=", g_config.gameplayEnabled,
                " Gameplay.Mode=", config::GameplayModeName(g_config.gameplayMode),
                " Cinematic.AspectRatio=", CinematicAspectPolicyName(g_config.cinematicAspectPolicy),
                " Cinematic.FovMode=", config::CinematicFovModeName(g_config.cinematicFovMode),
                " Dialogue.Zoom=", DialogueZoomPolicyName(g_config.dialogueZoomPolicy),
                " Diagnostics.Enabled=", g_config.diagnosticsEnabled ? "true" : "false", ".");
            Log("Gameplay aspect fix loaded. FOV is preserved from the game's settings.");
            cinematicStatus = cinematics::InitializeTransactional(cinematicRequested,
                InstallCinematicAspectComponent, InstallCinematicFovComponent,
                RollbackCinematicAspectComponent, RollbackCinematicFovComponent,
                CommitCinematicComponents);
            bool lifecycleObservationAvailable = cinematicStatus == FeatureStatus::Available;
            if (!cinematicRequested && (nonNativeDialogueRequested || g_config.hotkeysEnabled)) {
                try {
                    if (!InstallCinematicFovComponent())
                        throw std::runtime_error("cinematic lifecycle observation setup failed");
                    if (!CommitCinematicObservationOnly())
                        throw std::runtime_error("cinematic lifecycle observation commit failed");
                    lifecycleObservationAvailable = true;
                    Log("Cinematic lifecycle observation installed; presentation intervention disabled.");
                } catch (...) {
                    RollbackCinematicFovComponent();
                    Log("Cinematic lifecycle observation unavailable; presentation intervention unchanged.");
                }
            }
            g_cinematicLifecycleObservationAvailable.store(lifecycleObservationAvailable,
                std::memory_order_release);
            if (cinematicStatus == FeatureStatus::Available)
                Log("Cinematics feature initialized transactionally: policy=",
                    CinematicAspectPolicyName(g_config.cinematicAspectPolicy), ".");
            else if (cinematicStatus == FeatureStatus::Failed)
                Log("Cinematics feature failed transactionally; native cinematic behavior retained.");
            else
                Log("Cinematics policy is Native; cinematic override hooks bypassed.");
            if (gameplayRequested) {
                try {
                    if (!VerifyExecutableAndInstruction())
                        throw std::runtime_error("camera-writer signature or validated FOV instruction did not match");
                    Log("Installing validated gameplay hook.");
                    auto gameplayHook = safetyhook::MidHook::create(
                        g_fovWriteAddress, SafeMidHookEntry<&ReplayManualTransition>,
                        safetyhook::MidHook::StartDisabled);
                    if (!gameplayHook) throw std::runtime_error("validated gameplay hook creation failed");
                    g_hook = std::move(*gameplayHook);
                    gameplayStatus = FeatureStatus::Available;
                    g_gameplayAvailable.store(true, std::memory_order_release);
                    if (!g_hook.enable()) throw std::runtime_error("validated gameplay hook activation failed");
                    g_gameplayHookGate.store(true, std::memory_order_release);
                    Log("Validated gameplay hook installed: true.");
                } catch (...) {
                    g_hook.reset();
                    g_fovWriteAddress = nullptr;
                    gameplayStatus = FeatureStatus::Failed;
                    g_gameplayAvailable.store(false, std::memory_order_release);
                    Log("Gameplay feature failed locally; gameplay correction is unavailable.");
                }
            } else {
                gameplayStatus = FeatureStatus::Disabled;
                Log("Gameplay aspect fix disabled by configuration; camera-writer observer bypassed.");
#ifdef POST_EXIT_GAMEPLAY_OBSERVER_DIAGNOSTIC
                if (!InstallPostExitGameplayObserverDiagnostic())
                    Log("Post-EXIT diagnostic gameplay observer: NOT_INSTALLED; production behavior unchanged.");
#endif
            }
            const bool nonNativeDialogueCapability =
                plugin::DialogueLifecycleCapabilityAvailable(
                    true, lifecycleObservationAvailable,
                    gameplayStatus == FeatureStatus::Available);
            g_dialogueNonNativeCapabilityAvailable.store(nonNativeDialogueCapability,
                std::memory_order_release);
            if (dialogueRequested) {
                const bool currentDialogueCapability =
                    plugin::DialogueLifecycleCapabilityAvailable(
                        nonNativeDialogueRequested, lifecycleObservationAvailable,
                        gameplayStatus == FeatureStatus::Available);
                if (!currentDialogueCapability) {
                    dialogueStatus = FeatureStatus::Failed;
                    Log("Dialogue feature unavailable: non-Native Dialogue requires "
                        "cinematic lifecycle observation and Gameplay recovery observation.");
                } else {
                    try {
                        if (!InstallDialogueBoundary())
                            throw std::runtime_error("dialogue boundary setup failed");
                        dialogueStatus = FeatureStatus::Available;
                    } catch (...) {
                        g_dialogueBoundaryHook.reset();
                        dialogueStatus = FeatureStatus::Failed;
                        Log("Dialogue feature failed locally; native dialogue behavior retained.");
                    }
                }
            } else {
                dialogueStatus = FeatureStatus::Disabled;
                Log("Dialogue policy is Native; dialogue boundary hook bypassed.");
            }
            Log("Initialization summary: Gameplay=", FeatureStatusName(gameplayStatus),
                " Cinematics=", FeatureStatusName(cinematicStatus),
                " Dialogue=", FeatureStatusName(dialogueStatus), ".");
            if (g_config.hotkeysEnabled) {
                if (!StartWorker(
                    [](void*) -> DWORD { HotkeyLoop(); return 0; }, "hotkey"))
                    Log("Hotkey feature failed locally; production hooks remain active.");
                else
                    hotkeyStatus = FeatureStatus::Available;
                    Log("Hotkeys enabled: F9=Gameplay mode cycle, F10=Cinematics aspect cycle, F11=Cinematics FOV cycle for next cinematic, F12=Dialogue cycle.");
            } else {
                hotkeyStatus = FeatureStatus::Disabled;
                Log("Hotkeys disabled by configuration.");
            }
#ifdef GAMEPLAY_ONE_SHOT_CINEMATIC_TRIGGER
            diagnosticStatus = FeatureStatus::Failed;
            if (!StartWorker(OneShotTriggerLoop, "one-shot trigger"))
                Log("Diagnostic worker failed locally; production behavior continues.");
            else {
                diagnosticStatus = FeatureStatus::Available;
                Log("Diagnostic one-shot trigger enabled: press F7 to arm; no timer or repeated flag write is used.");
            }
#ifdef COMBINED_GAMEPLAY_DIAGNOSTIC
            if (!StartWorker(ResolutionMonitorLoop, "resolution monitor")) {
                diagnosticStatus = FeatureStatus::Failed;
                Log("Diagnostic resolution worker failed locally; production behavior continues.");
            } else
                Log("Combined diagnostic resolution monitor enabled: display/window/client changes are logged automatically.");
#endif
#endif
#ifdef DIALOGUE_DISCOVERY_DIAGNOSTIC
            diagnosticStatus = FeatureStatus::Failed;
            if (!StartWorker(DialogueDiscoveryLoop, "dialogue discovery"))
                Log("Dialogue discovery worker failed locally; production behavior continues.");
            else {
                diagnosticStatus = FeatureStatus::Available;
                Log("Dialogue discovery enabled: press F12 for one bounded 10-second read-only trace.");
            }
#endif
            Log("Initialization status: Gameplay=", FeatureStatusName(gameplayStatus),
                " Cinematics=", FeatureStatusName(cinematicStatus),
                " Dialogue=", FeatureStatusName(dialogueStatus),
                " Hotkeys=", FeatureStatusName(hotkeyStatus),
                " Diagnostics=", FeatureStatusName(diagnosticStatus), ".");
            g_logger->flush();
        } catch (const std::exception& exception) {
            ResetAllRuntimeResources();
            Log("Gameplay hook setup failed safely: ", exception.what());
            g_logger->flush();
        } catch (...) {
            ResetAllRuntimeResources();
            Log("Gameplay hook setup failed safely with an unknown exception.");
            g_logger->flush();
        }
        return 0;
    }
}

namespace plugin
{
    void SetModuleHandle(HMODULE module)
    {
        g_module = module;
    }

    DWORD WINAPI InitializeThread(void* parameter)
    {
        return Initialize(parameter);
    }

    void Shutdown()
    {
        ResetAllRuntimeResources();
        if (g_logger) g_logger->flush();
    }

    void NotifyProcessDetach(bool processTerminating)
    {
        if (processTerminating) return;
        g_stopping.store(true, std::memory_order_release);
        g_gameplayHookGate.store(false, std::memory_order_release);
        g_cinematicHookGate.store(false, std::memory_order_release);
        g_workers.SignalStop();
    }
}
