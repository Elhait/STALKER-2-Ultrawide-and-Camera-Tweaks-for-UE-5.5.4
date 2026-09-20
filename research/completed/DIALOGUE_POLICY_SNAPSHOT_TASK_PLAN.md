# Task 6 — Dialogue Selected / Active Policy Snapshot

## Objective

Separate the user-selected Dialogue policy from the immutable policy snapshot
used by the currently confirmed Dialogue lifecycle.

## Established evidence and current state

- Task 4 added deterministic `RearmPending` recovery gating.
- Task 5 bounded Candidate lifetime with trajectory cancellation while
  preserving legitimate slow descent.
- The current transform path reads the global runtime Dialogue policy on every
  callback, so a hotkey change during Active/Exiting can mutate the current
  Dialogue behavior.
- ZOOM is observation-only; HorPlus, Cinematics and AspectRecalculation are
  independent.

## Approved scope

- Audit config/hotkey policy storage and every Dialogue transform policy read.
- Keep selected policy as the user/next-Dialogue value.
- Snapshot selected policy only at `Candidate -> Active`.
- Use active snapshot through Active, Exiting and RearmPending.
- Release active snapshot at completed lifecycle reset.
- Add transition-only selected/active policy telemetry.
- Add focused policy snapshot harness coverage.

## Explicit non-goals

- No Candidate detection or `RearmPending` changes.
- No Dialogue FOV formula changes.
- No positive Dialogue ownership detector or `IsInStaticDialog()` access.
- No ZOOM, ADS owner, HorPlus, Cinematics, AspectRecalculation or mode
  switching changes.
- No game launch, Git commit/release or stable ASI replacement.

## Expected files/areas

- `src/plugin/runtime.cpp` for policy state, hotkey selection and transform
  integration.
- `tests/dialogue/` and `test.cmd` for focused policy snapshot coverage.
- `backlog/TASKLOG.md` after implementation and review.

## Batches and validation

1. Audit current policy reads and implement selected/active snapshot state.
2. Add focused tests for Active/Exiting mutation, Candidate mutation, next
   Dialogue snapshot, Native/Disabled semantics and lifecycle release.
3. Run full `test.cmd`, combined diagnostic build, normal production compile
   and `git diff --check`.
4. Perform read-only Git review, archive this plan and update `TASKLOG.md`.

## Risks and safe failure

- If active policy is unavailable outside a confirmed lifecycle, preserve
  native/pass-through behavior.
- Preserve current config defaults and policy parsing.
- Preserve atomic/thread-safety of selected policy and synchronize active
  snapshot with the existing Dialogue mutex.

## Stop conditions and phase gates

- Stop after Task 6; do not begin Task 7 automatically.
- Runtime validation remains deferred to the combined regression matrix.
- Preserve unrelated dirty worktree changes.

## Expected final Git review

Confirm changed paths match this plan and separate static/harness evidence from
runtime validation status.
