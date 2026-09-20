# Cinematics FOV Initialization Diagnostic Task Plan

## Objective

Identify which stage of the current cinematic FOV initialization boundary
causes `CinematicFOV` to become failed.

## Established evidence and current state

The production configuration exposes one `[Cinematics] AspectRatio` policy.
Aspect-store and FOV hooks are implementation components of that policy. The
current runtime can report an FOV failure, but the precise failure stage has
not been established by runtime evidence.

## Approved scope

- Add `CFOV_INIT` logging around the existing FOV initialization boundary.
- Log resolver result, ENTER hook creation, EXIT hook creation, success and
  both exception paths.
- Preserve the existing control flow and cleanup behavior.
- Build the diagnostic ASI for one user-controlled runtime run.

## Explicit non-goals

- No cinematic semantic or failure-domain repair yet.
- No resolver, signature, scan-range or validation changes.
- No hook-ordering, FOV-policy, aspect-policy or gameplay changes.
- No status-model changes.
- No tests or game launch by the agent.

## Expected files/areas

- `src/plugin/runtime.cpp`: diagnostic logging only.

## Validation

- `build.cmd`.
- `git diff --check`.
- Read-only Git review of the affected paths.

## Safe-failure behavior

All new statements are observational. Existing resolver results, hook
creation, exception handling, cleanup and status assignments remain unchanged.

## Stop condition

Stop after build and diff review. The user performs the runtime check and
returns the log before any semantic repair is considered.
