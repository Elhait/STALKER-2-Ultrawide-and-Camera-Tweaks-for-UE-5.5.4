# Gameplay Baseline Propagation Task Plan

## Objective

Repair the production wiring that propagates the latest valid Gameplay-mode
native FOV observation into the retained baseline context consumed by the
`GameplayHorPlus` cinematic ENTER policy.

## Established evidence and current state

- Runtime telemetry recorded a valid Gameplay pair before cinematic ENTER:
  native `112.623`, HorPlus `143.132`, aspect `3.55556`.
- The same ENTER received `matchGameplayNativeBaseline=nan` and fell back to
  `NativeHorPlus`, producing `90 -> 126.87`.
- In `Gameplay.Mode=HorPlus`, `ReplayManualTransition` returns after
  `ApplyHorPlusGameplay` and therefore bypasses the legacy observation update
  in `ReplayManualTransitionOriginal`.
- The repair must retain the last valid Gameplay baseline while presentation
  changes to CinematicActive.

## Approved scope

- Update the existing production Gameplay HorPlus writer path to publish the
  valid native FOV/source/aspect observation used by the retained baseline.
- Add deterministic harness coverage for publication and retention across a
  cinematic ENTER boundary.
- Keep the existing `GameplayHorPlus` formula and NativeHorPlus fallback
  unchanged.

## Explicit non-goals

- No MatchGameplay formula changes.
- No Dialogue, ZOOM, Cinematic EXIT, AspectRecalculation, or resolver changes.
- No new global owner/state machine.
- No runtime game launch in this batch.
- No cleanup of unrelated pre-existing worktree changes.

## Expected files or areas

- `src/plugin/runtime.cpp`
- Relevant gameplay/cinematic harness files under `tests/`
- This plan, a bounded implementation report, and `backlog/TASKLOG.md`

## Batches and validation

1. Add retained baseline publication at the valid Gameplay writer boundary.
   Validate with the relevant harness.
2. Verify baseline remains available after ENTER-side state changes and is not
   overwritten by CinematicActive writer samples. Run `test.cmd`.
3. Build the production and diagnostic artifacts, run `git diff --check`, and
   perform a read-only Git scope review.

## Risks and rollback / safe failure

- Invalid source, aspect, or FOV must leave the retained baseline unchanged.
- Cinematic writer samples must never replace the Gameplay baseline.
- If publication fails, `GameplayHorPlus` must retain its existing fail-closed
  fallback to `NativeHorPlus`.
- Rollback is limited to the bounded changes in the listed source/test/report
  files.

## Stop conditions and phase gates

- Stop if the relevant harness cannot distinguish Gameplay publication from
  Cinematic observations.
- Stop before runtime if any test or build fails.
- Stop if the diff expands into unrelated camera subsystems.

## Expected final Git review

Confirm changed paths match this plan, identify pre-existing changes as such,
record validation limits, and archive this plan under `research/completed/`
only after implementation and static validation pass.
