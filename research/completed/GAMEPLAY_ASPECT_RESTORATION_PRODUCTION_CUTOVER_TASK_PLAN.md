# Gameplay Aspect Restoration Production Cutover — Task Plan

## Objective

Switch exactly one production consumer, `ApplyPendingGameplayModeTransition()`, from the fragmented legacy aspect pair to the coherent `GameplayAspectRestorationState` snapshot.

## Established evidence/current state

- Shadow projection is implemented and deterministic-harness validated.
- Full existing tests and production build passed.
- Runtime shadow validation was reported as a clean comparison with the mode round trip and no cinematic/dialogue contamination.
- Legacy producers and Auto-restore exclusions are established and remain unchanged.

## Approved scope

- Read one coherent restoration snapshot in the production consumer.
- Preserve current callback aspect/flags, defer/no-op/write-failure behavior and fail-closed semantics.
- Remove production fallback to the legacy pair for this consumer.
- Retain legacy pair as shadow/reference telemetry.
- Add deterministic consumer/lifecycle harness and bounded decision diagnostics.

## Explicit non-goals

- No legacy state deletion.
- No producer changes except none required for the cutover.
- No GameplayBaseline, HorPlus, Cinematic, Dialogue, ZOOM, ADS, AspectRecalculation, cached-ENTER, hook, resolver or performance changes.
- No game launch, commit or release.

## Expected files/areas

- `src/camera/gameplay_aspect_restoration.hpp/.cpp` decision model.
- `src/plugin/runtime.cpp` one consumer and diagnostics only.
- focused restoration consumer/lifecycle harness and `test.cmd` entry.
- `research/reports/GAMEPLAY_ASPECT_RESTORATION_PRODUCTION_CUTOVER.md`.

## Validation

Focused and existing harnesses, full `test.cmd`, production `build.cmd`, `git diff --check`, targeted reader audit and static scope review. Runtime is a separate post-cutover gate.

## Stop conditions

Stop before legacy deletion, producer changes, unrelated migration or game launch.

## Final review

Confirm the consumer no longer uses legacy production input, preserve unrelated working-tree changes, and report runtime as not performed. Plan archived after implementation validation.
