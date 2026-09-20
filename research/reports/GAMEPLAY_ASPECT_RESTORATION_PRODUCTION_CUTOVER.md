# Gameplay Aspect Restoration — Production Consumer Cutover

Date: 2026-09-19  
Status: production consumer migrated; post-cutover runtime not performed.

## Exact change

`ApplyPendingGameplayModeTransition()` now reads one copy from `GameplayAspectRestorationStore::Read()` and uses that copy as the sole retained restoration target. The current callback still supplies the current aspect and current flags.

Before:

```text
legacy atomic aspect + legacy atomic validity
    -> restoration decision
```

After:

```text
coherent GameplayAspectRestorationState snapshot
    -> restoration decision
current callback aspect/flags
    -> current-state checks and write
```

There is no production fallback from the coherent state to `g_lastObservedAspect` or `g_lastObservedAspectValid`.

## Preserved decision tree

- no pending/wrong target/disabled/non-Gameplay coordinator: unchanged early return;
- unreadable current camera state: `DEFER`;
- invalid current aspect: `DEFER`;
- current aspect already ultrawide: `CONSUME_NO_WRITE`;
- invalid/unavailable coherent restoration state or non-restorable target: `DEFER`;
- valid retained target: `RESTORE` using current callback flags;
- write failure: `DEFER`;
- successful write: consume pending transition.

The consumer does not use source token, sequence, FOV validity, writer flags stored in the state, `GameplayBaseline`, ZOOM, Dialogue or cinematic state as new production predicates.

## Legacy state after cutover

`g_lastObservedAspect` and `g_lastObservedAspectValid` remain allocated and their existing producers/invalidation remain unchanged. They are no longer production inputs to this consumer. When diagnostics are enabled, they are read only as a comparison reference and are labeled `legacyAspect` / `legacyValid`.

`RESTORATION_CONSUMER` telemetry reports the coherent production target, legacy reference, current aspect/flags and `DEFER`, `CONSUME_NO_WRITE` or `RESTORE` decision. Restore results additionally report `restoredAspect` and write success. Diagnostics do not affect decisions.

## Producers and boundaries unchanged

No producer semantics were changed in this batch:

- `ReplayManualTransitionOriginal` and its Auto-restore exclusion;
- Gameplay-disabled aspect publication;
- `ApplyHorPlusGameplay` restoration projection;
- `GameplayBaseline` projection;
- AspectRecalculation native writes;
- Cinematic ENTER/EXIT and FovMode;
- Dialogue, ZOOM, ADS/binocular and recovery semantics;
- cached ENTER guard, hooks and resolvers.

## Deterministic consumer harness

The restoration harness now also covers the pure consumer decision model:

- no pending transition: `NO_ACTION`;
- unreadable/invalid current state: `DEFER`;
- current aspect already ultrawide: `CONSUME_NO_WRITE`;
- invalid coherent target: `DEFER`;
- valid target: exact `RESTORE`;
- arbitrary/noncanonical target: exact restore value;
- coherent target remains authoritative when a modeled legacy value differs;
- current flags remain consumer-local and are not taken from retained state.

The existing shadow/projection harness remains in place for producer semantics and lifecycle invalidation/repopulation.

## Targeted reader audit

Source search confirms:

```text
ApplyPendingGameplayModeTransition
    -> production decision reads GameplayAspectRestorationStore
    -> no legacy fallback
```

The only legacy reads in this function are inside diagnostics-gated reference comparison. Other legacy writes remain because they still support shadow/reference telemetry and independent responsibilities; deletion is explicitly deferred.

## Validation

- focused restoration consumer/lifecycle harness: PASS;
- existing restoration, GameplayBaseline and FOV observation harnesses: PASS;
- full `test.cmd`: PASS;
- production `build.cmd`: PASS;
- `git diff --check`: PASS, with normal line-ending warnings only;
- known external Zydis C4201 warnings only;
- runtime/game launch: NOT PERFORMED.

## Post-cutover runtime matrix

Run one combined session covering:

1. startup/direct load;
2. stable HorPlus Gameplay;
3. HorPlus -> AspectRecalculation invalidation;
4. constrained/custom aspect repopulation;
5. fix-owned Auto restore retention;
6. AspectRecalculation -> HorPlus exact restoration;
7. complete round trip;
8. ADS and binocular;
9. Cinematic ENTER/EXIT;
10. GameplayHorPlus and NativeHorPlus cinematic modes where practical;
11. Dialogue coexistence;
12. custom/noncanonical aspect and camera recreation/save-load where practical.

Telemetry acceptance should show coherent production target, legacy reference, decision, restored target, current flags and pending lifecycle. Legacy-reference mismatches must be interpreted with the fragmented-atomic/torn-read caveat.

## Final verdict

```yaml
production_consumer_cutover: PASS
coherent_snapshot_read: PASS
legacy_production_fallback: NONE
decision_tree_preserved: PASS
fail_closed_behavior: PASS
current_flags_preserved: PASS
arbitrary_aspect_support: PASS
restoration_producers: UNCHANGED
legacy_reference_state: RETAINED
deterministic_consumer_harness: PASS
lifecycle_harness: PASS
full_tests: PASS
build: PASS
runtime_validation: NOT_PERFORMED
readiness: RESTORATION_PRODUCTION_CUTOVER_RUNTIME_READY
```

`RESTORATION_PRODUCTION_CUTOVER_RUNTIME_READY` means the production consumer is migrated and ready for the separate post-cutover runtime gate. Legacy state is not deletable yet.
