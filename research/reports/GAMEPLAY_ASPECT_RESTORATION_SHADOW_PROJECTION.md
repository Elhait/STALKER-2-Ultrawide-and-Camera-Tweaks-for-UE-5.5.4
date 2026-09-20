# GameplayAspectRestorationState — Shadow Projection Batch

Date: 2026-09-19  
Status: implemented and statically/harness validated; runtime not performed.

## Scope and architecture

This batch adds a coherent shadow representation of the established legacy restoration semantics. The production consumer is unchanged: `ApplyPendingGameplayModeTransition()` still reads `g_lastObservedAspect` and `g_lastObservedAspectValid`.

The new state is explicitly not `GameplayBaseline.aspect` and not the latest generic camera aspect. It is a historical restoration-target projection.

Implemented in `src/camera/gameplay_aspect_restoration.hpp/.cpp`:

```cpp
struct GameplayAspectRestorationState {
    float aspect;
    FovWriterSourceToken source;
    std::uint64_t observationSequence;
    bool valid;
};
```

`GameplayAspectRestorationStore` replaces the whole state under one mutex and returns copies after unlocking. It exposes `Update`, `Retain`, `Invalidate` and `Read`. Its sequence is local shadow commit provenance only, not a game generation, timeout or ownership proof.

No FOV, HorPlus result, Dialogue, ZOOM, cinematic policy, mode copy or flags were added to the retained state.

## Factual source and projection points

The shadow uses the same local aspect facts as the legacy path:

- `ReplayManualTransitionOriginal`: after the existing Auto-restore exclusions and at the same point as the legacy aspect update;
- Gameplay-disabled `ReplayManualTransition`: valid readable aspect updates the shadow, matching legacy behavior;
- `ApplyHorPlusGameplay` under `CoordinatorState::Gameplay`: valid Gameplay writer aspect updates both legacy and shadow state;
- `SelectGameplayMode` on `HorPlus -> AspectRecalculation`: coherent shadow invalidation occurs with legacy invalidation.

The shadow does not fabricate FOV evidence and does not require FOV or flags validity. It does not update from CinematicActive samples, cached transformed ENTER values or ZOOM callbacks.

## Semantic contract

### UPDATE

Update for every aspect-valid, non-owned-restore path where the legacy aspect pair updates. A valid aspect is sufficient even when later FOV observation publication is impossible.

### RETAIN

Retain on invalid/unreadable aspect, fix-owned Auto restore, pending own Auto restore, CinematicActive contamination paths and other established non-update paths. This preserves the historical ultrawide target.

### INVALIDATE

Clear coherently on `HorPlus -> AspectRecalculation`; numeric reset values are not valid provenance.

The preserved lifecycle is:

```text
HorPlus -> AspectRecalculation: INVALIDATE
constrained/ultrawide external aspect: UPDATE
native normalization: unchanged
fix-owned native restore: RETAIN
HorPlus: legacy consumer remains authoritative
```

The existing `isOwnAutoRestore` and `isPendingOwnAutoRestore` conditions remain in control of shadow publication ordering.

## Boundaries

Gameplay HorPlus and Gameplay pass-through update the shadow wherever the legacy path updates the aspect. Gameplay-disabled valid aspect also updates it intentionally. Dialogue source/FOV invalidation remains separate. ADS/binocular samples are ordinary Gameplay evidence when coordinator ownership is Gameplay; ZOOM observations have no restoration authority. CinematicActive and cached transformed ENTER paths do not contaminate the state.

The retained state stores no flags. Restoration continues to use current callback flags. No aspect whitelist was added; physical noncanonical 21:9, 32:9 and custom 3:1 remain factual values.

## Harness and equivalence coverage

The new deterministic harness covers:

- startup invalid state;
- normal and consecutive updates;
- valid aspect with modeled invalid FOV;
- valid aspect with modeled invalid flags;
- fix-owned and pending Auto restore retention;
- Gameplay-disabled update;
- CinematicActive retention;
- invalidation and repopulation;
- custom/noncanonical aspect values;
- no stale source/sequence after invalidation.

It also models the future consumer decision without changing production: defer with no target, consume when already ultrawide, restore a valid ultrawide target, and fail closed for invalid target.

## Diagnostics

When `[Diagnostics] Enabled=true`, changed shadow actions emit bounded `RESTORATION_STATE` and `RESTORATION_COMPARE` records. Identical repeated updates do not emit a new state record. The comparison reports the fragmented legacy read; a possible torn legacy read is not treated as a shadow defect. Diagnostics never affect FOV/aspect output.

## Production paths confirmed unchanged

- `ApplyPendingGameplayModeTransition` still reads only the legacy pair;
- writer FOV output, HorPlus transform and cached ENTER guard are unchanged;
- AspectRecalculation writes are unchanged;
- `GameplayBaseline` projection is unchanged;
- Cinematic ENTER, Dialogue, ZOOM and resolver/hook paths are unchanged.

## Validation

- focused restoration harness: PASS;
- full `test.cmd`: PASS;
- production `build.cmd`: PASS;
- `git diff --check`: PASS, with only normal line-ending warnings in pre-existing touched files;
- known external Zydis C4201 warnings only;
- game/runtime: NOT PERFORMED.

## Combined runtime gate

One session should compare legacy and shadow telemetry across startup/direct load, stable HorPlus Gameplay, both mode directions and the round trip, physical/custom ultrawide aspect, Auto restore, ADS, binocular, Cinematic ENTER/EXIT, Dialogue coexistence and save/load or camera recreation where practical. This gate is for shadow validation only; it does not authorize consumer cutover or legacy deletion.

## Final verdict

```yaml
restoration_shadow_projection: PASS
valid_aspect_invalid_fov_equivalence: PASS
invalid_flags_equivalence: PASS
auto_restore_exclusion: PASS
aspect_recalculation_repopulation: PASS
horplus_path_equivalence: PASS
gameplay_disabled_equivalence: PASS
arbitrary_aspect_support: PASS
consumer_decision_modeled_equivalence: PASS
production_consumer: LEGACY_UNCHANGED
production_behavior_change: NONE_INTENDED
static_harness_validation: PASS
build: PASS
runtime_validation: NOT_PERFORMED
readiness: RESTORATION_SHADOW_RUNTIME_READY
```

`RESTORATION_SHADOW_RUNTIME_READY` means ready for one combined runtime shadow-validation session only. It does not mean production cutover is validated and does not make legacy state deletable.
