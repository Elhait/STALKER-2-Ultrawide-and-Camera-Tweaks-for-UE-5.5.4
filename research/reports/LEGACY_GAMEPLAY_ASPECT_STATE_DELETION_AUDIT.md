# Legacy Gameplay Aspect State — Deletion Audit

Date: 2026-09-20  
Scope: read-only repository audit after GameplayBaseline Batch 2A/2A.1 and Cinematic ENTER Batch 2B.

## Verdict

The production restoration decision no longer depends on `g_lastObservedAspect` or
`g_lastObservedAspectValid`. `GameplayAspectRestorationStore` is the current
production authority and `ApplyPendingGameplayModeTransition()` reads it directly.
The legacy pair remains only as a diagnostics/shadow comparison surface in the
current source tree.

The pair is therefore ready for a separate, bounded cleanup batch. This audit did
not remove it and did not change behavior.

```yaml
legacy_aspect_production_readers: NONE
legacy_aspect_production_writers_required: NO
restoration_state_full_replacement: YES
diagnostics_dependency: PRESENT
tests_dependency: PRESENT
safe_to_delete_legacy_aspect_pair: YES
```

`YES` means “safe to remove in the explicitly scoped cleanup batch after adapting
the diagnostics/reference surface”; it does not mean that deletion was performed
by this audit.

## Current architecture

The current production path is:

```text
valid aspect observation
    -> GameplayAspectRestorationStore
    -> ResolveGameplayAspectRestorationDecision()
    -> ApplyPendingGameplayModeTransition()
    -> restoration write/defer decision
```

`ApplyPendingGameplayModeTransition()` obtains one coherent
`GameplayAspectRestorationState` from `g_gameplayAspectRestorationStore.Read()`.
The legacy pair is loaded only inside a diagnostics conditional to report whether
the old value would have selected the same target.

`GameplayBaselineStore` is a separate retained gameplay-FOV projection used by
Cinematic ENTER / `GameplayHorPlus`. It has no current dependency on the legacy
aspect pair.

## Complete current source inventory

| Location | Operation | Classification | Finding |
| --- | --- | --- | --- |
| `src/plugin/runtime.cpp:377-378` | aliases to `RuntimeState` fields | transitional declaration | No consumer by itself. |
| `src/plugin/runtime.cpp:556-575`, `TraceGameplayAspectRestorationShadow()` | reads pair and compares it with shadow state | `DIAGNOSTICS_ONLY`, `SHADOW_REFERENCE_ONLY` | Emits `RESTORATION_STATE` and `RESTORATION_COMPARE`; no production decision. |
| `src/plugin/runtime.cpp:1126-1178`, `ApplyPendingGameplayModeTransition()` | reads pair inside `if (diagnostics::Enabled())` | `DIAGNOSTICS_ONLY`, `SHADOW_REFERENCE_ONLY` | Calculates/logs `legacyWouldChooseSameTarget`; the actual decision uses `GameplayAspectRestorationStore`. |
| `src/plugin/runtime.cpp:1480-1525`, `ReplayManualTransitionOriginal()` | writes valid observed aspect | `SHADOW_REFERENCE_ONLY` | Publishes the old reference value alongside `UpdateGameplayAspectRestorationState()`. It no longer supplies the restoration consumer. |
| `src/plugin/runtime.cpp:2967-2984`, disabled-gameplay branch | writes valid observed aspect | `SHADOW_REFERENCE_ONLY` | Keeps diagnostic/shadow evidence while gameplay is disabled; no production restoration read. |
| `src/plugin/runtime.cpp:3239-3263`, `ApplyHorPlusGameplay()` | writes valid gameplay aspect | `SHADOW_REFERENCE_ONLY` | The same callback updates the new stores, but the legacy pair is used only for old comparison telemetry. |
| `src/plugin/runtime.cpp:3620-3639`, `SelectGameplayMode()` | writes native/invalid pair on HorPlus → AspectRecalculation | `SHADOW_REFERENCE_ONLY` | Resets the old reference surface and invalidates the new restoration shadow; no production consumer reads the pair. |

There are no current source reads outside diagnostics and no current production
branch that selects a restoration target from the pair.

The `RuntimeState` fields and local aliases are still live only because the
diagnostic/reference code above still exists. They are not evidence of an
independent production owner.

## Responsibility checks

### AspectRecalculation and mode transitions

The mode transition path is fully covered by `GameplayAspectRestorationStore` for
the production decision. The old pair is reset or updated so the diagnostic
comparison remains meaningful, but this is not a second authority.

### GameplayBaseline

`GameplayBaselineStore` tracks the coherent gameplay native-FOV observation and its
optional HorPlus evidence. Its validity and retained snapshot are independent of
the legacy aspect pair. No baseline projection read requires
`g_lastObservedAspect*`.

### Cinematic ENTER and EXIT

Current Cinematic ENTER consumes the retained gameplay baseline and its own
cinematic policy/context. It does not read the legacy aspect pair. Current EXIT
and recovery paths likewise have no pair dependency.

Older reports describing a `TraceCinematicEnter()` read of the legacy pair are
historical documentation from before Batch 2B and are not current source
dependencies.

### Dialogue

Dialogue invalidation remains dependent on the separate pair:

```text
g_lastGameplayCameraSource
g_lastGameplayCameraFov
    -> source/FOV change detection
    -> Dialogue runtime invalidation
```

That is a different responsibility. Those two globals must not be included in the
legacy aspect deletion scope merely because they are adjacent in `RuntimeState`.

### ZOOM / ADS / binocular

No current ZOOM, ADS, binocular, or native transition decision reads the legacy
aspect pair. Their observations may pass through the gameplay writer, but the
pair is not their owner or classifier.

## Neighboring gameplay globals

`g_lastGameplayCameraSource` and `g_lastGameplayCameraFov` remain
`PRODUCTION_REQUIRED` in the current source. `ReplayManualTransitionOriginal()`
exchanges them on each gameplay writer callback and uses source changes or
material FOV jumps to invalidate an active Dialogue runtime. They are also reset
on the HorPlus → AspectRecalculation transition.

Their role is not replaced by `GameplayBaselineStore` in the reviewed scope, and
they are explicitly excluded from the deletion batch proposed below.

## Diagnostics and tests

Diagnostics dependency is `PRESENT`:

- `TraceGameplayAspectRestorationShadow()` reads and compares the legacy pair.
- `ApplyPendingGameplayModeTransition()` logs the legacy comparison fields.
- Current producer writes keep the old reference values available for these logs.

Tests dependency is `PRESENT` at the contract level, even though the current
deterministic harnesses do not directly access the runtime atomics. The restoration
and shadow/projection tests establish the replacement semantics and the cleanup
must update any assertions or log-schema expectations that still require
`RESTORATION_COMPARE`, `legacyAspect`, or `legacyValid`.

No test evidence requires the legacy pair to remain a production state owner.

## Historical and non-authoritative references

Repository reports and archived task plans contain older descriptions in which the
legacy pair participated in restoration or Cinematic ENTER. Those documents are
useful migration history, but current source is authoritative for this audit. The
current source shows that the production restoration consumer and Cinematic ENTER
have already moved to the new stores.

## Double-transform and stale-state check

The legacy aspect pair is not a HorPlus transform cache. It stores aspect
observations, not transformed FOV. Therefore this pair itself does not create a
current double-transform path, reverse transformed-to-native reconstruction, or
cinematic-aspect substitution. Removing it must nevertheless preserve the
policy-specific aspect reads used by the active restoration and cinematic paths.

The safe invariant remains:

```text
native FOV evidence
    -> policy-selected aspect
    -> exactly one HorPlus projection where applicable
```

Deletion of the aspect pair must not touch the separate GameplayBaseline,
Cinematic FOV cache, or `g_lastGameplayCamera*` responsibilities.

## Exact next cleanup scope

This is a recommendation for the next implementation batch, not work performed
here.

### Remove

- `RuntimeState::lastObservedAspect` and `RuntimeState::lastObservedAspectValid`.
- The corresponding local aliases `g_lastObservedAspect` and
  `g_lastObservedAspectValid`.
- The four legacy pair write sites listed above.
- The legacy loads and comparison fields in
  `TraceGameplayAspectRestorationShadow()`.
- The legacy comparison block and `legacy*` log fields in
  `ApplyPendingGameplayModeTransition()`.

### Preserve

- `GameplayAspectRestorationStore` and its production consumer.
- `GameplayBaselineStore` and Cinematic ENTER behavior.
- All actual aspect reads from the camera context used by gameplay/mode logic.
- `g_lastGameplayCameraSource` and `g_lastGameplayCameraFov`.
- Dialogue, ZOOM/ADS, cinematic EXIT/recovery, HorPlus math, and
  AspectRecalculation behavior.

### Validation for that future cleanup batch

1. Repository-wide search proves no remaining pair symbol or stale comparison.
2. Relevant restoration and gameplay-baseline harnesses pass.
3. Full project tests and production build pass.
4. Read-only Git diff review confirms only the approved pair/diagnostic scope.
5. One focused runtime regression rechecks F11
   `HorPlus -> AspectRecalculation -> HorPlus`, including restoration write and
   `writeSuccess=true`, because the supplied runtime evidence establishes the
   replacement but the cleanup must confirm no wiring was accidentally removed.

No dead-state deletion beyond this pair is authorized by this audit. In
particular, deletion of `g_lastGameplayCameraSource/Fov` requires a separate
responsibility audit after Dialogue behavior has an explicit replacement.

## Final classification

```yaml
g_lastObservedAspect:
  current_reads: DIAGNOSTICS_ONLY / SHADOW_REFERENCE_ONLY
  current_writes: SHADOW_REFERENCE_ONLY
  production_authority: NONE

g_lastObservedAspectValid:
  current_reads: DIAGNOSTICS_ONLY / SHADOW_REFERENCE_ONLY
  current_writes: SHADOW_REFERENCE_ONLY
  production_authority: NONE

g_lastGameplayCameraSource:
  classification: PRODUCTION_REQUIRED
  responsibility: Dialogue source-continuity and camera-context invalidation

g_lastGameplayCameraFov:
  classification: PRODUCTION_REQUIRED
  responsibility: Dialogue material-FOV-change invalidation

legacy_aspect_production_readers: NONE
legacy_aspect_production_writers_required: NO
restoration_state_full_replacement: YES
diagnostics_dependency: PRESENT
tests_dependency: PRESENT
safe_to_delete_legacy_aspect_pair: YES
```

The deletion pair is cleanup-ready, but no deletion or behavior change was made
in this audit.
