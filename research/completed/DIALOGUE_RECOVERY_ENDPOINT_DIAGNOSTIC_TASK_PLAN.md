# Task Plan — Dialogue Recovery Endpoint Diagnostic

## Objective

Collect read-only, change-driven telemetry for the native Dialogue recovery
blend so the current `baselineG` re-arm predicate can be compared with the
native context's running, target and paired FOV values.

## Established evidence and current state

- Task 4 has a runtime-confirmed premature re-arm case.
- `baselineG=89.9734` was treated as converged, while native samples continued
  monotonically toward approximately `89.9985`.
- Static audit established `RSI+0x2C` as a native FOV blend target/end value;
  `+0x28` is the running value and `+0x30` is a paired blend value.
- No recovery completion repair is established.

## Approved scope

- Add a compile-gated diagnostic trace in the existing Dialogue boundary.
- Safely read `RSI+0x28`, `RSI+0x2C` and `RSI+0x30`.
- Log only changes during `Exiting`/`RearmPending` and a bounded observation
  window after the existing re-arm.
- Record current FOV, baseline, previous sample, target/running/paired values,
  direction, deltas and whether the current predicate would re-arm.
- Preserve all production behavior and existing Task 4–6 semantics.

## Explicit non-goals

- No recovery predicate or Dialogue classifier change.
- No timer, hardcoded FOV endpoint or epsilon adjustment.
- No change to Candidate, policy snapshot, HorPlus, ZOOM or Cinematics.
- No game launch, ASI replacement or release packaging.

## Expected files/areas

- `src/plugin/runtime.cpp`
- `test.cmd` only if a focused harness is required.
- `backlog/TASKLOG.md` after static/build validation.

## Batches and validation

1. Add compile-gated SafeRead/change-driven recovery telemetry and bounded
   post-rearm observation.
2. Run full harness suite, production compile, diagnostic compile and
   `git diff --check`.
3. Perform read-only Git review and archive this plan.

## Risks and safe failure

- Any unreadable context field is logged as unavailable and never affects the
  production state machine.
- The diagnostic observation stops on source change, invalid reads or its
  bounded sample limit.
- Existing production behavior is byte-for-byte unchanged outside the
  compile-gated instrumentation.

## Stop conditions and phase gates

- Stop after build/harness validation; do not choose a completion predicate.
- Runtime validation is deferred to the combined regression session.

## Expected final Git review

Confirm only diagnostic source/plan/log paths changed for this batch, preserve
all unrelated dirty worktree changes, and separate build evidence from runtime
evidence.
