# Task 5 — Dialogue Candidate Lifetime / Cancellation Hardening

## Objective

Make the Dialogue `Candidate` state short-lived and cancellable so unrelated
FOV activity cannot leave a stale hypothesis armed until a later descent.

## Established evidence and current state

- Task 4 added and validated `RearmPending`; it remains the authoritative
  post-recovery gate.
- The Dialogue hook is a generic FOV/blend boundary and has no established
  positive game-owned Dialogue discriminator.
- Current Candidate capture accepts the first valid gameplay FOV sample and
  has no bounded cancellation for stabilization or reversal.
- ZOOM is observation-only after Task 3; `[RSI+0x2C]` is a generic native
  blend target/end FOV value, not a Dialogue flag.

## Approved scope

- Audit the current Candidate transitions after Task 4.
- Add only deterministic direction/stabilization/invalid-sample cancellation
  that can be justified by current hook values.
- Preserve slow and small-step legitimate descent detection.
- Add focused Candidate and regression harness coverage.
- Keep transition-only diagnostic logging.

## Explicit non-goals

- No wall-clock timer or arbitrary callback-count timeout without source-level
  justification.
- No target-70/magic Dialogue fingerprint and no direct APC state integration.
- No Task 4 redesign or removal of `RearmPending`.
- No selected/active policy snapshot (Task 6).
- No ZOOM classifier input, ADS owner, HorPlus, Cinematics,
  AspectRecalculation or mode-switching changes.
- No game launch, Git commit/release or stable ASI replacement.

## Expected files/areas

- `src/plugin/runtime.cpp` for Candidate state integration and logs.
- `src/dialogue/dialogue_state.hpp/.cpp` for small testable Candidate helper
  logic if extraction remains minimal.
- `tests/dialogue/` and `test.cmd` for focused cases.
- `backlog/TASKLOG.md` after implementation and review.

## Batches and validation

1. Audit current Candidate predicates and choose the smallest deterministic
   cancellation contract.
2. Implement the bounded Candidate state and transition-only reasons.
3. Add harness cases for legitimate fast/slow descent, stale Candidate,
   reversal, stabilization, invalid samples and Task 4 interaction.
4. Run full `test.cmd`, combined diagnostic build, normal production compile,
   and `git diff --check`.
5. Perform read-only Git review, archive this plan and update `TASKLOG.md`.

## Risks and safe failure

- If a bounded predicate would reject legitimate slow Dialogue trajectories,
  stop and record `NOT ESTABLISHED` rather than adding aggressive thresholds.
- Invalid or unavailable values fail closed and must not confirm Candidate.
- Preserve native pass-through and existing Active/Exiting behavior on any
  ambiguous condition.

## Stop conditions and phase gates

- Stop after Task 5; do not begin Task 6 automatically.
- Runtime validation remains deferred to the combined matrix.
- Preserve all unrelated existing dirty worktree changes.

## Expected final Git review

Confirm changed paths match this plan, identify intentionally untouched
subsystems, and separate static/harness evidence from runtime status.
