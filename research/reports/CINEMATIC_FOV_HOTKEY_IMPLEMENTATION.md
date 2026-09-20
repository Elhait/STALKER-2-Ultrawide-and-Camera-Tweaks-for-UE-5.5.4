# Cinematic FOV Hotkey Implementation

Date: 2026-09-19

## Result

Added configurable `CinematicFovCycle=F12` under `[Hotkeys]`.

When hotkeys are enabled, F12 cycles:

```text
NativeHorPlus -> GameplayHorPlus -> NativeHorPlus
```

The selected mode is applied only to the next cinematic, persisted to
`Cinematics.FovMode`, and logged with the same persistence-success/failure
semantics as the existing hotkeys. The active cinematic is not mutated.

## Validation

- Full `test.cmd`: PASS.
- Config synchronization and F12 cycle coverage: PASS.
- `build.cmd`: PASS with existing external Zydis C4201 warnings.
- `git diff --check`: PASS; only normal line-ending conversion warnings.
- No game launch.

## Scope review

Changed only hotkey configuration, mode-cycle selection, runtime hotkey edge
handling, documentation and deterministic config coverage. Cinematic FOV math,
baseline propagation, active cinematic behavior, Dialogue, ZOOM and Gameplay
mode handling were not changed.
