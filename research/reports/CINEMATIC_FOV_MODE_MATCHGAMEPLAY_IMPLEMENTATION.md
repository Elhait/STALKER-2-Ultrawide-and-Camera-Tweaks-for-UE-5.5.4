# Cinematic FOV Mode MatchGameplay Implementation

Date: 2026-09-19

## Scope

Added an opt-in `Cinematics.FovMode=MatchGameplay` mode to the canonical ASI.
The existing `HorPlus` mode remains the default and the fallback path.

## Implementation

- Added `CinematicFovMode::{HorPlus, MatchGameplay}` configuration parsing,
  template synchronization, and startup logging.
- Kept the same cinematic writer hook, coordinator eligibility, effective
  aspect, cached ENTER numeric guard, and output write timing.
- In `MatchGameplay`, valid native writer samples use tangent-space transfer
  from the captured gameplay baseline and ENTER reference, then the existing
  HorPlus aspect transform.
- If context or math is invalid, the existing HorPlus calculation is used.
- Cached transformed ENTER samples remain bypassed by the existing guard.
- Added configuration and MatchGameplay math harness coverage.

## Validation

- Full `test.cmd`: PASS.
- `build.cmd`: PASS with the existing external Zydis C4201 warnings.
- `git diff --check`: PASS apart from normal line-ending warnings.
- No game launch.

## Status

- `FovMode=HorPlus`: static/harness equivalent to the existing path.
- `FovMode=MatchGameplay`: implemented and build/harness validated, not yet
  runtime validated.
- MatchGameplay semantic reference remains an experimental ENTER observation;
  no universal cinematic-reference claim is made.
- Direct-load and normal Gameplay-to-Cinematic runtime A/B remain required.
