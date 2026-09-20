# Global HorPlus — Combined Runtime Pass

Date: 2026-09-20

## Evidence identity

The supplied combined runtime log is:

`E:\Steam\steamapps\common\S.T.A.L.K.E.R. 2 Heart of Chornobyl\Stalker2\Binaries\Win64\STALKER2CameraTweaks.log`

The log records:

```text
modSha256=19F2F31C20BB5D47CD12D2D3D773774985A5D771E6F7F8730A6363983161DA72
gameSha256=61BC1E030740CEBC30CF1DAD0C86CF65E39E12FF0500225821D684181E08D56B
```

Initialization reported Gameplay, Cinematics and Dialogue available.

## Milestone

The combined runtime session is accepted as the current milestone:

```yaml
global_horplus_combined_runtime: PASS
gameplay_horplus: PASS
native_horplus: PASS
gameplay_horplus_cinematic: PASS
f11_live_mode_switch: PASS
f12_cinematic_mode_switch: PASS
auto_cinematic_aspect: PASS
forced_cinematic_aspect: PASS
ads_binocular: PASS
dialogue_coexistence: PASS
save_load_camera_recreation: PASS
legacy_aspect_cleanup_regression: PASS
```

The log contains restoration invalidation/update events, F11 transitions in
both directions, F12 mode changes, Auto viewport resolution at `3.55556`,
forced `21:9` policy at `2.38889`, coherent GameplayBaseline selection for
GameplayHorPlus, NativeHorPlus fallback selection, cinematic EXIT recovery,
and subsequent Dialogue candidate activity without a new ownership decision.

The user also confirmed the corresponding visual and interaction checks for
ADS/binocular, Dialogue coexistence, save/load/camera recreation and the live
mode switches during this session.

## Important qualification

The cached ENTER numeric guard remains a known provenance ambiguity. It had no
observed runtime defect in this session, so it is not changed or reclassified
as a failure.

## Corpus update

The minimal forced-21:9 cinematic fixture records the observed authored FOV,
GameplayBaseline native FOV, effective aspect and both deterministic transform
results. The replay harness now checks that fixture in addition to the existing
32:9 evidence. Unstable timestamps, pointers and sequence IDs are not encoded
as expected values.
