# V1.0.0 Coordinator Recovery Without Gameplay Task Plan

## Objective

Terminate `CinematicExiting` safely when the Gameplay writer hook is
unavailable, without adding a disabled-gameplay observer or changing the
existing Gameplay-owned atomic handoff.

## Established evidence and current state

- `TraceCinematicExit()` enters `CinematicExiting`.
- When `g_gameplayAvailable` is true, the existing gameplay writer callback
  confirms native recovery and returns the coordinator to `Gameplay`.
- When `g_gameplayAvailable` is false, no callback can arrive and dialogue
  remains permanently gated by `CinematicExiting`.
- The validated Design D invariant forbids a continuous gameplay observer
  when Gameplay is disabled.

## Approved scope

- Use the existing cinematic EXIT boundary as the terminal transition when
  Gameplay is unavailable.
- Preserve `CinematicExiting` and the existing atomic handoff when Gameplay is
  available.
- Reset the existing dialogue runtime state on the immediate transition.
- Add bounded coordinator transition coverage for available, disabled and
  failed Gameplay states.

## Explicit non-goals

- No gameplay-disabled observer, timer, worker, polling or new hook.
- No dialogue-driven recovery or guessed delay.
- Do not change Finding 1, scanner/parser, cinematic calculations, signatures,
  atomic handoff semantics, or Findings 3–6.
- Do not launch the game.

## Expected files or areas

- `src/gameplay/gameplay_state.hpp`
- `src/gameplay/gameplay_state.cpp`
- `src/plugin/runtime.cpp`
- `tests/cinematics/coordinator_recovery_harness.cpp`
- `test.cmd`
- `backlog/TASKLOG.md`

## Validation

- Coordinator recovery harness for available, disabled and failed Gameplay
  paths, including no early completion when available.
- Existing harness suite through `test.cmd`.
- Production `build.cmd`.
- `git diff --check`.

## Risks and safe failure

Only the unavailable-Gameplay branch may complete at EXIT. The available branch
must continue waiting for the existing writer-confirmed recovery and atomic
handoff. If validation shows a change to that path, stop and revert the
bounded repair before runtime deployment.

## Stop condition

Stop after validation and read-only Git review. Combined runtime validation is
user-controlled and separate.

## Final review

Confirm no observer or new hook was introduced and Finding 1 remains untouched
apart from existing integration state.
