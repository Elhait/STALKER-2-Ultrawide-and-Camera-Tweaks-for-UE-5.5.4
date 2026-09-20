# Dialogue Recovery Single Target Sample Task Plan

## Objective

Close Dialogue recovery immediately on the first valid native FOV sample
inside the established `nativeTarget ± 0.01` tolerance. Remove the additional
confirmation-sample requirement that can leave the lifecycle pending when the
native producer stops emitting callbacks at that sample.

## Established evidence

- Runtime 2.0.6 observed `89.9618 -> 89.9949`, with native target `90`.
- `abs(89.9949 - 90) = 0.0051`, already inside the accepted `0.01`
  tolerance.
- No further Dialogue callback occurred before the next ADS/ZOOM transition.
- The existing two-sample confirmation therefore left stale Dialogue owner
  state during ADS.

## Approved scope

- Change only `RecoveryRearm` target-convergence completion semantics.
- Keep source-change cancellation and invalid-target fail-closed behavior.
- Add focused harness coverage for target completion with no additional sample.
- Preserve Candidate hardening, policy snapshot, ZOOM, HorPlus, Cinematics and
  AspectRecalculation behavior.

## Explicit non-goals

- No timer or callback-absence heuristic.
- No `state28` invariant.
- No redesign of Dialogue ownership.
- No new runtime research or game launch in this batch.

## Validation

- Full `test.cmd` suite.
- Production build.
- Combined HorPlus FOV/ZOOM diagnostic build.
- `git diff --check` and read-only Git review.
- Runtime 2.0.6 validation remains pending.
