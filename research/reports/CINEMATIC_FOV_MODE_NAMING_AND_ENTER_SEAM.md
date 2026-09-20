# Cinematic FOV Mode Naming and ENTER Seam

Date: 2026-09-19

## Result

The public cinematic FOV modes are now:

- `NativeHorPlus` — the existing validated `90 -> 126.87` behavior and
  default.
- `GameplayHorPlus` — opt-in gameplay-baseline-relative transfer.

Legacy parser aliases `HorPlus` and `MatchGameplay` remain accepted for
existing INI files and map to the renamed modes.

## Seam correction

`GameplayHorPlus` is applied at the existing cinematic ENTER transform seam.
The gameplay writer no longer contains an independent cinematic FovMode branch;
it retains the existing HorPlus/cached-value behavior.

Invalid or unavailable GameplayHorPlus context falls back to
`NativeHorPlus` at ENTER. No new hook or camera path was introduced.

## Validation

- Full `test.cmd`: PASS.
- `build.cmd`: PASS with existing external Zydis C4201 warnings.
- `git diff --check`: pending final review only.
- No game launch.

Runtime A/B validation remains pending.
