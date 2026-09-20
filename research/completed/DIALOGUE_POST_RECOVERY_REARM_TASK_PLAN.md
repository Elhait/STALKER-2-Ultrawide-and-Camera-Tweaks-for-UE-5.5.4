# Task 4 — Dialogue Post-Recovery Re-arm Repair

## Objective

Prevent the confirmed immediate post-Dialogue recovery-tail re-arm while
preserving the ability to detect a later legitimate Dialogue.

## Established evidence and current state

- Task 1–3 are complete and validated by source/build/harness checks.
- The Dialogue hook is a generic FOV/blend boundary; no positive ownership
  discriminator is introduced by this task.
- Current production logic resets `Exiting` to `Inactive` as soon as the
  incoming FOV enters the recovery tolerance, although native recovery may
  still emit additional tail samples.
- ZOOM, Cinematics, HorPlus and AspectRecalculation are independent inputs and
  are not repair targets here.

## Approved scope

- Audit and minimally change the Dialogue recovery state machine.
- Add a deterministic re-arm-pending boundary based on native FOV convergence
  and stability already observable at the Dialogue hook.
- Add focused Dialogue harness coverage for the reproduced tail, later
  legitimate Dialogue, recovery noise, and independence from HorPlus/ZOOM.
- Keep logs state-transition-only.

## Explicit non-goals

- No runtime/game launch or ASI replacement.
- No generic Candidate timeout/reversal hardening (Task 5).
- No positive Dialogue ownership detector or target-70 fingerprint.
- No F10 selected/active policy repair (Task 6).
- No ZOOM, Cinematics, HorPlus, AspectRecalculation, mode-switching or hook
  ownership changes.
- No Git commit/release operation.

## Expected files/areas

- `src/dialogue/dialogue_state.hpp/.cpp` for the testable recovery state logic,
  if extraction remains minimal and preserves current behavior.
- `src/plugin/runtime.cpp` for integration and transition-only logging.
- `tests/dialogue/` and `test.cmd` for deterministic regression coverage.
- `backlog/TASKLOG.md` after implementation and review.

## Batches and validation

1. Confirm current predicates and implement the bounded re-arm boundary.
   Validate compilation and source diff against this plan.
2. Add harness cases: recovery tail, legitimate next Dialogue, noisy tail,
   HorPlus independence and ZOOM independence.
   Run the full `test.cmd`.
3. Build the neutral combined diagnostic candidate and a separate normal
   production compile. Run `git diff --check`.
4. Perform read-only Git review and record completed, remaining and deferred
   items in `backlog/TASKLOG.md`.

## Risks and safe failure

- If no deterministic convergence condition can be justified from current
  values, stop without a timer and record `NOT ESTABLISHED`.
- On invalid input, source change, policy change or non-gameplay state, retain
  existing native pass-through/reset behavior.
- If the harness exposes a regression in the later Dialogue path, revert only
  the Task 4 change before reporting.

## Stop conditions and phase gates

- Stop before Task 5 after this batch.
- Do not claim runtime validation; it is deferred to the combined matrix.
- Do not modify unrelated dirty files or stable release output.

## Expected final Git review

Confirm only planned current source, test/build-script, plan and task-log paths
changed; identify pre-existing unrelated worktree changes separately.
