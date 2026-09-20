# V1.0.0 Cinematics Transactional Failure-Domain Task Plan

## Objective

Make `Cinematics` one transactional user-facing feature while preserving
separate aspect-store and FOV implementation modules.

## Established evidence and current state

- `[Cinematics].AspectRatio` is the single user-facing cinematic policy.
- Current initialization can expose aspect and FOV as independent statuses,
  allowing partial cinematic activation.
- For `Auto` and forced modes, both implementation components are required.
- `Native` requires no cinematic override hooks.
- Finding 2 (`Gameplay=false` and `CinematicExiting`) is explicitly out of
  scope.

## Approved scope

- Add a shared transactional Cinematics initialization boundary.
- Roll back already-installed cinematic state/hooks on component failure.
- Expose one user-facing Cinematics status for non-Native modes.
- Preserve separate aspect/FOV implementation files and successful behavior.
- Add bounded harness coverage for success, each failure order and Native
  bypass.

## Explicit non-goals

- Do not add `Cinematics.Enabled` or `Cinematics.Framing` settings.
- Do not change signatures, scanner behavior, Hor+ formula, hook callbacks,
  atomic gameplay handoff, Gameplay, Dialogue, defaults or config format.
- Do not fix Finding 2.
- Do not launch the game.

## Expected files or areas

- `src/plugin/runtime.cpp` and/or current Cinematics status/ownership types.
- Existing feature-status/lifecycle test infrastructure.
- `tests/` relevant harness.
- `test.cmd` only if required to include bounded coverage.
- `backlog/TASKLOG.md`.

## Validation

- Cinematics transactional harness: success, aspect failure, FOV failure
  after aspect success, and Native bypass.
- Existing harness suite through `test.cmd`.
- Production `build.cmd`.
- `git diff --check`.

## Risks and rollback or safe failure

The repair must leave no cinematic override hook or state installed after a
failed non-Native initialization. Native must remain a complete bypass.
Successful behavior must remain equivalent. If the implementation requires
changing scanner, resolver, callback or feature semantics, stop rather than
expand scope.

## Stop condition

Stop after validation and read-only Git review. Runtime validation is separate
and user-controlled.

## Final review

Compare changed paths with this plan and confirm Finding 2 and all unrelated
features remain untouched.
