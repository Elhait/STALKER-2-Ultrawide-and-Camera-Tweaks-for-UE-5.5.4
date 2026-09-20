# Gameplay Aspect Restoration Consumer — Pre-Cutover Contract Audit

Date: 2026-09-19  
Scope: read-only audit of `ApplyPendingGameplayModeTransition()` and the legacy observed-aspect state after GameplayBaseline Batch 2A, 2A.1 and Cinematic ENTER Batch 2B.

## Verdict

The legacy aspect state is a specialized restoration snapshot, not merely a copy of the latest coherent Gameplay FOV observation.

`GameplayBaseline.aspect` is equivalent only for the normal eligible Gameplay-writer subset. It is not equivalent for all current legacy update paths because the legacy cache can update from a valid aspect even when FOV is invalid or unavailable, and it can update while Gameplay baseline projection is not active.

```yaml
legacy_aspect_semantics: ESTABLISHED
gameplay_baseline_aspect_equivalence: PARTIAL
direct_consumer_cutover_safe: NO
separate_restoration_state_required: YES
legacy_aspect_state_deletable: NO
runtime_required_before_implementation: NO
verdict: SEPARATE_RESTORATION_PROJECTION_REQUIRED
```

## 1. Authority and current source

Current source is authoritative. The relevant implementation is in:

- `src/plugin/runtime.cpp::ReplayManualTransitionOriginal`;
- `src/plugin/runtime.cpp::ApplyHorPlusGameplay`;
- `src/plugin/runtime.cpp::ApplyPendingGameplayModeTransition`;
- `src/plugin/runtime.cpp::ReplayManualTransition`;
- `src/plugin/runtime.cpp::SelectGameplayMode`;
- `src/camera/gameplay_baseline.hpp/.cpp`;
- `src/camera/fov_observation.hpp/.cpp`.

Reviewed context reports include the global migration audit, retained-state audit, Batch 2A/2A.1 reports and the Batch 2B pre-implementation and ENTER cutover reports.

## 2. Exact legacy aspect-state definition

```cpp
g_lastObservedAspect       // atomic<float>
g_lastObservedAspectValid  // atomic<bool>
```

The source-defined meaning is:

> the latest externally observed valid camera aspect that can serve as the restoration target for a pending `AspectRecalculation -> HorPlus` transition, excluding the fix-owned native Auto restore.

It is not defined as the latest FOV sample's aspect, the latest `GameplayBaseline.aspect`, the cinematic aspect policy, or necessarily the current camera aspect. The float and validity are separate atomics, not one coherent publication.

## 3. All production writers

### `ReplayManualTransitionOriginal`

This is the primary legacy writer. After reading the camera aspect and flags, it computes `isOwnAutoRestore` and `isPendingOwnAutoRestore`. For a valid aspect it updates the pair unless either exclusion applies.

Important ordering and semantics:

- the aspect update occurs before the later FOV-validity check used by factual FOV observation;
- valid aspect does not require valid FOV for the legacy cache update;
- source readability and aspect validity are required;
- fix-owned native Auto restore is intentionally not retained;
- this writer is not restricted to the narrower `GameplayBaseline` eligibility;
- in `AspectRecalculation + Gameplay`, a separate valid native observation may also project into `GameplayBaseline`.

### Gameplay-disabled writer path

`ReplayManualTransition` updates the legacy aspect pair for any readable valid aspect when gameplay is disabled. It does not establish a `GameplayBaseline`. This is a direct semantic difference.

### `ApplyHorPlusGameplay`

When coordinator ownership is `Gameplay`, valid native FOV and aspect update the legacy pair. The callback also publishes a `CameraWriter` observation and projects `GameplayBaseline`.

When coordinator is `CinematicActive`, this function may transform or pass through the writer sample but does not update the legacy restoration pair. Cinematic samples therefore do not replace Gameplay restoration state through this path.

### `SelectGameplayMode`

For `HorPlus -> AspectRecalculation`, the source explicitly stores native aspect, clears legacy validity, clears legacy source/FOV, and invalidates `GameplayBaseline`.

For `AspectRecalculation -> HorPlus`, it only arms the pending transition. Population happens later from a writer callback.

### Startup/reset

Startup/reset uses native aspect as the numeric reset value and sets validity false. No restoration target is valid until a qualifying observation updates it.

## 4. All production readers

The current production reader of both legacy fields is:

```text
ApplyPendingGameplayModeTransition()
```

After Batch 2B, Cinematic ENTER no longer reads this aspect pair. Diagnostic code may expose related values but is not a production consumer.

## 5. `ApplyPendingGameplayModeTransition()` dataflow

The function returns unless all are true:

```text
transition pending
target mode == HorPlus
Gameplay enabled
coordinator == Gameplay
```

It reads current source aspect and flags. Unreadable fields cause deferral with the pending request retained.

If current runtime aspect is already ultrawide, the request is consumed without a write. Otherwise it independently loads the cached aspect and validity. Invalid/non-ultrawide cached state causes deferral. A valid ultrawide cached value is written with the current callback's flags; write failure defers.

The restored value is therefore the last retained applicable ultrawide aspect, not a FOV value and not the effective cinematic aspect.

## 6. Mode-transition lifecycle

### AspectRecalculation -> HorPlus

`SelectGameplayMode` marks the request pending. The next writer callback either defers on unreadable state, consumes the request if already ultrawide, or restores the retained ultrawide aspect using current flags. Successful write consumes the request.

### HorPlus -> AspectRecalculation

The pending request is cleared, legacy validity is cleared, source/FOV are reset, and `GameplayBaseline` is invalidated. Later AspectRecalculation callbacks can repopulate the legacy aspect cache from an externally observed valid aspect.

In the constrained 21:9 path, the aspect is observed before the native normalization write, so the constrained value remains available for later restoration. The fix-owned native restore is excluded.

### Round trip

The round trip is: invalidate -> observe constrained aspect -> normalize native state -> arm restoration -> restore retained constrained aspect or consume if already ultrawide.

## 7. What restoration restores

The source supports option **B**:

```text
last ultrawide aspect observed before native AspectRecalculation normalization
```

It is not simply the latest current aspect, the latest coherent baseline aspect, or an automatically captured transition-start snapshot. The constrained 21:9 path demonstrates that observation occurs before the fix's native normalization and is then retained.

## 8. Auto-restore exclusion

The exclusion remains active. It recognizes the fix-owned native restore through replay lifecycle state and, where available, restore source. The pending restore is recognized while the state is still `AppliedConstrainPass`.

The excluded value is the fix-written native aspect. The retained value must remain the previous externally observed ultrawide aspect. A plain latest-factual replacement would erase the restoration target.

This is a restoration semantic, not a generic FOV observation rule.

## 9. Event-by-event comparison

| Event/path | Legacy aspect state | `GameplayBaseline.aspect` | Result |
|---|---|---|---|
| normal HorPlus Gameplay | updates with valid aspect/FOV | coherent eligible observation | equivalent for supported sample |
| HorPlus pass-through | updates when valid | native pass-through projection | equivalent when FOV valid |
| AspectRecalculation Gameplay | updates before FOV validation | projects only after valid FOV/observation | partial; invalid-FOV aspect differs |
| mode invalidation | explicit validity clear | separate baseline invalidation | aligned for reset, different state |
| first sample after transition | retained target can be aspect-only | requires valid baseline observation | partial |
| fix-owned Auto restore | retains prior external aspect | projection excluded by guard | equivalent on guarded path |
| invalid aspect | no update; prior state retained | no projection; prior baseline retained | broadly similar, different eligibility |
| invalid flags | legacy aspect writer does not require valid flags | observation requires flags-valid | not equivalent |
| CinematicActive writer | does not update via HorPlus path | not projected | aligned non-contamination |
| cached transformed ENTER | no restoration update | explicitly excluded | aligned non-contamination |
| ADS-like Gameplay | valid Gameplay writer may update | valid native projection | equivalent when eligible |
| binocular Gameplay | valid Gameplay writer may update | valid native projection | equivalent when eligible |
| camera/source recreation | waits for next readable legacy aspect | waits for committed eligible observation | runtime validation needed |

The decisive mismatch is that legacy restoration tracks an applicable aspect independently of FOV validity and baseline eligibility.

## 10. Coordinator and subsystem dependencies

Restoration requires Gameplay coordinator ownership at the consumer. CinematicActive writer samples must not replace the restoration target. Cinematic ENTER/EXIT and `ResolveCinematicAspect()` are not sources for Gameplay restoration.

Dialogue has no required cutover dependency. Its source/FOV invalidation remains separate. ZOOM callbacks have no restoration authority. ADS and binocular writer aspects may legitimately update factual Gameplay state when the coordinator is Gameplay; ZOOM observations themselves must not update it.

## 11. Flags and arbitrary aspects

The legacy restoration state stores only aspect. The consumer reads current flags and preserves them while restoring the retained aspect. Flags are callback-local for this contract; no source evidence requires storing them in the retained aspect state.

The surrounding paths accept the established `0x4`/`0x5` cases. Restoration does not whitelist canonical aspect ratios. Physical 21:9 variants, 32:9, 3:1 and other finite ultrawide values remain values, not classes.

## 12. Invalid and unavailable behavior

| Condition | Current behavior |
|---|---|
| startup/no observed target | validity false; defer |
| unreadable current camera state | retain pending; defer |
| invalid current aspect | no write; retain pending |
| invalid cached aspect | retain pending; defer |
| write failure | retain pending; defer |
| current aspect already ultrawide | consume without write |
| camera recreation/direct load | wait for a readable eligible callback; no synthetic target |

Exact repopulation timing after every recreation path is not established statically and is a future runtime validation point.

## 13. Coherence assessment

The legacy pair permits a theoretical torn semantic read because the float and validity are loaded independently:

```text
new aspect + old validity
old aspect + new validity
```

`GameplayBaselineStore` is whole-snapshot coherent, but its narrower eligibility and FOV requirement make it an unsafe direct replacement. The right future shape is a separate coherent restoration projection, not a reinterpretation of `GameplayBaseline.aspect`.

## 14. Required future projection

If migrated later, use a separate semantic projection from committed `CameraWriter` facts plus explicit mode/lifecycle eligibility:

```cpp
struct GameplayAspectRestorationState {
    float aspect;
    FovWriterSourceToken source;
    std::uint64_t observationSequence;
    bool valid;
};
```

This is conceptual, not an implementation decision. It must preserve update, retention, invalidation and Auto-restore exclusion semantics. Flags should remain callback-local unless further evidence requires retention.

## 15. Exact bounded future cutover

```text
committed CameraWriter observation
    + explicit restoration eligibility
    -> coherent GameplayAspectRestorationState
    -> ApplyPendingGameplayModeTransition reads one snapshot
```

The future batch must keep the legacy pair as a shadow/reference, preserve the current deferral/no-op/write-failure behavior, and compare old/new decisions. It must not delete legacy state in the same batch. Deletion is a separate post-runtime cleanup batch.

## 16. Deterministic harness matrix

| Case | Expected result |
|---|---|
| startup/no aspect | INVALIDATE/DEFER |
| normal HorPlus ultrawide | UPDATE/APPLY |
| HorPlus pass-through | UPDATE/APPLY when FOV valid |
| AspectRecalculation | UPDATE before normalization; retain owned restore exclusion |
| AspectRecalculation -> HorPlus | APPLY or consume if already ultrawide |
| HorPlus -> AspectRecalculation | INVALIDATE |
| round trip | UPDATE constrained aspect, then APPLY |
| invalid aspect/flags | RETAIN or DEFER; fail closed |
| unreadable state | DEFER |
| fix-owned Auto restore | RETAIN prior external aspect |
| custom 3:1/noncanonical 21:9 | exact value, no whitelist |
| CinematicActive/cached ENTER | RETAIN; no contamination |
| ADS/binocular Gameplay | UPDATE only from eligible writer evidence |
| source/camera recreation | DEFER until coherent eligible observation |

Future harness must compare legacy and new decisions, especially valid-aspect/invalid-FOV, invalid-flags and Auto-restore cases.

## 17. Runtime gate and non-goals

The contract and mismatch are statically established; runtime is not required before designing the projection. Runtime is required before switching the production reader. One combined scenario should cover custom ultrawide setup, both mode directions and round trip, Auto restore, arbitrary aspect, camera recreation/save-load, ADS/binocular, Dialogue/Cinematic coexistence, and legacy-shadow versus new-projection decisions.

This audit does not modify source, migrate the consumer, delete legacy state, redesign `GameplayBaseline`, change Cinematic ENTER, change HorPlus math, change AspectRecalculation, migrate Dialogue/ZOOM/ADS, repair cached ENTER guard, add hooks, or perform performance work.

## Final answers

`g_lastObservedAspect` is a historical restoration target with broader update eligibility and a fix-owned Auto-restore exclusion. `GameplayBaseline.aspect` is the aspect paired with a coherent eligible native Gameplay FOV observation. The identical `float` type hides different temporal and semantic contracts.

Therefore the direct cutover is not safe. A separate coherent restoration projection is required if this consumer is migrated. Until shadow comparison and runtime validation pass, the legacy specialized pair remains authoritative and deletable status is **NO**.
