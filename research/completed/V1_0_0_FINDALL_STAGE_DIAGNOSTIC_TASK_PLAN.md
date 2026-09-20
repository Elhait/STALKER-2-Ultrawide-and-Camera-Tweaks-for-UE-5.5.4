# FindAll Stage Diagnostic Task Plan

## Objective

Identify the second failure stage inside current `hooks::FindAll` after the
parser forward-progress repair.

## Established evidence and current state

The parser cursor repair passed the scanner harness, but the runtime FOV
resolver still fails before returning from `FindAll(CinematicEnter)`. The
resolver match log is not reached, so section enumeration, scan bounds,
result accumulation and other exception paths remain to be distinguished.

## Approved scope

- Add stage markers around parser completion, section enumeration, scanning,
  result insertion and return.
- Add bounded counters and local `std::bad_alloc`/exception diagnostics with
  rethrow.
- Preserve the existing FOV exception boundary while logging its reason.
- Build the diagnostic ASI for one user-controlled runtime run.

## Explicit non-goals

- No scanner bounds, arithmetic, accumulation, parser, pattern or resolver
  repair.
- No Cinematics failure-domain or policy change.
- No hook, FOV, gameplay or status behavior change.
- No tests or game launch by the agent.

## Expected files/areas

- `src/hooks/signature_scanner.cpp`: diagnostics only.
- `src/plugin/runtime.cpp`: exception reason logging only.

## Validation

- `build.cmd`.
- `git diff --check`.
- Read-only Git review of affected paths.

## Safe-failure behavior

All caught exceptions are rethrown after logging. Scan bounds, loop control,
result accumulation and resolver behavior remain unchanged.

## Stop condition

Stop after build and diff review. The user performs the runtime check and
returns the log before any second scanner repair is considered.
