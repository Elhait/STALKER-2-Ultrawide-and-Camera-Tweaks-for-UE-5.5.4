# Dialogue Recovery Completion Repair Task Plan

## Objective

Replace the Dialogue recovery re-arm predicate that treats the captured
`baselineG` as the recovery endpoint. Use the validated native recovery target
at `RSI+0x2C`, with fail-closed handling for an unreadable/invalid target and
source-change cancellation.

## Established evidence and current state

- `baselineG` is a Dialogue lifecycle baseline, not a guaranteed native
  recovery endpoint.
- Static Steam 2.0.5 analysis identifies `RSI+0x2C` as the native target/end
  FOV for the observed blend context.
- Runtime traces on 2.0.5 and 2.0.6 show current FOV continuing toward the
  native target after it is already close to `baselineG`.
- `RSI+0x28` is an evolving native blend-state value; `RSI+0x28 == 0` is not
  an established invariant and will not be required.
- The current implementation uses baseline proximity plus local stability,
  which caused both premature re-arm and failure to release after the native
  target was reached.

## Approved scope

- Update `dialogue::RecoveryRearm` to track source and native target.
- Complete only after stable convergence to the native target.
- Cancel/release the pending recovery observation when the native source
  changes.
- Keep invalid/unreadable target fail-closed: do not invent an endpoint or
  release the active Dialogue policy.
- Update the focused recovery harness and the existing candidate harness for
  the new API.
- Update the Dialogue boundary call site to safely read `RSI+0x2C`.

## Explicit non-goals

- No changes to Candidate hardening (Task 5).
- No changes to selected/active policy snapshot semantics (Task 6).
- No changes to ZOOM, HorPlus, Cinematics, AspectRecalculation or FOV math.
- No hard-coded 90-degree endpoint.
- No timer-based completion.
- No Ghidra analysis of patch 2.0.6.
- No game launch or injected runtime validation in this batch.

## Expected files or areas

- `src/dialogue/dialogue_state.hpp`
- `src/dialogue/dialogue_state.cpp`
- `src/plugin/runtime.cpp`
- `tests/dialogue/recovery_rearm_harness.cpp`
- `tests/dialogue/candidate_hardening_harness.cpp`

## Implementation batches

1. Change the `RecoveryRearm` state/API to accept source and native target,
   return an explicit pending/complete/cancel decision, and preserve pending
   state when target data is invalid.
2. Read and validate `RSI+0x2C` at the Dialogue recovery boundary; keep the
   observer pending when the target is unavailable and handle source changes.
3. Update focused harness coverage for premature baseline proximity, target
   convergence, invalid target fail-closed behavior, source cancellation,
   active-policy release conditions, and next-lifecycle readiness.

## Validation

- Build and run all existing harnesses through `test.cmd`.
- Build the production ASI with the normal `build.cmd` path.
- Run `git diff --check`.
- Perform read-only `git status`, diff summary, and relevant diff review.
- Do not launch the game; runtime validation remains pending.

## Risks and rollback / safe-failure behavior

- A target read can fail or become non-finite. The recovery observer must stay
  pending and must not release the active policy.
- A source identity change invalidates the old recovery trajectory. The
  observer cancels and resets the Dialogue lifecycle so a new source can be
  classified normally.
- If build or harness validation fails, stop with the plan and source changes
  unpromoted; do not alter unrelated tasks or runtime artifacts.

## Stop conditions and phase gates

- Stop if the native target cannot be read safely at the existing hook.
- Stop if the change requires modifying unrelated lifecycle owners or FOV
  transformations.
- Stop after build/harness and static review. No runtime test in this batch.

## Expected final Git review

Confirm that only the approved source, harness, plan, and task-log paths are
changed; record completed, remaining, deferred, blocked, and not-runtime-
validated status before archiving this plan.
