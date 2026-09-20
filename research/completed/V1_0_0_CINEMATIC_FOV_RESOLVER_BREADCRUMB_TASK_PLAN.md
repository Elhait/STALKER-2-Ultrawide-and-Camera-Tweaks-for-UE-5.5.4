# Cinematic FOV Resolver Breadcrumb Task Plan

## Objective

Identify the last execution stage reached before the current cinematic FOV
resolver failure.

## Established evidence and current state

The diagnostic ASI was deployed and contains the previous resolver diagnostic
strings, but the runtime log contains none of them. The resolver failure stage
therefore remains unknown.

## Approved scope

- Add unconditional `CFOV_DIAG` breadcrumbs before and after each resolver
  scan, validation and target-validation stage.
- Add diagnostic markers for standard and unknown exception boundaries.
- Build the production ASI for one user-controlled runtime check.

## Explicit non-goals

- No scanner, signature, range, validation, fallback or exception-semantic
  changes.
- No FOV, hook, status, failure-boundary, gameplay or cinematic behavior
  changes.
- No tests or game launch by the agent.

## Expected files/areas

- `src/plugin/runtime.cpp`: breadcrumbs only.

## Validation

- `build.cmd`.
- Read-only diff review and `git diff --check`.

## Safe-failure behavior

All breadcrumbs must be observational. Existing return values, catch behavior,
hook setup and feature status updates remain unchanged.

## Stop condition

Stop after build and diff review. The user performs the runtime check and
returns the log.
