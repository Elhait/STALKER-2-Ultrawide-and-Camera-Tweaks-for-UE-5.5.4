# v1.0.0 HorPlus Cinematic EXIT Handoff — Implementation Task Plan

## Objective

Repair the confirmed HorPlus post-cinematic state transition without
rewriting the Cinematics subsystem: HorPlus must not arm or wait for the
AspectRecalculation recovery handoff at cinematic EXIT.

## Established evidence and current state

- Runtime log confirms HorPlus cinematic ENTER applies the cinematic FOV
  transform (`90 -> 126.87`).
- Runtime log confirms the gameplay writer hook is installed.
- Current `TraceCinematicExit()` still selects the old
  `CinematicExiting`/atomic-handoff path based only on Gameplay availability.
- HorPlus writer dispatch returns before the old recovery path, so the
  coordinator can remain in `CinematicExiting` and gameplay HorPlus does not
  resume after EXIT.
- `AspectRecalculation` is validated and must remain unchanged.

## Approved scope

- Add one mode-aware EXIT transition for `Gameplay.Mode=HorPlus`.
- HorPlus EXIT must bypass AspectRecalculation handoff arming and return the
  coordinator to `Gameplay` through the existing validated lifecycle boundary.
- Preserve existing `AspectRecalculation` EXIT behavior byte-for-byte aside
  from the required dispatch condition.
- Add bounded pure transition/harness coverage for both modes.

## Explicit non-goals

- Do not rewrite cinematic aspect/FOV hooks or formulas.
- Do not change the HorPlus transformation or gameplay writer hook.
- Do not change Dialogue behavior, scanner logic, config defaults, or flags.
- Do not add a new hook, timer, polling, cache, or reverse-engineering
  anchor.
- Do not redesign the broader cinematic ownership model.

## Expected files or areas

- `src/gameplay/gameplay_state.hpp/.cpp`
- `src/plugin/runtime.cpp`
- `tests/cinematics/coordinator_recovery_harness.cpp`
- `test.cmd`

## Implementation and validation batches

1. Extend the existing pure cinematic EXIT transition decision with the
   gameplay mode; preserve the old branch for `AspectRecalculation` and make
   HorPlus return `Gameplay` without arming the old handoff.
2. Route `TraceCinematicExit()` through that decision and add harness cases.
3. Run `test.cmd`, `build.cmd`, `git diff --check`, and review the bounded Git
   diff. Do not launch the game.

## Risks and safe failure

- Main risk is altering the validated AspectRecalculation path. The harness
  must prove its existing available/unavailable decisions remain unchanged.
- If the mode-aware transition cannot be isolated without changing the old
  path, stop without modifying runtime behavior.
- Runtime behavior remains unconfirmed until the user runs the ASI.

## Stop conditions and phase gates

- Stop after static/build/harness validation.
- Do not expand into cinematic research or additional runtime instrumentation.
- Report cinematic runtime integration as not yet validated.

## Expected final Git review

- Confirm only the mode-aware EXIT dispatch and its harness coverage changed.
- Confirm `AspectRecalculation` implementation, cinematic hooks and HorPlus
  math remain unchanged.
- Record completed, remaining, deferred and not-runtime-validated items.
