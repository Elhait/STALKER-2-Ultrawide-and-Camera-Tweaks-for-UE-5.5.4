# GameplayBaseline to Cinematic ENTER Production Cutover — Batch 2B Task Plan

## Objective

Switch only the `Cinematic ENTER / GameplayHorPlus` target selection from the
distributed legacy FOV/aspect pair to one coherent `GameplayBaseline` snapshot.
Preserve NativeHorPlus, fallback, cinematic aspect policy, math and all legacy
responsibilities.

## Established evidence and current state

- Batch 2A.1 provides native Gameplay coverage for transformed, pass-through and
  AspectRecalculation writer events.
- `GameplayBaseline.nativeFov` is the required production target; optional
  `horPlusFov` is evidence only.
- Pre-2B review established that `GameplayBaseline.aspect` must not replace
  `ResolveCinematicAspect()`.
- Legacy globals remain required for Dialogue invalidation and aspect
  restoration, but are no longer needed for the migrated ENTER target choice.

## Approved scope

- Add/use a pure target-selection helper for CinematicFovMode.
- Read one coherent `GameplayBaseline` copy in `TraceCinematicEnter`.
- Use only usable `nativeFov.value` for GameplayHorPlus.
- Preserve authored ENTER fallback when baseline is unavailable.
- Keep NativeHorPlus independent of baseline.
- Add target-selection and aspect-independence harness coverage.
- Add observational selection telemetry and one combined future runtime matrix.

## Explicit non-goals

- No deletion or migration of legacy globals.
- No Dialogue invalidation or aspect-restoration migration.
- No GameplayBaseline redesign, FOV/aspect math, cinematic policy, CameraState,
  ZOOM, ADS, resolver, hook, generation or unrelated cleanup.
- No runtime game launch, Git commit or release.

## Expected files or areas

- `src/cinematics/cinematic_fov.hpp/.cpp`
- `src/plugin/runtime.cpp`
- `tests/cinematics/cinematic_fov_harness.cpp` or the relevant existing
  harness location
- `build.cmd`, `test.cmd`
- `research/reports/GAMEPLAY_BASELINE_BATCH2B_ENTER_CUTOVER.md`
- `backlog/TASKLOG.md`

## Implementation batches

1. Define pure baseline selection and usable-baseline predicate.
2. Replace only ENTER GameplayHorPlus target selection with one baseline read;
   leave NativeHorPlus and authored fallback unchanged.
3. Add diagnostic selection telemetry outside the baseline mutex.
4. Add deterministic target-selection, fallback and cinematic-aspect
   independence coverage.

## Validation

- Relevant cinematic target-selection harness.
- Existing GameplayBaseline and all full `test.cmd` harnesses.
- `build.cmd`.
- `git diff --check`.
- Read-only Git review against this plan.
- No game launch.

## Risks and safe failure

- Invalid baseline must fall back to authored ENTER FOV, never legacy distributed
  state, zero/default values or `horPlusFov`.
- NativeHorPlus must ignore the baseline entirely.
- `GameplayBaseline.aspect` must never become effective cinematic aspect.
- If the cutover requires changing legacy responsibilities or transform math,
  stop and report scope conflict.

## Stop conditions and phase gates

- Stop after static/harness/build/diff/report and combined runtime matrix.
- Do not delete legacy state or begin another architecture batch.
- Runtime validation is a separate next gate and must not be launched here.

## Expected final Git review

Confirm only the narrow ENTER consumer, helper, tests, build wiring, report,
task log and plan paths changed for this batch; separate pre-existing dirty
worktree changes and list remaining runtime questions.
