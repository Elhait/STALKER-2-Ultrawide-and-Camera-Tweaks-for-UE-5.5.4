# V1 Pre-Audit Baseline

```yaml
date: 2026-09-20
source_root: E:/Work/Slaker2 mods/01-Projects/STALKER-2-Ultrawide-Fix-for-UE-5.5.4
test_entrypoint: test.cmd
production_build_entrypoint: build.cmd
diagnostic_build_entrypoint: build-diagnostic.cmd
production_artifact: STALKER2CameraTweaks.asi
diagnostic_artifact: STALKER2CameraTweaksDiagnostic.asi

test_cmd: PASS
production_build: PASS
diagnostic_build: PASS
diff_check: PASS
runtime: NOT_PERFORMED

production_behavior_changed: NO
documentation_corrected: NO
cleanup_performed: NO
```

## Repository navigation

```text
src/                  C++ source and module directories
tests/                deterministic harness source
tests/fixtures/       small recorded runtime-evidence values
tests/regression/     runtime-evidence replay harness
research/reports/     accumulated research, runtime and implementation reports
research/completed/   completed task plans
research/audit/       this neutral pre-audit file
```

## Build and test files

- `build.cmd` invokes the production profile and writes
  `STALKER2CameraTweaks.asi`.
- `build-diagnostic.cmd` invokes `build.cmd` with the diagnostic profile and
  writes `STALKER2CameraTweaksDiagnostic.asi`.
- `test.cmd` compiles and runs the deterministic harness set, including the
  Runtime Evidence Replay harness.
- The diagnostic profile defines the diagnostic surfaces listed in
  `build.cmd`; the production profile does not pass those high-rate diagnostic
  compile definitions.

## Configuration files and names present in source

- `Gameplay.Mode`: `AspectRecalculation`, `HorPlus`.
- `Cinematics.AspectRatio`: `Auto`, `Native`, `16:9`, `21:9`, `32:9`.
- `Cinematics.FovMode`: `NativeHorPlus`, `GameplayHorPlus`.
- Hotkey defaults: F9 cinematic aspect cycle, F10 Dialogue cycle, F11 Gameplay
  mode cycle, F12 cinematic FOV cycle.
- `[Diagnostics] Enabled` is present in the generated configuration template.

No game was launched for this baseline. No Git commit or release was created.
