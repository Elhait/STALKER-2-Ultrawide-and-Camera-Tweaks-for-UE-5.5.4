# Hotkey Order — Task Plan

## Objective

Change the default runtime hotkey order to F9 Gameplay, F10 Cinematic aspect,
F11 Cinematic FOV and F12 Dialogue, with matching INI descriptions and user
documentation.

## Established evidence and current state

The current defaults are F9 Cinematic aspect, F10 Dialogue, F11 Gameplay and
F12 Cinematic FOV. Runtime dispatch already uses configurable key fields, so
the bounded change is limited to defaults, generated configuration text,
startup wording and documentation.

## Approved scope

- `src/config/feature_config.hpp`
- `src/config/config_repository.cpp`
- `src/config/config_template.cpp`
- `src/plugin/runtime.cpp` startup hotkey description
- `README.md`
- `NEXUS_DESCRIPTION.md`

## Non-goals

- No changes to hotkey dispatch behavior beyond new default bindings.
- No camera, FOV, Dialogue, Cinematic or Gameplay algorithm changes.
- No runtime launch, release packaging or unrelated cleanup.

## Validation

- Relevant deterministic tests through `test.cmd`.
- Production and diagnostic builds.
- `git diff --check`.

## Risks and safe failure

Explicit user-configured hotkeys remain authoritative. If a key is absent or
invalid, the new compiled default is used. Existing mode logic is unchanged.

## Stop condition

Stop after source/documentation synchronization and validation. Archive this
plan after completion; do not create a task-log entry for this bounded change.
