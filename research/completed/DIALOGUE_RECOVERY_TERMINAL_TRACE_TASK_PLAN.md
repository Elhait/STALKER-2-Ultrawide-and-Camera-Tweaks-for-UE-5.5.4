# Task Plan — Dialogue Recovery Terminal Trace

## Objective

Extend the diagnostic-only Dialogue recovery observer far enough to capture
the terminal behavior of the native `RSI+0x28` state after the current
premature re-arm point.

## Established evidence and current state

- `RSI+0x2C=90` remained the native recovery target in the tested paths.
- `RSI+0x28` continuously evolved while current FOV approached that target.
- The prior 64-sample bound stopped before terminal `+0x28` behavior was seen.
- `+0x28 -> 0` is a candidate completion signal, not an accepted contract.

## Approved scope

- Rename diagnostic `runningFov` labels to neutral `state28`.
- Increase only the diagnostic sample bound from 64 to 256.
- Preserve post-rearm observation and all production behavior.

## Explicit non-goals

- No Dialogue state-machine or recovery predicate changes.
- No timer, epsilon, hardcoded endpoint or `+0x28` production use.
- No changes to Candidate, policy snapshot, HorPlus, ZOOM or Cinematics.
- No game launch or release packaging.

## Expected files/areas

- `src/plugin/runtime.cpp`
- `backlog/TASKLOG.md`

## Validation and stop conditions

- Run the full harness suite, diagnostic compile and `git diff --check`.
- Stop after the diagnostic ASI is built; runtime terminal semantics remain
  unestablished until the next user test.
