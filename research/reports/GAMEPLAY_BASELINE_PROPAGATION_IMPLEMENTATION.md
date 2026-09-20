# Gameplay Baseline Propagation Implementation

Date: 2026-09-19

## Result

The retained Gameplay baseline used by `GameplayHorPlus` cinematic ENTER is
now published from the actual `Gameplay.Mode=HorPlus` writer path. A valid
Gameplay sample stores its native FOV, source identity and observed aspect in
the existing retained runtime cache.

The cache is updated only while the coordinator is `Gameplay`. CinematicActive
writer samples therefore cannot replace the Gameplay baseline. Invalid source,
FOV or aspect observations leave the previous baseline untouched, preserving
the existing fail-closed fallback to `NativeHorPlus`.

## Root cause

The HorPlus writer path returned before `ReplayManualTransitionOriginal`, while
the latter was the only path publishing `g_lastGameplayCameraFov` and related
observation state. Diagnostics and CameraState saw the native pair, but the
ENTER policy read `NaN` from the retained production cache.

## Validation

- Full `test.cmd`: PASS, including CameraState and MatchGameplay harnesses.
- `build.cmd`: PASS with existing external Zydis C4201 warnings.
- `git diff --check`: PASS; only normal line-ending conversion warnings.
- No game launch.

## Runtime status

Not runtime-validated. The next controlled run should use
`Cinematics.FovMode=GameplayHorPlus` after stable idle Gameplay and verify:

```ini
matchGameplayCandidateAvailable=true
matchGameplayNativeBaseline=112.623
matchGameplayHorPlusBaseline=143.132
matchGameplayBaselineAspect=3.55556
```

The cinematic ENTER must no longer fall back to `NativeHorPlus` solely because
the retained Gameplay baseline is unavailable.

## Scope review

Changed production behavior only at the existing Gameplay writer observation
boundary. Dialogue, ZOOM, cinematic formula, fallback behavior,
AspectRecalculation and resolver logic were intentionally untouched.
