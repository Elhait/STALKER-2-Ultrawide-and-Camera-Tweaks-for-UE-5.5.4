# Scanner bad_alloc Diagnostic Task Plan

## Objective

Identify whether the `CinematicEnter` scanner failure originates in pattern
parsing, scan progress, result accumulation or another allocation path.

## Established evidence and current state

The current runtime reaches `FindAll(g_executable, CinematicEnter)`, spends
approximately 32 seconds in that call and then reports `std::bad_alloc`.
`CinematicEnter` is unchanged from the pre-split reference. The exact
allocation site is not yet established.

## Approved scope

- Add diagnostic markers and counters to `PatternBytes`/`PatternScanAll`.
- Record parser completion, section bounds, scan progress and match milestones.
- Catch `std::bad_alloc` locally at pattern construction and result insertion,
  log the stage, then rethrow unchanged.
- Build the diagnostic ASI for one user-controlled runtime run.

## Explicit non-goals

- No parser, pattern, scanner-loop or match-limit changes.
- No production guards or allocation policy changes.
- No cinematic initialization or failure-domain repair.
- No changes to resolver validation, hooks or feature behavior.
- No tests or game launch by the agent.

## Expected files/areas

- `src/hooks/signature_scanner.cpp`: diagnostic instrumentation only.

## Validation

- `build.cmd`.
- `git diff --check`.
- Read-only Git review of affected paths.

## Safe-failure behavior

The existing exception is rethrown after diagnostic logging. Scanner control
flow and all return/exception behavior remain unchanged.

## Stop condition

Stop after build and diff review. The user performs the runtime check and
returns the scanner diagnostic log before any repair is considered.
