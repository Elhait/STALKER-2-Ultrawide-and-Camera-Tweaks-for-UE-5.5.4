# Unified Cinematic FOV Baseline Task Plan

## Objective

Unify NativeHorPlus and GameplayHorPlus cinematic FOV execution into one
common tangent-space transfer plus HorPlus pipeline. The selected mode may
choose only the target baseline/reference value.

## Established evidence and current state

- NativeHorPlus runtime behavior is validated as authored `90 -> 126.87` at
  32:9.
- GameplayHorPlus runtime behavior is validated with retained Gameplay
  baseline `112.623 -> 143.132`.
- Current source has separate `TryTransformEnterFov` and
  `TryTransformMatchGameplay` implementations containing overlapping
  validation and transformation logic.
- NativeHorPlus is the identity-baseline case of the GameplayHorPlus
  transfer: target baseline equals cinematic reference.

## Approved scope

- Add one common cinematic transform function accepting current FOV, reference
  FOV, target baseline FOV, effective aspect and native aspect.
- Make runtime choose only the target baseline and call that common function.
- Preserve compatibility wrappers where existing harness/API calls need them;
  wrappers must delegate to the common implementation.
- Add deterministic identity, GameplayHorPlus and invalid-baseline coverage.

## Explicit non-goals

- No change to the public default `NativeHorPlus` behavior.
- No change to retained baseline propagation.
- No changes to writer/cached provenance, Dialogue, ZOOM, AspectRecalculation,
  resolver logic or CameraState.
- No new camera path or state machine.
- No runtime game launch in this batch.

## Expected files or areas

- `src/cinematics/cinematic_fov.hpp`
- `src/cinematics/cinematic_fov.cpp`
- `src/plugin/runtime.cpp`
- `tests/gameplay/horplus_gameplay_harness.cpp`
- implementation report and `backlog/TASKLOG.md`

## Batches and validation

1. Implement the common transform and delegate existing wrappers to it.
2. Replace the ENTER mode split with baseline selection followed by one common
   transform call.
3. Extend deterministic harness coverage for native identity, gameplay
   baseline transfer and invalid-context fallback; run full `test.cmd`.
4. Run `build.cmd`, `git diff --check`, and a read-only Git scope review.

## Risks and rollback / safe failure

- NativeHorPlus must remain numerically equivalent within the existing float
  tolerance.
- Invalid Gameplay baseline must select the cinematic reference baseline,
  preserving NativeHorPlus fallback.
- Invalid input/reference/aspect must fail without writing a transformed value.
- Rollback is limited to the listed source, test, report and plan files.

## Stop conditions and phase gates

- Stop if identity or fallback harness coverage fails.
- Stop before runtime if tests/build fail.
- Stop if the diff changes any unrelated camera subsystem.

## Expected final Git review

Confirm only the approved areas changed, distinguish pre-existing worktree
changes, record validation and runtime limits, then archive this plan under
`research/completed/`.
