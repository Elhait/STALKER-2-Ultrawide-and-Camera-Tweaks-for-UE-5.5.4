# v1.0 Gameplay Mode Hotkey — Task Plan

## Objective

Add a configurable runtime hotkey that cycles the existing gameplay modes
`AspectRecalculation` and `HorPlus`, and document the setting in the generated
INI template and README.

## Established evidence and current state

- `GameplayMode` already has exactly the two supported values.
- Runtime dispatch already reads the atomic runtime gameplay mode.
- Existing hotkey infrastructure cycles and persists runtime policies.
- `AspectRecalculation` remains the default validated mode.

## Approved scope

- Add one `GameplayCycle` hotkey setting, defaulting to `F11`.
- Parse, name, cycle and persist the setting through existing config/hotkey
  infrastructure.
- Make the hotkey update the existing runtime mode dispatch.
- Update generated INI descriptions, README configuration text and config tests.

## Explicit non-goals

- No changes to either gameplay correction algorithm.
- No changes to cinematic, dialogue, coordinator, scanner or hook ownership.
- No new gameplay mode, setting or hook.
- No game/runtime launch.

## Files or areas expected to be touched

- `src/config/feature_config.hpp/.cpp`
- `src/config/config_repository.cpp`
- `src/config/config_template.cpp`
- `src/plugin/runtime.cpp`
- `tests/config/gameplay_mode_harness.cpp`
- `tests/config/config_persistence_harness.cpp` if required
- `README.md`
- `research/ue4ss/STALKER2CameraTweaks/STALKER2CameraTweaks.ini` if it is the
  maintained example template
- `build.cmd`/`test.cmd` only if required by source additions

## Implementation batches

### Batch 1 — Config and runtime hotkey

- Add the default key and `NextGameplayMode` helper.
- Parse/persist `GameplayCycle` and update the runtime atomic mode on key edge.
- Preserve mode dispatch and `Enabled=false` semantics.

Validation: targeted config harness and source review.

### Batch 2 — Documentation and full validation

- Update generated template comments and README.
- Run `build.cmd`, `test.cmd` and `git diff --check`.

## Risks and rollback / safe-failure behavior

- Unknown or invalid key values retain the existing default `F11`.
- The hotkey is inactive when `[Hotkeys] Enabled=false`.
- The hotkey changes only the existing runtime mode selector; no hook is
  installed or removed by the hotkey.
- Rollback is limited to reverting this bounded patch after Git review.

## Stop conditions and phase gates

- Stop if runtime mode switching requires changes to correction algorithms or
  cinematic lifecycle.
- Stop after build/test/static review; runtime validation remains separate.

## Expected final Git review

- Confirm only the planned configuration, runtime hotkey, documentation and
  harness paths changed; preserve unrelated dirty worktree changes.
- Report automated validation separately from runtime validation.
