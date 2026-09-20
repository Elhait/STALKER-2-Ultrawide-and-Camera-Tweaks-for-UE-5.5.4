# GameplayBaseline Projection Architecture — Batch 2A Task Plan

## Objective

Add a coherent retained `GameplayBaseline` semantic projection from eligible
committed `CameraWriter` observations, in parallel with the existing legacy
retained state. Establish deterministic equivalence without migrating any
production consumer.

## Established evidence and current state

- Batch 1 provides factual `CameraFovObservation` values with component
  validity, FOV space, provenance, source and publication sequence.
- The retained-state audit mapped the distributed legacy FOV/source/aspect
  fields and their separate responsibilities.
- `ApplyHorPlusGameplay` is the established point where Gameplay coordinator
  eligibility is known and the CameraWriter observation is committed.
- Cinematic ENTER still consumes legacy retained fields and must remain so in
  this batch.

## Approved scope

- Add a coherent retained `GameplayBaseline` snapshot and short-mutex storage.
- Project only eligible committed native-to-HorPlus `CameraWriter` samples from
  the established Gameplay path.
- Preserve the committed observation's native/result/aspect/source/sequence
  values without recomputation.
- Implement explicit Updated/Retained/Invalidated projection semantics.
- Invalidate the new baseline at the existing HorPlus to AspectRecalculation
  transition point.
- Add observational diagnostics and deterministic equivalence harness coverage.
- Keep all legacy fields, ordering, readers and responsibilities unchanged.

## Explicit non-goals

- No production consumer migration or Cinematic ENTER read from the new state.
- No deletion or repurposing of legacy globals.
- No Dialogue invalidation, aspect-cache, CameraState, FOV math, resolver,
  hook, ADS, ZOOM, generation or stable-endpoint changes.
- No runtime game launch, Git commit or release packaging.

## Expected files or areas

- `src/camera/gameplay_baseline.hpp/.cpp`
- `src/plugin/runtime.cpp`
- `tests/camera/gameplay_baseline_harness.cpp`
- `build.cmd`, `test.cmd`
- `research/reports/GAMEPLAY_BASELINE_BATCH2A_IMPLEMENTATION.md`
- `backlog/TASKLOG.md`

## Implementation batches

1. Define coherent snapshot/storage and projection result API.
2. Publish a committed CameraWriter observation, then project it only when
   Gameplay semantic eligibility is established; keep legacy writes untouched.
3. Add explicit invalidation at the existing mode transition and diagnostic
   equivalence output without making diagnostics a correctness dependency.
4. Add deterministic harness cases A–K from the approved task.

## Validation

- Relevant baseline harness.
- Full `test.cmd`.
- `build.cmd`.
- `git diff --check`.
- Read-only Git review against this plan.
- No game launch.

## Risks and safe failure

- Invalid, transformed, cached or non-HorPlus observations must retain the
  previous baseline and never create partial state.
- CinematicActive samples must not project because eligibility is checked at the
  established Gameplay projection point.
- If projection storage or harness compilation fails, stop before any runtime
  validation.
- Existing production behavior remains safe because all production consumers
  continue reading legacy state.

## Stop conditions and phase gates

- Stop if implementing the projection requires changing observation ownership,
  production consumer reads, legacy write ordering or accepted FOV math.
- Stop after implementation, harness, test/build, diff review and report.
- Batch 2B is a separate phase and must not begin automatically.

## Expected final Git review

Confirm the actual changed paths match this plan, distinguish this batch from
the pre-existing dirty worktree, and report projection/equivalence status,
remaining discrepancies and runtime requirements separately.
