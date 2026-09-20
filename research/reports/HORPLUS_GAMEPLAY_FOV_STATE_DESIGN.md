# HorPlus Gameplay FOV State Design

## Current source behavior

The current production HorPlus path reads the writer's current `XMM0` FOV, reads runtime aspect and flags from the camera context, and calls `TryTransformHorPlus()` on every eligible writer callback. It does not currently maintain a gameplay HorPlus cache or classify the reason for each FOV change.

This is separate from the already validated ADS owner integration and from the cinematic/Dialogue exclusion paths.

## Design contract

### Separate FOV domains

Gameplay values:

```text
ConfiguredGameplayFov
NativeGameplayCameraFov
HorPlusGameplayCameraFov
```

Cinematic values:

```text
NativeCinematicFov
CinematicHorPlusFov
```

An authored cinematic FOV, including an observed value of `90`, is not a gameplay baseline and must not make the gameplay cache valid.

### Cache validity

```text
GameplayHorPlusState {
    valid
    configuredFov
    nativeGameplayFov
    runtimeAspect
    gameplayMode
    horPlusGameplayFov
}
```

The cache is valid only when all applicable inputs match the result. Changes to runtime aspect, configured gameplay FOV, gameplay mode, load/restore ownership, or the first trustworthy gameplay-camera value after cinematic invalidate or re-establish it as appropriate.

The configured gameplay FOV source is not yet established in the current source and must not be guessed during implementation.

### Ownership policy

```text
Gameplay / ADS → gameplay HorPlus may apply
Cinematic      → cinematic HorPlus only; do not manufacture gameplay cache
Dialogue       → Dialogue owns its FOV; gameplay HorPlus does not apply
Recovery       → no gameplay HorPlus transform
Unknown        → native pass-through + diagnostic record
```

For `Gameplay → Cinematic`, preserve a valid gameplay cache without recalculating it from cinematic samples. For direct load into cinematic, keep gameplay state invalid until gameplay ownership supplies a trustworthy native gameplay FOV.

### Change-only telemetry

The diagnostic contract is:

```text
GAMEPLAY_FOV_CHANGE
configured=...
nativeOld=...
nativeNew=...
modifier=...
horPlusOld=...
horPlusNew=...
aspect=...
owner=...
cacheValid=...
reason=...
handled=...
```

`modifier` is `nativeNew - configured` and is evidence, not an ownership classifier.

## Required runtime questions

One later runtime pass should use a non-90 configured gameplay FOV where practical and cover:

- idle gameplay;
- ADS and rapid ADS reversal;
- sprint/run and weapon changes;
- damage/DoT and death/respawn;
- Dialogue;
- Cinematic ENTER/EXIT;
- binocular 2.0;
- load into gameplay and load into cinematic where available.

The first purpose is to enumerate unknown native gameplay FOV changes. It is not permission to classify all unknown changes as HorPlus-eligible before evidence.

## Status

Static design prepared.

Telemetry-only candidate implementation is now built behind `HORPLUS_FOV_STATE_DIAGNOSTIC`:

- `GAMEPLAY_FOV_CHANGE`, `CINEMATIC_FOV_CHANGE`, `DIALOGUE_FOV_CHANGE` and `UNKNOWN_FOV_CHANGE` are change-driven;
- `CAMERA_OWNER` transitions are logged separately;
- `HORPLUS_STATE` validity establishment/invalidation records a reason;
- configured gameplay FOV remains `UNKNOWN` until a validated source is found;
- HorPlus output behavior is not changed by the telemetry path.

Build and existing harness validation pass. Runtime validation remains pending. The stable ASI was not replaced.
