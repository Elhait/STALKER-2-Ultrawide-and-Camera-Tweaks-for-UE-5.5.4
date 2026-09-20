# GameplayBaseline Native Coverage Extension — Batch 2A.1 Task Plan

## Objective

Extend the parallel shadow `GameplayBaseline` projection so native Gameplay
target validity does not depend on a transformed HorPlus result, covering
valid pass-through and AspectRecalculation writer events without migrating any
production consumer.

## Established evidence and current state

- Batch 2A has a mutex-protected `GameplayBaseline` shadow store.
- Pre-2B review identified gaps for valid pass-through samples and
  AspectRecalculation samples.
- Current ENTER still reads legacy state and must remain unchanged.
- `ReplayManualTransitionOriginal` retains Dialogue and legacy state duties.

## Approved scope

- Make native FOV validity required and HorPlus result validity optional.
- Accept truthful native pass-through CameraWriter observations as native-only
  updates.
- Publish/project valid AspectRecalculation Gameplay writer observations.
- Preserve whole-snapshot replacement, RETAIN semantics and explicit mode
  invalidation.
- Extend deterministic harness and diagnostics/reporting as needed.

## Explicit non-goals

- No production consumer migration, legacy global removal or Batch 2B.
- No FOV/aspect math, cinematic policy, Dialogue, ZOOM, ADS classifier,
  CameraState, resolver, hook or generation changes.
- No new ownership fields in factual observations.
- No runtime game launch, Git commit or release.

## Expected files or areas

- `src/camera/gameplay_baseline.hpp/.cpp`
- `src/plugin/runtime.cpp`
- `tests/camera/gameplay_baseline_harness.cpp`
- `research/reports/GAMEPLAY_BASELINE_BATCH2A1_NATIVE_COVERAGE.md`
- `backlog/TASKLOG.md`

## Batches and validation

1. Revise native/optional transformed schema and projector predicate.
2. Add truthful AspectRecalculation CameraWriter publication and projection;
   keep legacy writes/order and ENTER consumer unchanged.
3. Extend native/pass-through/AspectRecalculation equivalence harness.
4. Run `test.cmd`, `build.cmd`, `git diff --check`, review scope and report.

## Risks and safe failure

- Invalid, cached transformed or semantically ineligible observations retain
  the previous baseline and cannot fabricate a native target.
- Pass-through updates replace the whole snapshot and clear optional stale
  HorPlus evidence.
- If legacy ordering or production consumers would need changing, stop and
  report the scope conflict.

## Stop conditions and phase gates

- Stop after static/harness/build validation and report.
- Do not switch Cinematic ENTER to the new baseline.
- Batch 2B requires a separate plan and review.

## Expected final Git review

Confirm changed paths match this plan, distinguish them from the pre-existing
dirty worktree, and report native coverage, equivalence limits and remaining
Batch 2B gaps separately.
