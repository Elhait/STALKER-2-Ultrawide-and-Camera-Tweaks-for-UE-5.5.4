# Cinematic FOV Resolver Diagnostic Task Plan

## Objective

Identify the exact validation stage that rejects the cinematic FOV resolver in
the current production build.

## Established evidence and current state

The runtime log reports cinematic aspect availability but no cinematic FOV
availability. The existing resolver returns only a general failure message for
some validation paths, so the rejection stage is not yet observable.

## Approved scope

- Add diagnostic logging around the existing cinematic ENTER/EXIT resolver.
- Report match counts, legacy/indexed EXIT fallback, decode/operand and
  structural validation results, callsite validation, call-target validation
  and ENTER/EXIT target relationship.
- Build the production ASI for one user-controlled runtime check.

## Explicit non-goals

- No signature changes or scan-range changes.
- No validation-condition changes.
- No resolver fallback or selection changes.
- No FOV formula, hook ordering, status or failure-boundary changes.
- No gameplay, dialogue, handoff or lifecycle changes.
- No game launch by the agent.

## Expected files/areas

- `src/plugin/runtime.cpp`: diagnostic logging only.

## Validation

- `build.cmd`.
- Read-only diff review and `git diff --check`.

## Risks and safe-failure behavior

Diagnostic code must not alter resolver return values, selected candidates,
validation predicates or installed hooks. If the build fails, retain the
pre-change source behavior and report the failure.

## Stop condition

Stop after the production build and factual diff review. The user performs the
single runtime check and supplies the resulting log before any further change.
