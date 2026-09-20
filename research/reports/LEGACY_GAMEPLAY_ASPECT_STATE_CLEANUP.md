# Legacy Gameplay Aspect State — Cleanup

Date: 2026-09-20  
Scope: removal of the obsolete legacy gameplay-aspect pair after the production
restoration cutover.

## Implementation

Removed from `src/plugin/runtime.cpp`:

- `RuntimeState::lastObservedAspect`;
- `RuntimeState::lastObservedAspectValid`;
- `g_lastObservedAspect` and `g_lastObservedAspectValid` aliases;
- all legacy aspect-pair writes and reset writes;
- legacy comparison loads and `RESTORATION_COMPARE` telemetry;
- `legacyAspect`, `legacyValid`, and `legacyWouldChooseSameTarget` fields from
  restoration-consumer telemetry.

`TraceGameplayAspectRestorationShadow()` was renamed to
`TraceGameplayAspectRestorationState()` because it now logs only the current
`GameplayAspectRestorationStore` state. This was a local naming cleanup with no
semantic change.

Preserved unchanged:

- `GameplayAspectRestorationStore` and its decision tree;
- restoration producers and `ApplyPendingGameplayModeTransition()` behavior;
- `GameplayBaseline` and Cinematic ENTER integration;
- `g_lastGameplayCameraSource` and `g_lastGameplayCameraFov`;
- Dialogue invalidation, ZOOM/ADS/binocular, Cinematic, HorPlus and
  AspectRecalculation behavior.

## Validation

Repository-wide runtime/source search:

```text
src/ + tests/ + test.cmd + build.cmd:
g_lastObservedAspect                 0
g_lastObservedAspectValid            0
lastObservedAspect fields            0
legacyAspect/legacyValid telemetry   0
legacyWouldChooseSameTarget          0
TraceGameplayAspectRestorationShadow 0
```

Historical research reports still mention the removed names as migration
evidence. They were intentionally not rewritten in this cleanup; they are not
compiled source, runtime consumers or test inputs.

Validation results:

- restoration harness: PASS;
- GameplayBaseline harness: PASS;
- FOV observation harness: PASS;
- relevant camera/cinematic/gameplay/dialogue harnesses: PASS;
- full `test.cmd`: PASS;
- `build.cmd`: PASS;
- `git diff --check`: PASS.

The build emitted only the existing external Zydis C4201 unnamed-struct warnings;
there were no cleanup-related errors.

## Final verdict

```yaml
legacy_aspect_pair_removed: PASS
stale_references: NONE
gameplay_restoration_authority: GameplayAspectRestorationState
gameplay_camera_source_fov_state: PRESERVED
production_behavior_change: NONE_INTENDED
tests: PASS
build: PASS
runtime: NOT_PERFORMED
```

```text
LEGACY_ASPECT_CLEANUP_RUNTIME_READY
```

The requested runtime regression remains for the next step:

```text
F11: HorPlus -> AspectRecalculation -> HorPlus
ADS after each transition, without reload
```

No game launch, commit, release packaging or unrelated legacy cleanup was
performed.
