# Cinematic FOV Hotkey Task Plan

## Objective

Add a configurable F12 hotkey that cycles `Cinematics.FovMode` for the next
cinematic without changing the currently active cinematic.

## Established evidence and current state

- Existing F9/F10/F11 hotkeys use configurable INI keys and persist selected
  values for future transitions.
- Cinematic FOV modes are `NativeHorPlus` and `GameplayHorPlus`.
- ENTER reads a runtime-selected mode, so the new hotkey can reuse the same
  decision boundary.

## Approved scope

- Add `CinematicFovCycle=F12` to config parsing/template/persistence.
- Add the mode-cycle helper and runtime hotkey edge handling.
- Use an atomic runtime FovMode selection and log/persist the selected mode.
- Add deterministic config/cycle coverage.

## Explicit non-goals

- No change to FOV math, baseline propagation or cinematic hooks.
- No change to active cinematic state; F12 affects the next cinematic only.
- No changes to Dialogue, ZOOM, Gameplay mode or resolver behavior.
- No game launch in this batch.

## Expected files or areas

- `src/config/feature_config.hpp/.cpp`
- `src/config/config_repository.cpp`
- `src/config/config_template.cpp`
- `src/plugin/runtime.cpp`
- relevant config harness and report/task log

## Validation

- Relevant config/cycle harness and full `test.cmd`.
- `build.cmd` and `git diff --check`.
- Read-only Git scope review.

## Risks and safe failure

- Invalid hotkey values retain the default F12.
- F12 edge handling must not mutate the active cinematic.
- Persistence failure must leave the selected runtime mode active for the next
  cinematic and report the failure.

## Stop conditions

- Stop if config synchronization or cycle tests fail.
- Stop before runtime if test/build/diff validation fails.
- Stop if the diff expands beyond hotkey/config/runtime selection code.
