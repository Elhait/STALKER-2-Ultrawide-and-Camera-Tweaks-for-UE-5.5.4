# Task Plan — Early Callback Crash Diagnostic

## Objective

Localize the `Gameplay.Enabled=false` startup crash by determining whether it
occurs inside the first mod-owned cinematic aspect or dialogue callbacks.

## Established evidence and current state

- `Gameplay.Enabled=true` startup and cinematic path pass runtime validation.
- `Gameplay.Enabled=false`, `Cinematics=Auto`, `Dialogue=Reduced` reaches a
  completed initialization summary and then crashes with an access violation
  writing address `0x8`.
- No cinematic ENTER or EXIT callback log appears before the crash.
- Finding 2's production change is inside `TraceCinematicExit()` and is not
  reachable before cinematic EXIT.
- Static audit did not establish a crash-producing write path.

## Approved scope

- Add entry/normal-return breadcrumbs to `ApplyCinematicAspectStore()`.
- Add entry/return breadcrumbs to `TraceDialogueBoundary()` for its existing
  early-return branches.
- Use the existing logging primitive only.

## Explicit non-goals

- No production repair or safety guard.
- No callback suppression, exception handling, timing change or hook change.
- No changes to Findings 1–6, scanner/parser, signatures, policies or state
  semantics.
- No game launch or runtime test in this batch.

## Expected files or areas

- `src/plugin/runtime.cpp`

## Implementation batch

1. Add bounded callback entry/return markers without changing control flow.

## Validation

- `build.cmd`
- `git diff --check`
- Read-only diff/status review against this plan

## Risks and safe-failure behavior

- Diagnostic logging may affect timing or logging volume; no production repair
  is inferred from this build.
- If the build fails, stop without expanding the diagnostic scope.

## Stop condition and phase gate

Stop after the diagnostic build and report. Runtime execution is performed by
the user and is required before any callback-specific repair is considered.

## Final Git review

Confirm only the planned diagnostic source change and this task plan are
present, with no unrelated production behavior changes.
