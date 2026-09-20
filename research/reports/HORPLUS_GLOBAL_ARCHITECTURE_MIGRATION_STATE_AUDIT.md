# Global HorPlus / FOV Architecture Migration State Audit

Date: 2026-09-19  
Scope: read-only source/history audit after GameplayBaseline Batch 2A, 2A.1 and Cinematic ENTER Batch 2B.

## Executive result

The repository now has one shared HorPlus mathematical primitive and two production decision seams already integrated:

1. `src/plugin/runtime.cpp::ApplyHorPlusGameplay` — per-sample Gameplay camera-writer transform.
2. `src/plugin/runtime.cpp::TraceCinematicEnter` — cinematic ENTER transform using the selected baseline.

The first seam transforms native Gameplay writer input and publishes a factual `CameraWriter` observation. The second reads the retained `GameplayBaseline` when `GameplayHorPlus` is selected, applies the unified cinematic transfer, and publishes a `CinematicEnter` observation.

This is a migration state audit, not a cleanup proposal. Legacy state remains in place where it still has independent responsibilities. No source behavior was changed by this audit.

## Current architecture map

```text
native FOV evidence
        |
        +--> Gameplay CameraWriter
        |       |
        |       +--> Gameplay HorPlus decision (when eligible)
        |       +--> CameraFovObservationStore[CameraWriter]
        |       +--> GameplayBaselineStore projection
        |
        +--> Cinematic ENTER
                |
                +--> select authored/native or retained Gameplay baseline
                +--> unified cinematic FOV transfer + HorPlus
                +--> CameraFovObservationStore[CinematicEnter]

Dialogue and ZOOM remain separate evidence/lifecycle systems.
Cinematic EXIT/recovery remains a lifecycle boundary, not a HorPlus transform.
```

The factual observation layer is in `src/camera/fov_observation.hpp/.cpp`. It stores per-boundary observations with input/result FOV, aspect, source token, space and provenance. `GameplayBaselineStore` in `src/camera/gameplay_baseline.hpp/.cpp` is a retained semantic projection of eligible Gameplay writer observations. `CameraStateSnapshot` is diagnostic composition only and is not a production owner.

## The two already-migrated production integration points

These are established from current source, not assumed from earlier plans.

### 1. Gameplay writer: `ApplyHorPlusGameplay`

Callsite: `src/plugin/runtime.cpp`, `ApplyHorPlusGameplay` (approximately lines 3146-3285), reached from `ReplayManualTransition` when `Gameplay.Mode=HorPlus`.

Evidence that this is the new global path:

- native input is read from the writer callback context (`XMM0`);
- the production transform owner is `gameplay::EvaluateHorPlus` / `TryTransformHorPlus`;
- the result is written back to the same writer context;
- the callback publishes `CameraWriter` factual observation into `CameraFovObservationStore`;
- when coordinator ownership is Gameplay, the same observation is projected into `GameplayBaselineStore`.

Lifetime is per writer sample. The transform uses the writer object's aspect and validated flags. ADS and binocular trajectories therefore remain ordinary Gameplay native samples; `ZOOM_IN/ZOOM_OUT` are not owners and do not supply FOV values.

### 2. Cinematic ENTER: `TraceCinematicEnter`

Callsite: `src/plugin/runtime.cpp`, `TraceCinematicEnter` (approximately lines 1808-1885).

Evidence that this is the second new global decision seam:

- it reads one coherent retained `GameplayBaseline` snapshot;
- it selects the target baseline through `SelectCinematicBaseline`;
- it resolves the effective cinematic aspect with `ResolveCinematicAspect`, rather than substituting gameplay aspect;
- it calls the shared `TryTransformCinematicFov` once and writes the result to the ENTER callback;
- it publishes a `CinematicEnter` observation and updates the existing transformed ENTER cache.

Lifetime is the ENTER transition/cache boundary, not a per-frame Gameplay writer decision. `NativeHorPlus` is the identity baseline case; `GameplayHorPlus` selects the retained Gameplay native baseline when valid and otherwise falls back to the authored ENTER reference.

The CameraWriter observation has several producer paths, including AspectRecalculation pass-through and cached transformed-writer diagnostics. These are one factual boundary, not additional global HorPlus decision seams.

## HorPlus and aspect-aware transform inventory

| Location / function | Input/native source | Aspect source | Owner/output | Lifetime | Status / migration risk |
|---|---|---|---|---|---|
| `src/cinematics/cinematic_fov.cpp::HorPlus` | caller-provided native FOV | caller-provided aspect and native aspect | shared tangent-space projection | per call | migrated common primitive |
| `src/gameplay/horplus_gameplay.cpp::EvaluateHorPlus` | Gameplay writer `XMM0` | writer object field plus validated flags | Gameplay writer output | per sample | migrated production seam |
| `src/plugin/runtime.cpp::ApplyHorPlusGameplay` | `context.xmm0` | `RSI + aspect offset`, flags | writes writer `XMM0`; publishes observation | per sample | migrated production owner |
| `src/plugin/runtime.cpp::TraceCinematicEnter` | authored/native ENTER value | `ResolveCinematicAspect()` policy | ENTER writer output and cache | transition | migrated production seam |
| `src/cinematics/cinematic_fov.cpp::TryTransformCinematicFov` | current cinematic native value `C`, reference and target baselines | effective cinematic aspect | unified transfer then HorPlus | per cinematic sample/call | shared production helper |
| `src/plugin/runtime.cpp::ReplayManualTransitionOriginal` | native writer value | writer/native aspect path | native two-pass AspectRecalculation output | per callback/transition | intentional no-HorPlus path |
| `src/plugin/runtime.cpp::ApplyHorPlusGameplayDiagnostic` | observed native sample | diagnostic aspect | log/prediction only | per diagnostic sample | diagnostic; no production authority |
| `src/plugin/runtime.cpp` cinematic writer diagnostic path | observed writer input | diagnostic/effective aspect | trace only | per sample | diagnostic; no production authority |
| `src/diagnostics/matchgameplay_prediction.hpp` | diagnostic `C`, `E`, `G` | supplied cinematic aspect | predicted native/ HorPlus pair | per diagnostic sample | duplicate diagnostic math; not production |
| `src/dialogue/dialogue_fov.cpp::TransformProjectionSample` | Dialogue lifecycle sample | none | Dialogue transform | lifecycle samples | intentional separate policy |
| `src/dialogue/dialogue_fov.cpp::TransformExitSample` | Dialogue exit sample | none | Dialogue recovery output | recovery samples | intentional separate policy |
| `src/dialogue/dialogue_fov.cpp::AdaptiveTarget` / `ReducedTarget` | Dialogue baseline/target | none | policy-specific target math | lifecycle | intentional Dialogue domain, not global HorPlus |

The Dialogue `tan/atan` operations are projection interpolation around native Dialogue target `70` and reference gameplay `90`; they are not aspect-aware HorPlus and must not be merged into this migration without a separate contract.

No other production aspect-aware `tan/atan` transform or inverse transformed-to-native reconstruction was found in the source search. The diagnostic MatchGameplay prediction duplicates the transfer formula by design, but has no production output authority.

## Current transform contract

The intended invariant is:

```text
native FOV evidence
    -> policy-selected aspect
    -> exactly one HorPlus projection
    -> downstream writer/output
```

Gameplay HorPlus follows this through `EvaluateHorPlus` and the common `HorPlus` primitive. Cinematic ENTER performs one projection-space baseline transfer and one HorPlus projection. AspectRecalculation deliberately performs its native two-pass correction without HorPlus. Dialogue has a separate FOV policy and lifecycle.

The current code has a numeric cached-ENTER bypass in the Gameplay writer. It prevents the known cached transformed ENTER value from being transformed again when it numerically matches `g_cinematicTransformedFov`. The observation layer labels cached transformed input explicitly, but the production bypass is still numeric rather than a full provenance/generation token.

## Legacy and transitional state

| State | Current responsibility | Classification |
|---|---|---|
| `g_lastGameplayCameraSource`, `g_lastGameplayCameraFov` | Dialogue invalidation and legacy callback context; also still written by Gameplay HorPlus | transitional, not safe to delete |
| `g_lastObservedAspect`, `g_lastObservedAspectValid` | AspectRecalculation restoration / pending mode transition | transitional but active |
| `g_cinematicTransformedFov` | cached ENTER transformed value and numeric writer guard | transitional active safety guard |
| `g_cinematicEnterAspect` | diagnostic writer trace context | diagnostic/transitional |
| `g_matchGameplay*` atomics | diagnostic prediction context | diagnostic only |
| `runtime.horPlusGameplayCacheValid` and telemetry atomics | old HorPlus diagnostic state | diagnostic only; not baseline authority |
| `CameraFovObservationStore::ReadLatest` | factual store read API | transitional; no production consumer yet |
| `GameplayBaselineStore` | coherent retained Gameplay baseline | active production consumer at ENTER |
| `CameraStateSnapshot` | composed diagnostic state | observational only |

No deletion is recommended in this audit. In particular, the legacy globals must remain until each independent reader and invalidation responsibility has been migrated and runtime-validated.

## Dependency and lifecycle map

- **Dialogue:** independent native target/policy/lifecycle subsystem. It may observe the same camera callbacks but does not own Gameplay HorPlus. Candidate, Active, Exiting and recovery semantics remain separate.
- **ZOOM / ADS / binocular:** `ZOOM_IN/ZOOM_OUT` are neutral transition observations without FOV input or owner semantics. ADS and binocular native trajectories enter through the Gameplay writer and are transformed there when Gameplay owns the callback.
- **Cinematic ENTER:** the second migrated HorPlus seam. It selects either native/authored or retained Gameplay baseline and uses cinematic aspect policy.
- **Cinematic EXIT:** records the native recovery target and resets the transformed ENTER cache. It is not itself a HorPlus transform.
- **Recovery:** native recovery samples remain outside Gameplay baseline ownership until coordinator eligibility returns to Gameplay.
- **AspectRecalculation:** native two-pass writer behavior is intentionally preserved. It publishes native factual evidence but does not perform HorPlus.
- **Gameplay HorPlus:** per-sample native writer transform and baseline projection.

## Risks and unresolved gaps

1. A dynamic already-transformed cinematic writer input could evade numeric equality with the cached ENTER value and be transformed twice. This is a concrete static risk, not a confirmed runtime defect.
2. The old ENTER reconstruction from separate legacy FOV/aspect state has been replaced by the coherent baseline read, but those legacy fields remain used elsewhere.
3. Cinematic policy aspect is correctly resolved by `ResolveCinematicAspect`; future consumers must not substitute `GameplayBaseline.aspect` for it.
4. Cached transformed provenance is explicit in the observation store, while the production guard still relies on numeric equality and tolerance.
5. The semantic meaning of the cinematic reference argument is narrower than a universal authored reference contract; do not generalize it without evidence.

## Migration classification

### Migrated

- shared HorPlus primitive used by Gameplay and Cinematic code;
- Gameplay writer production transform and factual CameraWriter publication;
- GameplayBaseline projection for transformed, pass-through and AspectRecalculation coverage;
- Cinematic ENTER selection from the coherent retained baseline and CinematicEnter publication.

### Transitional

- legacy Gameplay source/FOV and aspect atomics;
- numeric cached ENTER guard;
- factual observation store before broader production consumers;
- diagnostic CameraState and MatchGameplay prediction state;
- compatibility/test helpers `TryTransformMatchGameplay` and `TryTransformEnterFov`;
- common primitive still housed in the Cinematic module rather than a neutral FOV module.

### Untouched or intentional

- Dialogue math and lifecycle;
- ZOOM/ADS ownership semantics;
- Cinematic EXIT and recovery ownership;
- native AspectRecalculation two-pass behavior;
- resolver/hook architecture and viewmodel FOV research.

## Plans and history

No single authoritative global migration master plan was found. The closest active design record is `research/active/HORPLUS_GAMEPLAY_FOV_STATE_DESIGN_TASK_PLAN.md`; it is now partially superseded by the completed Batch 1, 2A, 2A.1 and 2B plans/reports. Relevant completed records include:

- `research/completed/CAMERA_FOV_OBSERVATION_BATCH1_TASK_PLAN.md` and its implementation report;
- `research/completed/GAMEPLAY_BASELINE_BATCH2A_TASK_PLAN.md` and report;
- `research/completed/GAMEPLAY_BASELINE_BATCH2A1_NATIVE_COVERAGE.md` / related plan;
- `research/completed/GAMEPLAY_BASELINE_BATCH2B_TASK_PLAN.md` and report;
- `research/reports/GAMEPLAY_BASELINE_RETAINED_STATE_AUDIT.md`.

The current source is authoritative where an older plan describes a pre-migration state. No older batch number is invented here.

## Recommended next production batch

The minimum next cutover is **Gameplay Aspect Restoration Consumer Cutover**. This is a production consumer migration, not cleanup:

1. Define a coherent eligible Gameplay aspect snapshot for `ApplyPendingGameplayModeTransition`.
2. Switch only that reader from the legacy observed-aspect pair to the new coherent projection, preserving the existing two-pass write, flags, deferral and fail-closed behavior.
3. Keep `g_lastObservedAspect` as a shadow/reference until static equivalence and runtime validation pass.
4. Do not delete legacy atomics, helpers or caches in this batch.

The exact source of the replacement aspect must be confirmed from the existing contract; do not blindly use a baseline aspect if the restoration path requires a different policy-specific aspect. After runtime validation, a separate batch may migrate Dialogue source/FOV invalidation, then a separate audit may replace the numeric cached-ENTER guard. Dead-state deletion is explicitly deferred until those owners have been validated.

## Runtime validation matrix for the next cutover

- Gameplay HorPlus at 21:9 and 32:9; establish a coherent baseline.
- F11 AspectRecalculation -> HorPlus when the current writer has an ultrawide aspect and when it has only native aspect.
- HorPlus -> AspectRecalculation -> HorPlus, including invalid/unavailable aspect and deferred transition cases.
- Compare legacy shadow aspect and new selected aspect, source, coordinator, mode and phase.
- ADS and binocular ZOOM transitions must not contaminate baseline ownership or Dialogue state.
- Dialogue active/recovery during mode transition must retain existing lifecycle behavior.
- Cinematic ENTER/EXIT around mode transitions must use cinematic aspect policy, not Gameplay baseline aspect, and must not double-transform cached values.
- Direct load, save/load, death/respawn and camera recreation must fail closed when no coherent Gameplay baseline is available.

## Explicit non-goals of this audit

- no source cleanup or optimization;
- no deletion of legacy state;
- no change to Dialogue, ZOOM, ADS, Cinematic EXIT/recovery or AspectRecalculation behavior;
- no performance P1/P2 work;
- no runtime launch, build or claim of new executable-version support.

## Final state

The global migration is partially complete: the two production decision seams are integrated, factual observations and retained Gameplay baseline are established, and the remaining work is consumer-by-consumer cutover with runtime gates. The next implementation should be the bounded restoration consumer migration above; legacy deletion must wait for evidence that the new owner/path fully replaces each old responsibility.
