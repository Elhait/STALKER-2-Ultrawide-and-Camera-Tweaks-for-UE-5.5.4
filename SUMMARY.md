# Project Summary

## 1. Production mod

The stable production mod now includes and validates:

- gameplay aspect-ratio correction;
- cinematic aspect/FOV correction;
- automatic cinematic aspect policy;
- forced cinematic modes for `16:9`, `21:9` and `32:9`;
- native cinematic mode;
- dialogue zoom modes: `Native`, `Adaptive`, `Reduced` and `Disabled`;
- dynamic signature resolution instead of fixed RVAs;
- cinematic ENTER/EXIT lifecycle handling;
- a unified gameplay/cinematic coordinator;
- atomic post-cinematic handoff behavior;
- protection against repeated application;
- cross-patch checks covering Steam `2.0.2–2.0.5`;
- production runtime validation on Steam `2.0.5`;
- the unified `v0.6.0` release configuration.

The stable production artifact is `STALKER2CameraTweaks.asi`. The older
`STALKER2UltrawideFix.asi` and `STALKER2GameplayAspectFix.asi` names are
superseded release names.

The production configuration variants covered:

```text
5 cinematic/aspect policies
  Auto, Native, forced 16:9, forced 21:9, forced 32:9

4 dialogue zoom policies
  Native, Adaptive, Reduced, Disabled
```

## 2. Weapon/viewmodel FOV behavior

The following behavior was repeatedly reproduced and is now treated as an
established behavioral baseline:

```text
Vanilla:
  cinematic EXIT → broken weapon framing
  ADS cycle      → correct framing

Weapon Viewmodel FOV - 0:
  cinematic EXIT → correct framing
```

The runtime investigation covered these states:

```text
S0 — normal/correct gameplay
S1 — cinematic
S2 — immediately after EXIT
S3 — weapon appeared
S4 — ADS active
S5 — ADS released
```

The final research hotkey sequence was:

```text
F7  — cinematic
F8  — immediately after EXIT
F9  — weapon appeared
F10 — ADS
F11 — ADS released
```

This provided six distinct runtime states and five primary snapshot points.

## 3. Tested and rejected ownership hypotheses

Approximately twelve independent ownership directions were checked:

1. gameplay camera FOV;
2. camera field `+0x234`;
3. other camera and PCM fields;
4. `CameraComponent` and `PlayerCameraManager` state;
5. `F_99_P` curve paths;
6. O-profile behavior;
7. exposed `WVF_Actor_C` fields;
8. `S2Dev_Event_Watcher_C` as the ADS owner;
9. `AnimScriptInstance` and `AimingData`;
10. the native ADS refresh path;
11. `MPC_FOV.TanFOV`;
12. a standalone ASI UE reflection bridge.

The O-profile matrix tested three values:

```text
O0   → baseline
O20  → farther framing
O40  → farther still
```

This established monotonic O-dependent behavior and closed that branch. No
additional O-profile matrix was justified afterward.

## 4. AnimScriptInstance and ADS evidence

Vanilla and `Weapon Viewmodel FOV - 0` post-EXIT snapshots had equivalent
weapon and animation state:

```text
AnimBP class       = AnimBP_pm_fp_C
bAiming            = false
AimAlpha           = 0
OffsetAimAlpha     = 0
AimState           = 0
```

Therefore persistent `AnimScriptInstance`/`AimingData` state did not explain
the visual difference:

```text
Vanilla S3  → BROKEN
WVF_0 S3    → CORRECT
```

During vanilla ADS, the reflected state changed from `AimState=0` to
`AimState=3`, then returned to baseline after ADS release while the corrected
framing remained. Weapon mesh and animation instance recreation were not
observed. This established a transient ADS refresh/recalculation effect, but
not its exact native owner.

## 5. UE4SS runtime probes

Four bounded research probes were used:

```text
AnimScriptInstanceS0S4Capture
WVFRuntimeDiscovery
WVFApplicationTrace
MPC_TanFOVCausalProbe
```

Runtime evidence confirmed:

```text
WVF_C                    live
WVF_Actor_C              live
S2Dev_Event_Watcher_C    owner of WVF_Actor_C
```

The `On Event Watcher` reflected aim state did not provide a useful ADS
transition and was rejected as the primary ADS anchor.

## 6. Static Blueprint/Kismet analysis

Four toolchain paths were attempted:

1. `repak` — blocked by an Oodle/hash mismatch;
2. `retoc` — container-readable, but no usable legacy asset output;
3. `UAssetToolRivals` — indexing worked, but extraction was blocked by its
   Oodle integration;
4. `CUE4Parse` — successful direct IoStore/Zen package inspection.

`UAssetGUI` was not required after the direct CUE4Parse path succeeded.

The recovered static bootstrap is:

```text
WVF_C::ExecuteUbergraph_WVF(15)
  → spawn S2Dev_Event_Watcher_C
  → spawn WVF_Actor_C
```

The recovered profile/application path is:

```text
all-0.wvf
  → .wvf profile parsing
  → string-to-double conversion
  → profile map
  → main-hand item lookup
  → viewport/FOV math
  → SetScalarParameterValue
```

The target was resolved statically as:

```text
Collection: /Game/_Stalker_2/Materials/MPC/MPC_FOV.MPC_FOV
Parameter:  TanFOV
```

## 7. Causal `TanFOV` proof

One controlled UE4SS intervention changed the active parameter:

```text
Before:   1.678197145462
Requested: 2.0977464318275
After:    2.0977463722229
```

Readback confirmed that the mutation reached the active parameter. At the
same time:

```text
WeaponMesh identity       unchanged
AnimInstance identity      unchanged
TanFOV state               changed
Visible framing            changed
```

This causally established:

```text
all-0.wvf
  → profile/FOV math
  → MPC_FOV.TanFOV
  → visible weapon/viewmodel framing
```

The downstream shader/material reader was not recovered because the available
cooked package metadata had no bounded reverse-reference index. A global
material crawl was explicitly rejected as out of scope.

## 8. Production boundary

The reference mechanism is known. The production implementation remains
deferred for one specific reason: the standalone ASI has no validated,
patch-resilient UE reflection/invocation bridge.

The following were not established in the current ASI architecture:

```text
UObject/UClass registry access
UFunction resolution
StaticFindObject bridge
ProcessEvent ABI
safe UE reflection invocation
```

Therefore:

```text
Reference WVF mechanism          CONFIRMED
TanFOV causal ownership           CONFIRMED
UE4SS independent MPC access     CONFIRMED
Native ASI reflection bridge     DEFERRED
Post-cinematic TanFOV repair     DEFERRED
Custom Weapon FOV                DEFERRED
Production changes               NONE
```

## 9. Overall counts

The documented research covered approximately:

```text
Production policy variants        9
  5 cinematic/aspect policies
  4 dialogue zoom policies

Weapon FOV runtime states         6
Primary snapshot points           5
O-profile values tested           3
Ownership hypotheses              ≈12
UE4SS research probes             4
Asset/parser toolchain paths      4
Major WVF research batches        5
Controlled TanFOV interventions   1
Reference lifecycle paths         2

Production WVF changes            0
Production ASI changes            0
```

The active Weapon Viewmodel FOV feature branch is closed. Reopening it is
justified only by a new, independently grounded UE reflection/invocation
anchor. The existing causal evidence should be reused directly rather than
repeating the O-profile, Camera/PCM, AnimInstance, converter or TanFOV
validation work.
