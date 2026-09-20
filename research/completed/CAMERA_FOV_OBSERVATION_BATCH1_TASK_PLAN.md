# Camera FOV Observation Architecture — Batch 1 Task Plan

## Objective

Introduce a read-only, provenance-aware Camera FOV observation store for the
established `CameraWriter` and `CinematicEnter` boundaries. The store must make
native/transformed values and their validity explicit without changing any
production camera decisions or outputs.

## Established evidence and current state

- Gameplay HorPlus and cinematic FOV paths are runtime-validated.
- Camera writer and cinematic-enter paths are distinct observation boundaries.
- Cached cinematic ENTER values may already be transformed and must not be
  presented as native input.
- Existing production caches and diagnostics currently carry factual FOV data
  in several places.
- AspectRecalculation is outside this batch.

## Approved scope

- Add one canonical observation schema with component validity, FOV space and
  provenance.
- Maintain isolated latest observations for `CameraWriter` and
  `CinematicEnter` with coherent mutex-protected publication.
- Use globally monotonic publication sequence numbers for successful commits.
- Publish observations from the two established paths only.
- Feed factual diagnostic fields from committed observations where available.
- Add deterministic harness coverage for the required invariants.
- Retain all existing production caches and keep the new store read-only to
  production behavior.

## Explicit non-goals

- No production FOV, aspect, dialogue, cinematic, gameplay-mode or recovery
  behavior change.
- No global latest-any observation API.
- No generation field in Batch 1.
- No migration of CameraStateSnapshot semantic ownership.
- No AspectRecalculation implementation or coverage requirement.
- No new hook, resolver, runtime game launch or Ghidra analysis.

## Expected files and areas

- `src/camera/fov_observation.hpp/.cpp`
- `src/plugin/runtime.cpp`
- `tests/camera/fov_observation_harness.cpp`
- `build.cmd`, `test.cmd`
- `research/reports/CAMERA_FOV_OBSERVATION_BATCH1_IMPLEMENTATION.md`
- `backlog/TASKLOG.md` after validation and Git review

## Batches and validation

1. Schema/store: compile and deterministic store harness.
2. Runtime publication wiring: source review; preserve existing caches and
   production branches.
3. Diagnostic migration: compile and inspect factual field provenance.
4. Full scoped validation: relevant harnesses, `test.cmd`, `build.cmd`,
   `git diff --check`, then read-only Git review against this plan.

## Risks and safe failure

- Invalid or unavailable samples remain invalid and are never synthesized.
- Cached transformed input remains transformed and cannot populate native FOV.
- Publication failure is observational only and cannot alter the existing
  writer or cinematic output.
- If compilation or harness validation fails, stop without runtime testing and
  report the exact failure.

## Stop conditions and phase gates

- Stop if the implementation requires changing production decision logic,
  adding a third boundary, or changing an accepted FOV contract.
- Stop after build, harness, diff review and report; do not launch the game.

## Expected final Git review

Confirm only the approved source, harness, build/test wiring and report/plan
paths changed for this batch, while explicitly separating pre-existing dirty
worktree changes from this batch's diff.
