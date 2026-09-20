# Camera/FOV Architecture Independent Review — Steam 2.0.5

Date: 2026-09-18  
Scope: current source tree plus the supplied Steam 2.0.5 runtime log  
Review mode: read-only; no build, harness, or game execution was performed

## Evidence vocabulary

- **CONFIRMED** — a concrete contradiction or defect is present in the current source/log.
- **STATICALLY ESTABLISHED** — the current source is sufficient to establish the claim.
- **RUNTIME CONFIRMED** — the supplied 2.0.5 log directly establishes the claim for that run.
- **PLAUSIBLE** — the path exists, but occurrence or impact needs targeted evidence.
- **NOT ESTABLISHED** — neither the current source nor the supplied run proves the claim.

These labels describe evidence strength. They are not release or blocker decisions.

## 1. Executive summary

The production camera path has a sound central boundary: the gameplay hook is installed on a uniquely resolved and decoded `MOVSS [RBX+0x30], XMM0`, and HorPlus is applied to the value about to be written. The immediately preceding matched instructions load `XMM0` from `[RSI+0x230]`. Arbitrary ultrawide ratios use the same mathematical path; 16:9 and invalid inputs bypass it. **STATICALLY ESTABLISHED.**

The source does not, however, prove the stronger global claims that every invocation reaches this boundary from only that fall-through predecessor, that every native transition sample reaches it exactly once, or that another game path can never feed the transformed output back into a future source field. The supplied runtime log is consistent with fresh native trajectories, but it is not an instruction-level cardinality or data-provenance trace. Those stronger properties remain **NOT ESTABLISHED**, not contradicted.

`ZOOM_IN` and `ZOOM_OUT` are neutral transition-signal names at the signature level and, in the supplied diagnostic, are read-only observations of transition weights. They do not call `TryTransformHorPlus()`. The observed `XMM0` values are weights, not camera FOV. The pair is not ADS-specific. **STATICALLY ESTABLISHED / RUNTIME CONFIRMED.** Old `WideboyAds`/`AdsLifecycle` naming and an optional behavior-changing owner integration remain in the tree and no longer match the established signal semantics. **CONFIRMED.**

The largest current correctness problem is Dialogue classification. It has no positive game-owned dialogue discriminator. Any eligible descending FOV can move `Candidate -> Active`; `Candidate` has no timeout, stabilization cancellation, reversal cancellation, or source identity. In the supplied run, real-dialogue recovery is declared before the native return tail ends; the next tail sample creates a new Candidate, which remains armed and later classifies a generic zoom trajectory as Dialogue. **CONFIRMED / RUNTIME CONFIRMED.** The current hotkey also changes the global policy read by an already-active lifecycle, contradicting “next dialogue” semantics. **CONFIRMED.**

Cinematic ENTER/EXIT resolution for Steam 2.0.5 is structurally strong and the supplied run confirms the indexed EXIT fallback and shared call target. Gameplay and cinematic FOV domains are not used to manufacture one another. No cinematic lifecycle was exercised in the supplied log, so direct-load behavior, post-exit convergence, and the generic `CinematicActive` double-transform exclusion remain partly unproven.

AspectRecalculation remains a separate generalized fallback mechanism. Its current implementation is an atomic normalization/re-arm choreography, not the older two-pass choreography implied by stale names/comments. Runtime mode cycling does not perform state/source restoration or invalidation, so AspectRecalculation-to-HorPlus can inherit a source left at 16:9, and the reverse direction can inherit replay state. **CONFIRMED.**

Two instrumentation findings matter before using another run as evidence. First, `g_postExitTraceArmed` is armed on every cinematic exit and is never disarmed; after that edge, the production writer permanently performs seven guarded reads plus trace comparison/logging work. Second, merged HorPlus telemetry classifies Candidate as Dialogue and declines to compute HorPlus output for Dialogue/Cinematic/Recovery owners, while the subsequent production path can still transform the sample. Therefore `horPlusNew=nan` and owner labels in the supplied diagnostic do not necessarily describe the actual value written. **CONFIRMED.**

The next bounded work should repair evidence integrity and lifecycle boundaries before requesting another game run: remove/bound the production post-exit trace, make diagnostic ownership reflect actual transform gates, neutralize or isolate ADS-specific integration, make Dialogue Candidate/recovery bounded, latch active Dialogue policy, and define mode-switch cleanup. No request for a runtime test is made by this review.

## 2. Current architecture map

```text
game camera source
  [RSI+0x230] world/native FOV
  [RSI+0x254] runtime aspect
  [RSI+0x259] projection flags
        |
        v
validated gameplay writer boundary
  XMM0 -> MOVSS [RBX+0x30], XMM0
        |
        +-- ReplayManualTransition()
              |
              +-- post-exit observation / exclusion observation
              +-- optional merged diagnostic telemetry
              +-- GameplayMode::HorPlus
              |     `-- ApplyHorPlusGameplay()
              |            `-- TryTransformHorPlus()
              |
              `-- GameplayMode::AspectRecalculation
                    `-- ReplayManualTransitionOriginal()

ZOOM_IN / ZOOM_OUT
  transition weights only -> optional diagnostic observation
  no direct FOV transform in the supplied artifact

DialogueBoundary
  XMM6 native/generic FOV sample -> heuristic lifecycle -> optional XMM1 replacement

Cinematic ENTER
  native cinematic XMM0 -> cinematic HorPlus -> XMM0
  coordinator = CinematicActive

Cinematic EXIT
  native exit XMM0 retained as recovery target
  coordinator = Gameplay (HorPlus) or CinematicExiting (AspectRecalculation)
```

`RuntimeState` in `src/plugin/runtime.cpp:97-199` is the translation-unit-private runtime owner. The numerous `g_*` references are aliases to its fields. Production coordination is the three-state `gameplay::CoordinatorState`; diagnostic `HorPlusTelemetryOwner` is a derived label, not an ownership authority.

### FOV-state inventory

| Concept | Current production representation | Diagnostic representation | Result |
| --- | --- | --- | --- |
| `ConfiguredGameplayFov` | None; no authoritative source is read | Printed as `configured=UNKNOWN` | **STATICALLY ESTABLISHED:** correctly not guessed |
| `NativeGameplayCameraFov` | Current writer `XMM0`; no named durable production cache in HorPlus mode | `g_lastHorPlusNativeFov` and change telemetry | **STATICALLY ESTABLISHED** |
| `HorPlusGameplayCameraFov` | Local transformed value written back to `XMM0`; no production cache | `g_lastHorPlusTransformedFov` and validity metadata | **STATICALLY ESTABLISHED** |
| `NativeCinematicFov` | ENTER/EXIT callback `XMM0` | Cinematic trace snapshots | **STATICALLY ESTABLISHED** |
| `CinematicHorPlusFov` | ENTER result and `g_cinematicTransformedFov` guard value | Cinematic trace output | **STATICALLY ESTABLISHED** |

`research/reports/HORPLUS_GAMEPLAY_FOV_STATE_DESIGN.md` describes a proposed state model; it is not the current production implementation.

## 3. Gameplay HorPlus review

### Input and hook boundary

`src/hooks/signatures/signature_definitions.hpp:10-26` matches the sequence containing:

1. selector test at `[RSI+0x262]`;
2. `MOVSS XMM0,[RSI+0x230]`;
3. a conditional branch;
4. output address preparation from `RBX+0x30`;
5. `MOVSS [RBX+0x30],XMM0`.

`gameplay::ResolveCameraWriter()` (`src/gameplay/gameplay_camera.cpp:27-76`) searches `.text`, requires exactly one full signature match, and decodes the hook instruction as `MOVSS [RBX+0x30], XMM0`. The hook is therefore at the actual output store. **STATICALLY ESTABLISHED.**

For the matched fall-through sequence, `XMM0` comes from `[RSI+0x230]`, not from `[RBX+0x30]`. **STATICALLY ESTABLISHED.** A full control-flow proof for every possible predecessor and a whole-engine proof that output cannot later update the source are absent. Feedback transformed output -> future native input is **NOT ESTABLISHED**, but neither is its impossibility.

### Transform behavior

`ReplayManualTransition()` (`src/plugin/runtime.cpp:2215-2306`) dispatches HorPlus before the AspectRecalculation path. `ApplyHorPlusGameplay()` (`src/plugin/runtime.cpp:2467-2491`) accepts only `Gameplay` and `CinematicActive`, reads current aspect/flags from `[RSI+0x254]` and `[RSI+0x259]`, and calls `TryTransformHorPlus()`.

`TryTransformHorPlus()` (`src/gameplay/horplus_gameplay.cpp:10-20`) requires:

- finite input FOV strictly between 1 and 179;
- `aspect > 16:9 + 0.001`;
- flags `0x4` or `0x5`;
- a finite result strictly between 1 and 179.

It uses the same HorPlus formula for arbitrary eligible aspect ratios. Exact 21:9 or 32:9 values are not production detection gates. At 16:9, narrow, invalid aspect, invalid flags, or invalid FOV, it returns without changing `XMM0`. **STATICALLY ESTABLISHED.**

In `Gameplay`, every eligible writer callback recomputes HorPlus. There is no result cache. Repeated identical native values are repeatedly transformed once per callback invocation. **STATICALLY ESTABLISHED.** “Once per callback” is established; “exactly once per logical game sample” is **NOT ESTABLISHED** because callback/sample cardinality is unknown.

In `CinematicActive`, only a sample approximately equal to the single cached ENTER-transformed value is excluded (`src/plugin/runtime.cpp:2480-2485`). Dynamic authored cinematic values, or already-transformed values different from that cache, could pass into gameplay HorPlus. This is a **PLAUSIBLE** double-transform path, not demonstrated by the supplied run.

In normal HorPlus EXIT, `ResolveCinematicExitTransition()` returns directly to `Gameplay` (`src/gameplay/gameplay_state.cpp:32-37`), so HorPlus does not normally process under `CinematicExiting`. The recovery/exclusion observer still runs before mode dispatch.

The supplied log establishes `nativeGameplay=90`, `aspect=3.55556`, and diagnostic expected HorPlus `126.87`. It does not log the post-callback `XMM0` actually stored. The calculation is **RUNTIME CONFIRMED**; the final store value in that run is **NOT ESTABLISHED** by this telemetry alone.

## 4. ZOOM_IN / ZOOM_OUT review

The signatures at `src/hooks/signatures/signature_definitions.hpp:32-35` encode:

- ZOOM_IN: load `XMM0 <- [RAX+0x4C]`, load `XMM1 <- [RSI+0x138]`, then `UCOMISS XMM1,XMM0`;
- ZOOM_OUT: load `XMM0 <- [RAX+0x50]`, load `XMM1 <- [RSI+0x13C]`, then the same compare.

The optional hooks are placed at signature match +13, the compare instruction (`src/plugin/runtime.cpp:2308-2448`). Installation requires the exact Steam 2.0.5 executable SHA-256, exactly one match for each signature, and `0F 2E C8` at the hook point. On partial setup failure, the first hook is rolled back. **STATICALLY ESTABLISHED.**

The callback reads `RAX+0x4C`, `RAX+0x50`, `RSI+0x138`, and `RSI+0x13C`, captures XMM0/XMM1/XMM6, and—unless the optional owner-integration macro is enabled—does not modify registers or production coordinator state. In the supplied run, `RAX+0x4C` and `RAX+0x50` form complementary transition weights, XMM0 follows the selected weight, and XMM6 is a small unrelated value rather than camera FOV. **RUNTIME CONFIRMED.**

`TraceZoomTransition()` never calls `TryTransformHorPlus()`. The hooks are not installed in the default production artifact; they are conditional on `WIDEBOY_ADS_DIAGNOSTIC` or `WIDEBOY_ADS_OWNER_INTEGRATION`. There is no normal-production coupling to HorPlus, Dialogue, or Cinematics. **STATICALLY ESTABLISHED.**

The reviewed runtime artifact is `STALKER2CameraTweaks_ZoomHorPlusDiagnostic.asi`: its hash matches the supplied `modSha256`. Its build enables read-only zoom and HorPlus-state telemetry, not `WIDEBOY_ADS_OWNER_INTEGRATION`. Therefore the supplied zoom callbacks are observational. **RUNTIME CONFIRMED.**

The tree still contains `WideboyAds*` identifiers, `AdsLifecycle`, `AdsDirection`, ADS owner labels, and diagnostic build variants that enable `WIDEBOY_ADS_OWNER_INTEGRATION`. Under that macro, the generic signal mutates an ADS lifecycle which can suppress Dialogue and change diagnostic ownership (`src/plugin/runtime.cpp:2366-2371`). Because the signal is also controller-camera pull, that semantic coupling is invalid. **CONFIRMED.**

## 5. Zoom-to-writer data-flow review

The observed relationship is:

```text
ZOOM transition weight changes
    -> game-owned camera interpolation
    -> [RSI+0x230] native camera FOV trajectory
    -> validated gameplay writer
    -> HorPlus at final store
```

The log shows close temporal correlation between ZOOM weights and the native FOV trajectory at the writer. It also shows that the values have different meanings and scales. **RUNTIME CONFIRMED.**

There is no direct source-level call or shared state that maps each ZOOM callback to exactly one writer callback. Different telemetry thresholds and potential engine paths prevent a one-to-one inference. Therefore:

- every actual native trajectory sample reaches the writer: **NOT ESTABLISHED**;
- ZOOM callback -> writer callback ordering/cardinality is one-to-one: **NOT ESTABLISHED**;
- each writer invocation applies HorPlus at most once in this mod: **STATICALLY ESTABLISHED**;
- each logical native sample is transformed exactly once globally: **NOT ESTABLISHED**;
- transformed writer output cannot influence a later native source: **NOT ESTABLISHED**.

`tests/gameplay/zoom_transition_harness.cpp` intentionally supplies synthetic native samples and does not feed prior transformed output back as input. It tests the math contract. It cannot prove engine callback ordering or absence of feedback. Claims to that effect in `research/reports/HORPLUS_ZOOM_TRANSITION_STATIC_INTEGRATION_DESIGN.md` exceed the harness evidence.

## 6. Dialogue review

### Current state machine

`TraceDialogueBoundary()` is at `src/plugin/runtime.cpp:1799-1909`. The hook consumes the boundary's incoming native/generic FOV from XMM6 and, when transforming, writes XMM1.

| Phase | Entry/transition predicate | Current reset/cancel behavior |
| --- | --- | --- |
| `Inactive -> Candidate` | First valid sample after negative gates; baseline = incoming sample | Unconditional on positive dialogue identity |
| `Candidate -> Active` | Next qualifying descent greater than `0.01` | No timeout, stabilization cancel, upward-reversal cancel, or source identity |
| `Active -> Exiting` | Upward movement greater than `0.01`; recovery anchor captured | A later descent does not explicitly return to Active |
| `Exiting -> Inactive` | Incoming sample returns within `1.0` of baseline | Resets lifecycle and previous sample |

Negative gates include post-cinematic exclusion, optional ADS-owner exclusion, coordinator must be `Gameplay`, and policy must not be Native. These gates reduce scope but do not positively identify Dialogue. `[RSI+0x2C]` is a native blend target/end-FOV field; a target near 70 is not a dialogue flag and is not used here as a hardcoded detector.

### Confirmed re-arm sequence

The supplied run shows:

1. real Dialogue baseline around `89.9926`;
2. lifecycle recovery declared while the native return trajectory is still below baseline;
3. owner changes to Gameplay at `89.0913`;
4. approximately 1 ms later a new Candidate is captured at `89.2106`;
5. approximately 13 ms later telemetry labels the owner Dialogue;
6. the same ascending tail continues to approximately `89.9934` without cancelling Candidate;
7. a later generic zoom descent is then handled under the stale Dialogue classification.

This is **RUNTIME CONFIRMED**. The source-level cause is the combination of a wide recovery tolerance, immediate reset to Inactive, unconditional first-sample Candidate creation, and no Candidate cancellation/lifetime. Resetting the previous sample does not itself synthesize a descending sample; it permits immediate re-arming on the still-active transition tail. **CONFIRMED.**

The source/material-discontinuity reset (`sourceChanged` or FOV jump greater than 5) exists in `ReplayManualTransitionOriginal()` (`src/plugin/runtime.cpp:1282-1295`) and is therefore bypassed by the early HorPlus return. Dialogue does not receive equivalent source identity at its own hook. **CONFIRMED coverage gap.**

### Policy semantics

The callback reloads `g_runtimeDialoguePolicy` on every invocation. The F10 hotkey stores a new global policy; no active-policy snapshot is latched at Candidate/Active entry. An active Dialogue can therefore change transform policy in mid-lifecycle, despite the user-facing “for next dialogue” wording. **CONFIRMED.** The minimal contract is separate selected policy and lifecycle-latched active policy.

### Positive discriminator status

No game-owned positive Dialogue discriminator is present in the standalone ASI path. `APC::IsInStaticDialog()` is known from UE4SS runtime work, but access to it from this standalone ASI is **NOT ESTABLISHED**. It is a future evidence seam, not a current dependency and not grounds to hardcode target FOV values.

## 7. Cinematics review

`InstallCinematicFovHooks()` resolves a unique ENTER signature and first tries the legacy EXIT signature; the indexed EXIT signature is used only when legacy has zero matches (`src/plugin/runtime.cpp:1507-1634`). It decodes ENTER's RIP-relative FOV load, both relative calls, EXIT's legacy or `[RBX+RAX*4+0x38]` operand, executable call targets, and equality of the ENTER/EXIT target. The supplied Steam 2.0.5 run reports ENTER=1, legacy EXIT=0, indexed EXIT=1, both structural/decode validations PASS, and the same call target. **RUNTIME CONFIRMED.**

If legacy EXIT produces multiple matches, the resolver fails closed rather than trying indexed fallback. That is conservative and consistent with “fallback only when legacy is absent.”

Initialization is transactional at the feature level in `src/cinematics/cinematic_initialization.cpp`: aspect is installed first, then FOV; failure rolls back installed FOV state and aspect state. **STATICALLY ESTABLISHED.**

On ENTER (`src/plugin/runtime.cpp:1658-1687`), the runtime resets recovery/exclusion state, resolves the current cinematic aspect policy, transforms the callback's actual XMM0, caches only the resulting cinematic FOV for duplicate exclusion, and sets `CinematicActive`. There is no invariant that the native value equals 90. **STATICALLY ESTABLISHED.**

On EXIT (`src/plugin/runtime.cpp:1688-1737`), the runtime retains the actual exit XMM0 as the recovery/exclusion target, clears the cinematic transformed cache, and selects a mode-aware transition: HorPlus returns to Gameplay; AspectRecalculation may enter `CinematicExiting` and arm the handoff. Cinematic FOV is not copied into a gameplay-native or gameplay-HorPlus cache. **STATICALLY ESTABLISHED.**

The supplied log contains resolver/installation evidence but no cinematic ENTER/EXIT callback. Actual ENTER transform, EXIT recovery, post-cinematic exclusion, direct-load-into-cinematic behavior, and aspect-store callback behavior are **NOT ESTABLISHED** for this run. Static code requires the ENTER boundary to execute; a load resumed inside a cinematic without that boundary is therefore **NOT ESTABLISHED**.

`ResolveCinematicAspect()` Auto mode uses the Win32 client/display viewport (`src/plugin/runtime.cpp:988-1003`). The comment at `src/plugin/runtime.cpp:1034-1036` says it uses cached runtime camera aspect, and the log source label can say `runtime-camera` based on an observed object field even though that value does not choose the resolved aspect. This provenance description is **CONFIRMED** stale/misleading; the transform itself follows viewport Auto policy.

## 8. AspectRecalculation review

`ReplayManualTransitionOriginal()` (`src/plugin/runtime.cpp:1275-1402`) uses the generalized `IsUltrawideAspect()` predicate: finite and strictly greater than native 16:9 by `0.001`. No exact 2.38889 or 3.55556 production detection gate remains. **STATICALLY ESTABLISHED.**

Current behavior is:

- constrained ultrawide (`flags == 0x5`) writes native aspect with `0x5` and returns;
- a wide `0x4` sample after completion resets Dialogue state and moves replay state to Waiting;
- Waiting plus wide `0x4` atomically writes native aspect and `0x4`, then marks Complete;
- 16:9, narrow, nonfinite, unreadable, or incompatible flags bypass/refuse safely;
- cinematic EXIT can use the same atomic handoff in AspectRecalculation mode.

This is a validated independent fallback mechanism and should not be rewritten merely to resemble HorPlus. `ReplayState::AppliedConstrainPass` and comments describing an older pending/two-pass choreography are stale relative to the current atomic production path. **STATICALLY ESTABLISHED.**

### Cross-mode state

The gameplay hotkey changes only the atomic mode value. It does not restore source aspect/flags, reset `ReplayState`, clear last-source/last-aspect state, or define an explicit transition transaction.

- AspectRecalculation -> HorPlus can leave the current source at 16:9. HorPlus then sees a non-ultrawide aspect and bypasses until the game independently restores it.
- HorPlus -> AspectRecalculation can resume with old Waiting/Complete/source observations and apply fallback logic against stale lifecycle state.

These paths are present in current source and contradict an unconditional “immediate mode switch” expectation. **CONFIRMED.** Whether the engine always repairs the first case on the next frame is **NOT ESTABLISHED**.

## 9. Coordinator/ownership state table

| State/label | Established by | Cleared/replaced by | Readers and behavior | Stale/conflict assessment |
| --- | --- | --- | --- | --- |
| `CoordinatorState::Gameplay` | Initialization; HorPlus EXIT; Aspect recovery completion | Cinematic ENTER | HorPlus transform allowed; Dialogue eligible; Aspect fallback active | Stable default |
| `CoordinatorState::CinematicActive` | Cinematic ENTER | Cinematic EXIT | HorPlus allowed with one-value exclusion; Dialogue rejected | **PLAUSIBLE:** missed EXIT leaves cinematic ownership stale |
| `CoordinatorState::CinematicExiting` | AspectRecalculation EXIT when gameplay is available | Recovery target convergence | HorPlus bypass; Dialogue rejected; recovery observes writer | **PLAUSIBLE:** target may never reconverge |
| `DialoguePhase::Candidate` | First valid generic boundary sample | Confirmed descent or explicit reset gates | Telemetry already labels Dialogue; no transform on capture | **CONFIRMED:** unbounded stale Candidate |
| `DialoguePhase::Active` | Candidate + descent | Upward direction -> Exiting or reset gate | Dialogue policy transform writes XMM1 | **CONFIRMED:** generic descent can activate |
| `DialoguePhase::Exiting` | Active + ascent | Return within baseline tolerance or reset gate | Recovery transform behavior | **PLAUSIBLE:** reversal can retain stale phase |
| post-cinematic exclusion | Cinematic EXIT target | return to target, source change, or invalid reset | Blocks Dialogue classification only | **PLAUSIBLE:** no timeout; may suppress Dialogue indefinitely |
| `ReplayState::Waiting/Complete` | Aspect fallback transitions and initialization | Aspect path/handoff | Only AspectRecalculation logic should own it | **CONFIRMED:** not reset on mode switch |
| `AdsLifecycle` (optional) | Generic ZOOM observations under integration macro | Opposite/completion observations | Can block Dialogue; affects diagnostic owner | **CONFIRMED:** semantic owner is invalid for generic signal |
| `HorPlusTelemetryOwner` | Derived every diagnostic callback | Re-derived | Logging only; Candidate counts as Dialogue | **CONFIRMED:** can contradict actual transform gate |

The production coordinator is not a comprehensive camera-owner classifier; it is a coarse cinematic/gameplay gate. Dialogue phase and recovery exclusion are separate state machines. Treating the diagnostic owner label as authoritative ownership conflates those layers.

## 10. Cross-subsystem transition matrix

| Transition | Coordinator/native source | Transform expectation | Invalidation/stale-state result |
| --- | --- | --- | --- |
| Gameplay -> ZOOM_IN -> ZOOM_OUT -> Gameplay | Coordinator remains Gameplay; writer XMM0 is native camera trajectory | ZOOM does none; writer HorPlus once per callback | One-to-one ordering **NOT ESTABLISHED**; stale Dialogue Candidate can misclassify it |
| Gameplay -> Dialogue -> Gameplay | Coordinator remains Gameplay; Dialogue uses generic boundary XMM6/XMM1 contract | Dialogue policy at boundary; gameplay writer HorPlus independently executes | **CONFIRMED:** early recovery can re-arm Candidate on same tail |
| Gameplay -> Cinematic -> Gameplay | ENTER sets CinematicActive; EXIT is mode-aware | Cinematic ENTER HorPlus; writer guard attempts to prevent repeat | Domains separate; generic dynamic double-transform exclusion **PLAUSIBLE** |
| Gameplay zoom -> Cinematic | Cinematic ENTER should replace coordinator; ZOOM is only a signal | No ZOOM transform; cinematic path owns ENTER | Actual interleaving **NOT ESTABLISHED** |
| Dialogue -> gameplay zoom | Coordinator still Gameplay | Dialogue classifier may remain active and transform generic descent | False classification **RUNTIME CONFIRMED** in supplied sequence |
| Cinematic -> recovery -> gameplay zoom | HorPlus returns directly to Gameplay plus exclusion; Aspect mode may use CinematicExiting | Dialogue excluded during target recovery | Completion if target never returns **PLAUSIBLE**; no runtime sample supplied |
| HorPlus -> AspectRecalculation | Mode atomic changes; old replay/source state retained | Aspect fallback begins on next writer | **CONFIRMED** missing transition invalidation |
| AspectRecalculation -> HorPlus | Mode atomic changes; source may remain normalized to 16:9 | HorPlus may bypass | **CONFIRMED** missing restoration/transition contract |
| Runtime aspect change during Gameplay | Current source aspect read on every HorPlus callback | New ratio used immediately if valid | Diagnostic cache invalidates; production has no cache |
| Runtime aspect change during ZOOM | ZOOM callback ignores aspect; writer rereads it | Writer uses then-current source | Relative ordering **NOT ESTABLISHED** |
| Runtime aspect change during Dialogue | Dialogue policy path does not provide an authoritative separate owner; gameplay writer still reads current aspect | Actual writer can HorPlus despite telemetry Dialogue label | **CONFIRMED telemetry ambiguity** |
| Runtime aspect change during Cinematic | ENTER resolves configured/viewport policy; gameplay writer reads camera source | One cached cinematic FOV exclusion only | Dynamic behavior **NOT ESTABLISHED** |

## 11. Telemetry review

The supplied artifact adds `GAMEPLAY_FOV_CHANGE`, `CINEMATIC_FOV_CHANGE`, `DIALOGUE_FOV_CHANGE`, `UNKNOWN_FOV_CHANGE`, `CAMERA_OWNER`, and `HORPLUS_STATE`. It also adds ZOOM snapshot logging. `configured=UNKNOWN` is correct because no authoritative configured-gameplay-FOV source exists. **STATICALLY ESTABLISHED.**

ZOOM telemetry is change-filtered per thread/signal with a `0.01` float threshold, but intentionally produces trajectory samples rather than only lifecycle summaries. FOV telemetry is emitted when its diagnostic snapshot changes. Missing `DIALOGUE_CALLBACK_SUMMARY` is not a defect when process-resident ASI cleanup did not run.

`ResolveHorPlusTelemetryOwner()` (`src/plugin/runtime.cpp:2118-2137`) labels any non-Inactive Dialogue phase—including Candidate—as Dialogue. `TraceHorPlusFovState()` (`src/plugin/runtime.cpp:2139-2209`) computes a transformed result only for diagnostic Gameplay/Ads owners. Immediately afterward, production `ApplyHorPlusGameplay()` ignores this diagnostic owner and transforms under coordinator Gameplay or CinematicActive. Consequently:

- `owner=Dialogue/Cinematic/Recovery` can coexist with an actual gameplay HorPlus transform;
- `horPlusNew=nan handled=false` does not prove that the writer passed native FOV through;
- `cacheValid=true` is diagnostic cache validity, not a production transform cache;
- owner counts cannot be used as authoritative transform counts.

This is a **CONFIRMED instrumentation defect**. The attached log's `DIALOGUE_FOV_CHANGE` rows are evidence of the classifier/telemetry state, not direct evidence of the final writer value.

The attached Zoom/HorPlus diagnostic does not enable ADS owner integration and does not intentionally alter FOV registers. Other build scripts do enable `WIDEBOY_ADS_OWNER_INTEGRATION`; those artifacts are behavior-changing and must not be described as observation-only. **CONFIRMED build-contract distinction.**

The historical audit identified unconditional `EARLY_CB ASPECT ENTER/RETURN` in
`ApplyCinematicAspectStore()`. The V1 cleanup removed those duplicate lines;
the meaningful aspect policy/source/write result remains logged.

## 12. Performance/hot-path risks

No causal claim about historical stutter is supported without measurement. The following are static risks only.

1. **CONFIRMED production instrumentation leak:** `ObservePostExitRawWriterEntry()` (`src/plugin/runtime.cpp:716-756`) is called unconditionally from `ReplayManualTransition()` (`src/plugin/runtime.cpp:2215-2224`). A cinematic EXIT sets `g_postExitTraceArmed=true`, but no current path sets it false. Every later writer callback then performs seven `SafeRead` operations (each backed by memory-region validation), takes the dialogue-state observation path, compares a broad snapshot, and may synchronously log. Trigger: first cinematic EXIT. Observable consequence: permanent post-exit writer overhead and potentially ongoing trace log traffic. Subsystems: all gameplay camera modes after cinematics. Status: existing current-tree defect; historical regression origin **NOT ESTABLISHED**. Minimal repair: compile-gate/remove raw trace from production or add a bounded disarm independent of the required post-cinematic Dialogue exclusion.

2. **STATICALLY ESTABLISHED HorPlus cost:** every eligible writer callback performs two guarded source reads and a `tan/atan` HorPlus calculation; identical values are not cached. This may be acceptable, but frequency and cost impact are **NOT ESTABLISHED** without profiling.

3. **STATICALLY ESTABLISHED AspectRecalculation diagnostic cost:** its writer path performs source/output snapshot reads and `LogCameraModeChange()` work in addition to core state logic. This is production instrumentation on a high-frequency boundary. Impact is **NOT ESTABLISHED**.

4. **RUNTIME CONFIRMED diagnostic amplification:** the supplied artifact adds telemetry reads, mutex acquisition/state classification, floating-point comparisons, string formatting, and many ZOOM/FOV log records. Logging uses the shared logger with info-level flushing. Stutter observed with this artifact cannot be attributed to normal production HorPlus without a clean measurement.

5. **PLAUSIBLE low-frequency noise:** cinematic aspect `EARLY_CB` and per-store logs synchronously flush, but expected frequency is much lower than the gameplay writer.

## 13. Resolver/safety findings

| Hook | Current validation | Finding |
| --- | --- | --- |
| Gameplay writer | `.text` scan, exactly one complete signature, decoded `MOVSS [RBX+0x30],XMM0` | **STATICALLY ESTABLISHED** strong fail-closed boundary |
| Cinematic ENTER | unique pattern, decoded RIP-relative load and call, structural bytes/vcall evidence, executable target | **STATICALLY ESTABLISHED / RUNTIME CONFIRMED on 2.0.5** |
| Cinematic EXIT | legacy first; indexed fallback only if zero legacy; unique chosen pattern; decoded operands/call; shared target | **STATICALLY ESTABLISHED / RUNTIME CONFIRMED on 2.0.5** |
| Cinematic aspect | unique signature plus instruction validation before hook | **STATICALLY ESTABLISHED** |
| Dialogue boundary | unique signature and exact call-site byte relationship; hook at fixed offset | **STATICALLY ESTABLISHED** fail-closed uniqueness; no independent Zydis operand contract is claimed |
| ZOOM_IN/OUT | exact 2.0.5 SHA gate, unique full signatures, compare bytes at +13, rollback on partial install | **STATICALLY ESTABLISHED** for optional diagnostic; exact load operands are encoded by the signatures rather than separately decoded |

The scanner operates on the loaded executable image and the production feature installers refuse ambiguous/missing matches. Steam 2.0.5 is the supplied runtime-validated target. Older 2.0.2–2.0.4 remain static resolver evidence only.

One generic robustness concern is that nearby-byte helpers such as a fixed 96-byte structural window do not visibly clamp every read to the containing executable section. A candidate close to a section boundary could make that validation read outside the intended section. This is **PLAUSIBLE** for unsupported images; the supplied 2.0.5 matches are valid and do not demonstrate a fault.

## 14. Confirmed defects

### D1 — Dialogue recovery re-arms Candidate on the same transition tail

- Evidence: **CONFIRMED / RUNTIME CONFIRMED**.
- Location: `src/plugin/runtime.cpp:1799-1909`, especially Candidate capture, baseline recovery, and reset.
- Trigger: real Dialogue exits; sample enters the `1.0` recovery tolerance before native interpolation reaches baseline.
- Consequence: reset to Inactive, immediate Candidate on the continuing tail, later generic zoom classified as Dialogue.
- Affected subsystem: Dialogue classification, telemetry ownership, gameplay zoom behavior.
- Provenance: current existing defect; whether introduced by the latest batch is **NOT ESTABLISHED**.
- Minimal repair: require bounded recovery completion/re-arm separation and give Candidate cancellation/expiry; do not hardcode FOV 70.

### D2 — Active Dialogue policy is not latched

- Evidence: **CONFIRMED**.
- Location: `src/plugin/runtime.cpp:1799-1909` and hotkey policy cycling in `src/plugin/runtime.cpp` worker/control path.
- Trigger: F10 changes policy during Candidate/Active/Exiting.
- Consequence: an in-progress lifecycle can change transform semantics despite “next dialogue” UX.
- Affected subsystem: Dialogue.
- Provenance: current existing contract defect; regression status **NOT ESTABLISHED**.
- Minimal repair: separate selected policy from lifecycle-latched active policy.

### D3 — Diagnostic owner/result can contradict the actual HorPlus writer action

- Evidence: **CONFIRMED**.
- Location: `src/plugin/runtime.cpp:2118-2209` versus `src/plugin/runtime.cpp:2467-2491`.
- Trigger: coordinator permits HorPlus while derived telemetry owner is Dialogue, Cinematic, or Recovery.
- Consequence: `horPlusNew=nan handled=false` and owner totals misrepresent actual writer output/transform count.
- Affected subsystem: FOV-state telemetry and all conclusions drawn from it.
- Provenance: current diagnostic defect; regression status **NOT ESTABLISHED**.
- Minimal repair: derive telemetry from the exact production eligibility/result or observe the post-transform value.

### D4 — Post-exit raw writer trace remains permanently armed

- Evidence: **CONFIRMED**.
- Location: `src/plugin/runtime.cpp:716-756`, EXIT arming in `src/plugin/runtime.cpp:1688-1737`, unconditional call at `src/plugin/runtime.cpp:2215-2224`.
- Trigger: first cinematic EXIT.
- Consequence: permanent guarded-read/comparison/logging overhead on every later gameplay writer callback.
- Affected subsystem: gameplay camera hot path in both modes.
- Provenance: current existing instrumentation defect; regression origin **NOT ESTABLISHED**.
- Minimal repair: production compile gate or explicit bounded disarm; keep functional exclusion state separate.

### D5 — Gameplay mode switch has no transition cleanup contract

- Evidence: **CONFIRMED**.
- Location: mode hotkey/control path and `ReplayManualTransitionOriginal()` (`src/plugin/runtime.cpp:1275-1402`) / `ApplyHorPlusGameplay()` (`src/plugin/runtime.cpp:2467-2491`).
- Trigger: AspectRecalculation <-> HorPlus at runtime.
- Consequence: HorPlus may inherit normalized 16:9 and bypass; AspectRecalculation may inherit stale replay/source state.
- Affected subsystem: gameplay mode switching.
- Provenance: current existing defect; runtime occurrence in supplied log **NOT ESTABLISHED**.
- Minimal repair: a bounded mode-transition action that restores/invalidates only state owned by the previous mode.

### D6 — Generic ZOOM signal can still create ADS ownership in optional builds

- Evidence: **CONFIRMED**.
- Location: `src/plugin/runtime.cpp:2308-2448`, `AdsLifecycle` support, `WIDEBOY_ADS_OWNER_INTEGRATION` build variants.
- Trigger: building/running an owner-integration diagnostic and observing controller-style or ADS zoom.
- Consequence: generic camera pull can suppress Dialogue or alter diagnostic ownership as though it were ADS.
- Affected subsystem: optional diagnostic integration, Dialogue gating, telemetry.
- Provenance: stale existing scaffolding invalidated by current runtime semantics.
- Minimal repair: remove/isolate owner integration and neutralize names; retain read-only transition telemetry only if still needed.

### D7 — Cinematic Auto aspect provenance is documented/logged incorrectly

- Evidence: **CONFIRMED**.
- Location: `src/plugin/runtime.cpp:988-1003` versus comment/source label at `src/plugin/runtime.cpp:1034-1045`.
- Trigger: Auto cinematic aspect store.
- Consequence: evidence can claim runtime-camera provenance while resolution actually used client/display viewport.
- Affected subsystem: cinematic telemetry/documentation, not the core viewport-based transform.
- Provenance: current existing documentation/instrumentation defect.
- Minimal repair: align comment and source label with the actual resolver.

## 15. Plausible risks requiring evidence

1. `CinematicActive` excludes only the cached ENTER-transformed value; another already-transformed cinematic sample could receive gameplay HorPlus again.
2. A missed cinematic EXIT could leave `CinematicActive`; a recovery target that never reconverges could leave `CinematicExiting` or post-cinematic Dialogue exclusion active.
3. Dialogue `Exiting` can remain stale across a direction reversal because it has no explicit re-entry/cancel rule.
4. Diagnostic and logging overhead may contribute to stutter, but no current causal measurement exists.
5. A structural resolver look-ahead window could cross a section boundary on an unsupported binary.
6. Direct load into an already-active cinematic may not execute the ENTER boundary.

Each is actionable as an evidence question; none is asserted as an observed failure here.

## 16. Things explicitly NOT established

- An authoritative source for `ConfiguredGameplayFov`.
- Global one-to-one correspondence between ZOOM samples and gameplay-writer callbacks.
- That every native interpolation sample reaches the validated writer.
- A whole-engine proof of no transformed-output feedback into a later native source.
- Actual post-hook XMM0/store values from the merged pre-transform telemetry.
- Generic absence or presence of cinematic double transforms.
- Runtime behavior of cinematic ENTER, EXIT, direct-load, aspect store, or recovery in the supplied run.
- Runtime correctness of HorPlus <-> AspectRecalculation switching.
- Standalone ASI access to `APC::IsInStaticDialog()`.
- A positive game-owned Dialogue discriminator.
- Current HorPlus processing as the cause of historical stutter.
- Runtime support for 2.0.2–2.0.4 beyond existing static resolver evidence.
- Any requirement that native cinematic FOV equal 90.

## 17. Minimal bounded repair candidates

1. **Evidence integrity:** make merged telemetry observe the same eligibility/result as production; distinguish classifier phase from camera owner; remove behavior-changing ADS ownership from observational builds.
2. **Hot-path containment:** remove or compile-gate raw post-exit writer tracing, or disarm it at a strictly bounded terminal condition independent of functional recovery state.
3. **Dialogue boundary:** add Candidate expiry and cancellation on stabilization/reversal/source discontinuity; prevent recovery-tail immediate re-arm; preserve generic FOV values rather than magic-number classification.
4. **Dialogue policy:** snapshot selected policy when a lifecycle is accepted and use that active policy through recovery.
5. **Mode transition:** define minimal exit/entry cleanup for AspectRecalculation <-> HorPlus, including owned aspect restoration and replay/source invalidation.
6. **Cinematic evidence correctness:** correct Auto aspect provenance and keep cinematic/native gameplay state domains explicitly separate.
7. **Terminology cleanup:** rename/remove `WideboyAds`/`AdsLifecycle` scaffolding now that the signals are known to be generic zoom transitions.

These are separable bounded changes. They do not require unifying HorPlus and AspectRecalculation or inventing an engine ownership signal.

## 18. Recommended order of next static/implementation batches

1. Telemetry truthfulness and production trace containment.
2. Remove/isolate stale ADS-owner integration and neutralize ZOOM terminology.
3. Dialogue Candidate/recovery lifecycle repair plus selected/active policy split.
4. Gameplay mode-switch transition contract.
5. Cinematic Auto provenance cleanup and narrowly targeted review of the one-value double-transform guard.
6. Only then prepare one diagnostic artifact whose macros and behavioral effects are explicitly enumerated.

This order first makes subsequent runtime evidence trustworthy, then repairs the reproduced lifecycle defect, then addresses unexercised cross-mode/cinematic risks.

## 19. Minimal runtime matrix only after static repairs are ready

| Scenario | Required observation | Purpose |
| --- | --- | --- |
| Baseline 16:9 and arbitrary ultrawide Gameplay | pre-native and post-writer FOV, current aspect/flags | confirm identity/bypass and one writer transform |
| ADS zoom in/out | ZOOM weights plus post-writer FOV | confirm generic transition reaches writer without Dialogue ownership |
| Controller camera pull | same as ADS | confirm no ADS-specific state is created |
| Real Dialogue enter/exit, then idle | classifier phase, latched policy, post-writer FOV | confirm bounded recovery and no immediate Candidate re-arm |
| Gameplay zoom immediately after Dialogue | phase/owner and final writer value | regression check for the supplied false positive |
| Change Dialogue hotkey during active Dialogue | selected and active policies | confirm next-lifecycle semantics |
| Gameplay -> cinematic -> Gameplay in HorPlus | ENTER/EXIT native/result and first restored gameplay samples | confirm domain separation and no double transform |
| Direct load into cinematic | whether ENTER boundary executes and resulting FOV/aspect | resolve the current unknown |
| Cinematic exit followed by gameplay zoom | exclusion/recovery terminal edge and trace disarm | confirm no stale recovery and no permanent writer instrumentation |
| HorPlus -> AspectRecalculation -> HorPlus | source aspect/flags and replay state at each switch | confirm transition cleanup |
| Runtime aspect change in Gameplay, Dialogue, and Cinematic | actual source used and post-transform value | confirm current-aspect behavior without owner-label ambiguity |

The diagnostic should record the value after production transformation, identify enabled compile-time diagnostics, and avoid synchronous sample-by-sample logging unless that specific trajectory is under test. Performance conclusions require a separate clean production-versus-diagnostic measurement; this matrix is for behavioral correctness only.
