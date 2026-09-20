# Weapon Viewmodel FOV — Full Research and First-Iteration Plan

## Final status

**REJECTED — runtime write confirmed / visual ownership rejected.**

The first implementation iteration was intentionally limited to a fixed
first-person FOV value applied after the existing gameplay camera-writer
boundary. The write succeeded in the live game, but the weapon/viewmodel did
not change visually. The candidate camera field is therefore not the active
visual owner of Weapon Viewmodel FOV for the current supported executable.

Production source and the rebuilt ASI were restored to the pre-experiment
state. No weapon FOV feature is shipped.

## Environment

```text
Game:       STALKER 2 Steam 2.0.5
Engine:     Unreal Engine 5.5.4
Resolution: 2560x1440, 16:9
Mod:        STALKER2CameraTweaks.asi
```

The original subtitle issue and the weapon FOV issue were treated as separate
research tracks. Subtitle centering was closed independently: Lua located the
`SubtitleView` root and proved whole-block translation, but dynamic post-layout
geometry was unavailable. No subtitle production change was made.

## Initial objective

The proposed feature had two possible uses:

1. apply a larger Weapon Viewmodel FOV after a cinematic/save reload when the
   game leaves the viewmodel in the wrong state;
2. expose a configurable global Weapon Viewmodel FOV mode in the mod.

The first iteration was narrowed to the simpler question: can one exact live
value be increased and produce a visible weapon/viewmodel change? Custom
per-weapon profiles, lifecycle repair and automatic post-cinematic refresh
were explicitly deferred.

## Evidence before implementation

### Reference mod analysis

The standard reference archive was inspected:

```text
Reference-mods/Weapon Viewmodel FOV 2.1 2422 2.1
2026-09-09T17-28Z OxGWqGdDK.zip
```

The separate `Weapon Viewmodel FOV 2.1 - Custom Weapons` archive was excluded
from this task. The standard archive provides global baseline profiles from
`FOV 0` through `FOV 40`; the custom package only overrides individual
weapons.

Each baseline profile contains three groups (`N`, `F_99_P`, `O`) and, for
Vortex installation, nine `.pak/.ucas/.utoc` files in total.

### Cooked asset diff

The `FOV 0` ↔ `FOV 40` comparison established:

```text
N:
  byte-identical payload containers
  registry/plugin metadata changes only
  FOV payload: not present

F_99_P:
  seven cooked CurveFloat assets changed
  FOV payload: confirmed

O:
  zero-byte all-0.wvf/all-40.wvf marker changes
  payload containers otherwise unchanged
  exact marker role: unproven
```

Changed curve assets included:

```text
AimingWeaponFOVCurve.uasset
OffsetAimingWeaponFOVCurve.uasset
AimingBinocularsFOVCurve.uasset
AimingBinocularsFOVCurve_zoom_out.uasset
GP3AAimingFOVCurve.uasset
SKPAimingFOVCurve.uasset
```

The data-driven reference mechanism was therefore classified as:

```text
cooked CurveFloat assets → weapon/viewmodel FOV pipeline
```

The exact consumer and cached runtime state remained unknown.

### UE4SS CurveFloat discovery

UE4SS found the live objects:

```text
/Game/GameLite/Blueprints/Curves/AimingWeaponFOVCurve.AimingWeaponFOVCurve
/Game/GameLite/Blueprints/Curves/OffsetAimingWeaponFOVCurve.OffsetAimingWeaponFOVCurve
```

Both exposed `FloatCurve`, `Keys` and two key entries. The observed reference
profiles showed that the user-facing `+40%` label is not a simple
`10 × 1.4` multiplication. Representative runtime values were:

```text
FOV 0:
  AimingWeaponFOVCurve:       (0, 0), (1, 10)
  OffsetAimingWeaponFOVCurve: (0, 0), (1, 10)

FOV 40:
  AimingWeaponFOVCurve:       (0, 0), (1, 10)
  OffsetAimingWeaponFOVCurve: (0, 40), (1, 0)
```

The exact values varied by inspection order/profile state, but the important
facts were stable: the reference profile changed cooked curve data and the
FOV 40 profile visibly enlarged the weapon.

### Runtime curve mutation test

With the reference FOV 40 profile installed, UE4SS changed individual live
curve keys:

```text
AimingWeaponFOVCurve key 2: 40 → 20
OffsetAimingWeaponFOVCurve key 1: 40 → 20
```

The values read back successfully, but the active weapon did not visibly
change. This established that the live CurveFloat objects are inputs or
initialization data, not necessarily the already-evaluated active viewmodel
state. Re-equip/normal runtime observation did not produce a visual proof.

### Camera paths tested and rejected

UE4SS inspection found one live player camera and one PlayerCameraManager.
The player camera reported:

```text
FieldOfView=90
FirstPersonFieldOfView=90
FirstPersonScale=1
bEnableFirstPersonFieldOfView=false
```

Direct runtime changes produced no visual effect:

```text
FirstPersonFieldOfView: 90 → 120 → 90
FirstPersonScale:       1 → 1.5
bEnableFirstPersonFieldOfView: false → true
```

The PlayerCameraManager values also remained native and did not identify the
weapon viewmodel owner. The separate cinematic cameras were not the gameplay
weapon camera.

### Weapon and animation object discovery

The live character exposed:

```text
WeaponInHandsMeshComponent
ItemAppearanceComponent
AnimScriptInstance
PostProcessAnimInstance
```

The active weapon mesh was confirmed as:

```text
/Game/_STALKER2/SkeletalMeshes/weapons/smg/aku/SK_aku.SK_aku
```

The animation instance was valid. Its observed state changed correctly:

```text
Normal:
  bAiming=false
  AimAlpha=0
  OffsetAimAlpha=0
  AimState=0

ADS:
  bAiming=true
  AimAlpha=1
  OffsetAimAlpha=0
  AimState=3
```

This made the animation instance a promising future consumer/lifecycle anchor,
but no direct FOV ownership was established in this task.

## First implementation iteration

### Approved scope

The temporary implementation added:

```ini
[WeaponViewmodel]
FOV=120
```

and applied the value at the existing, already validated gameplay
camera-writer boundary. The write targeted the reference candidate:

```text
Camera +0x234       first-person FOV field
Camera +0x262 bit 2 first-person FOV enable selector
```

The default was `FOV=0`, so existing users were not changed unless they opted
into the test. Existing gameplay aspect, cinematic, dialogue and authored
world-FOV behavior remained untouched.

### Runtime result

The game log confirmed that the configuration was loaded and the native write
was executed:

```text
WeaponViewmodel.FOV=120
Weapon viewmodel FOV applied:
  previous=90
  target=120
  flags=0x1 -> 0x5
```

The weapon/viewmodel showed **no visible change**. This was a controlled
negative result, not a configuration failure or a failed memory write.

## Decision

```text
Camera +0x234 write:       CONFIRMED
Enable flag write:         CONFIRMED
Game stability in test:    CONFIRMED
Visible weapon FOV change: NONE
Visual ownership:          REJECTED
```

No further timing, ADS, reload, cinematic-exit or repeated-offset testing is
warranted for this camera seam. The game accepted the memory mutation but the
active viewmodel pipeline did not consume it visibly.

## Cleanup performed

The temporary implementation was removed from:

```text
src/experimental_cinematic_21_9_combined_fix_204.cpp
```

The generated configuration template was restored to its pre-experiment
state. The ASI was rebuilt from the restored production source. No release
archive or stable gameplay/cinematic/dialogue implementation was changed.

The installed external INI may still contain the old unused block:

```ini
[WeaponViewmodel]
FOV=120
```

The restored ASI ignores this section. It can be removed or set to `FOV=0`
manually.

## Non-goals and rejected branches

```text
Per-child subtitle alignment                 out of scope / separately closed
Fixed subtitle offsets                        rejected
Camera FirstPersonFieldOfView direct write   visual ownership rejected
Camera FirstPersonScale                       visual ownership rejected
PlayerCameraManager FOV fields                visual ownership rejected
CurveFloat live key mutation                  no active visual response
Custom per-weapon FOV                         excluded from standard archive task
ADS/cinematic periodic reapply                not justified after seam rejection
Slate/FGeometry bridge                        unrelated to weapon FOV seam
Release packaging                             not performed
```

## Future research entry point

### Post-closure archive inspection

After the camera-field experiment was rejected, the standard reference archive
was inspected again at the container level. This corrected an earlier
interpretation of the `N` group.

The `Weapon_Viewmodel_FOV_0_N.utoc/.ucas` data contains names for a shared
asset set including:

```text
WVF
WVF_Actor
Weapon_Viewmodel_FOV
FOV_0 ... FOV_40
FOV_Offset_0 ... FOV_Offset_40
FOV_Scope_0 ... FOV_Scope_40
FOV_Bino_0 ... FOV_Bino_40
FOV_Bino_Out_0 ... FOV_Bino_Out_40
```

The `N` containers are byte-identical between the reference `FOV 0` and
`FOV 40` profiles:

```text
N.pak  ==
N.ucas ==
N.utoc ==
```

Therefore the corrected conclusion is:

```text
N is not metadata-only.
N contains a shared WVF asset/infrastructure set.
N is unchanged across the selectable FOV profiles.
```

The container inspection alone does **not** yet prove that every listed asset
is loaded, that `WVF_Actor` is spawned at runtime, or that `N` itself performs
continuous reapplication. Those remain hypotheses until component isolation or
runtime object evidence confirms them.

The `O` `.ucas/.utoc` containers were identical between FOV 0 and FOV 40; the
`.pak` wrapper differed, consistent with the previously observed zero-byte
profile marker. Its loader semantics remain unproven.

The new behavioral observation is:

```text
Vanilla, after cinematic EXIT:          weapon framing broken
Reference Weapon Viewmodel FOV - 0:    weapon framing correct
```

This demonstrates that the complete reference installation changes the
post-cinematic result even when the selected profile is named `FOV 0`. It does
not identify which of `N`, `F_99_P`, `O`, or their combination is responsible.

The next research batch, if reopened, is therefore component isolation:

```text
N + F_99_P + O  baseline reference behavior
F_99_P only     curve-only test
N + O only      shared infrastructure/marker test
N only          infrastructure-only test
F_99_P + O      curve plus marker test
```

This batch must remain research-only and must not alter the stable ASI. Its
purpose is to locate the smallest reference component set that preserves the
post-cinematic weapon state before attempting UE4SS discovery of `WVF`,
`WVF_Actor` or `Weapon_Viewmodel_FOV` runtime objects.

Any future weapon FOV work must start from the confirmed data-driven path and
remain isolated from the stable ASI until visual proof exists:

```text
CurveFloat assets
    ↓
weapon initialization or consumer
    ↓
cached/evaluated weapon FOV state
    ↓
active AnimInstance / viewmodel transform
    ↓
visible weapon framing
```

The next safe research target is the consumer of
`AimingWeaponFOVCurve`/`OffsetAimingWeaponFOVCurve` during weapon initialization
and the relationship with `AnimScriptInstance.AimingData`. A future probe must
be research-only, use the current executable identity, and require visible
weapon movement before any production implementation is considered.

## Git and validation record

```text
Implementation source: restored to pre-experiment state
ASI build:            completed after cleanup
Release assets:       unchanged
Runtime validation:   negative result recorded above
Final classification: rejected
Commit:               not created by this task
```
