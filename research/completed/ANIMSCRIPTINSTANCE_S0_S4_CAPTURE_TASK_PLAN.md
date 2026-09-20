# Weapon Viewmodel FOV — AnimScriptInstance, WVF A/B and Lifecycle Boundary

## Objective

Record the bounded runtime evidence needed to separate the vanilla ADS repair
path from the reference `Weapon Viewmodel FOV - 0` repair path, then stop
repeating visual/A-B tests and prepare a focused WVF lifecycle/application
tracer.

This plan remains research-only. It does not authorize changes to the stable
ASI, production source, release artifacts or the installed reference mod.

## Established evidence and current state

### Fixed behavioral anchors

The following behavior is now treated as established input, not as a test to
repeat:

```text
Vanilla:
  cinematic EXIT → weapon framing BROKEN
  ADS cycle      → framing CORRECT

Weapon Viewmodel FOV - 0:
  cinematic EXIT → framing CORRECT
```

The reference mod is known to remain correct across repeated loading scenarios
from user testing. Further visual confirmation of this fact is out of scope.

### F7–F11 capture contract

Only these research hotkeys are used:

```text
F7  → S1: during cinematic
F8  → S2: immediately after EXIT, weapon absent
F9  → S3: weapon visible
F10 → S4: ADS active
F11 → S5: ADS released
```

Keys below `F7` are excluded from this research. `F9` is also the single
manual WVF discovery point, so one press records the S3 snapshot and the exact
WVF runtime discovery for the same observation moment.

### AnimScriptInstance evidence

- `S1` and `S2` correctly report no weapon/AnimScriptInstance.
- Vanilla S3/S4/S5 used one stable weapon mesh and one stable
  `AnimBP_pm_fp_C` instance within that lifecycle.
- The reflected `AimingData` transition is:
  - normal/post-ADS: `bAiming=false`, `AimAlpha=0`, `OffsetAimAlpha=0`,
    `AimState=0`;
  - active ADS: `bAiming=true`, `AimAlpha=1`, `OffsetAimAlpha=0`,
    `AimState=3`.
- After ADS release the fields return to baseline while the visual framing
  remains repaired. Persistent `AnimScriptInstance`/`AimingData` values do not
  explain the repair.
- Vanilla and WVF_0 S3 snapshots both use `AnimBP_pm_fp_C` with baseline
  `AimingData=false/0/0/0`, despite different visual framing. FullName and
  pointer equality across separate runs is not expected and is not an equality
  criterion.

### Corrected runtime WVF A/B

The initial WVF discovery ran too early at `InitMap`; that result was not
treated as evidence. The probe was then changed to run manually on F9.

WVF_0 run, `UE4SS.log` around `15:28:13`:

```text
S3 weapon visible; AnimBP_pm_fp_C; AimingData=false/0/0/0
WVF_Actor_C: liveMatches=1
  FullName = ...PersistentLevel.WVF_Actor_C_2147464576
  Class    = /Weapon_Viewmodel_FOV/WVF_Actor.WVF_Actor_C
  Owner    = ...S2Dev_Event_Watcher_C_2147464577
  Outer    = ...PersistentLevel
WVF_C: liveMatches=1
  FullName = ...WorldMap_WP:WVF_C_2147481093
  Class    = /Weapon_Viewmodel_FOV/WVF.WVF_C
  Owner    = <unavailable>
  Outer     = ...WorldMap_WP
```

Vanilla run, `UE4SS.log` around `15:37:05`:

```text
S3 weapon visible; AnimBP_pm_fp_C; AimingData=false/0/0/0
WVF_Actor_C: FindAllOf failed
WVF_C: FindAllOf failed
```

The vanilla result is limited to the F9 observation point: no live reflected
matches were found there. It is not a claim that WVF classes could never be
loaded in some unrelated process.

The resulting controlled matrix is:

| S3 post-EXIT | Vanilla | WVF_0 |
|---|---|---|
| Weapon mesh | present | present |
| Weapon mesh class | `SkeletalMeshComponent` | `SkeletalMeshComponent` |
| AnimInstance class | `AnimBP_pm_fp_C` | `AnimBP_pm_fp_C` |
| `bAiming` / `AimAlpha` / `OffsetAimAlpha` / `AimState` | `false/0/0/0` | `false/0/0/0` |
| `WVF_Actor_C` live match | not found | 1 |
| `WVF_C` live match | not found | 1 |
| Visual framing | BROKEN | CORRECT |

This closes the V-S3 ↔ W-S3 A/B as a runtime correlation. The exact causal
application function and downstream consumer remain unknown.

### Vanilla save-load / ClientRestart evidence

One vanilla run tested repair, save reload and the next cinematic in the same
process:

```text
15:42:23  S5 after ADS:
  WeaponMesh ...E8BED99F...
  AnimInstance ...AnimBP_pm_fp_C_2147461570
  AimingData=false/0/0/0
  visual CORRECT

15:42:34  S3 before Load Save:
  same WeaponMesh and AnimInstance
  AimingData=false/0/0/0
  WVF_Actor_C/WVF_C not found

15:42:45  ClientRestart

15:43:20  S3 after Load Save + cinematic:
  new WeaponMesh ...AA750DAE...
  new AnimInstance ...AnimBP_pm_fp_C_2147438389
  AimingData=false/0/0/0
  WVF_Actor_C/WVF_C not found
  visual BROKEN
```

This confirms `ClientRestart` as a lifecycle boundary for the vanilla
character, weapon mesh and AnimScriptInstance. The vanilla ADS repair is not a
persistent configuration that survives that boundary.

## Approved scope

- Keep the bounded UE4SS capture script under
  `research/ue4ss/AnimScriptInstanceS0S4Capture/`.
- Keep `F7`–`F11` as the only research hotkeys.
- Keep `WVFRuntimeDiscovery` manual and bound to `F9`; search only exact
  reflected class names `WVF_Actor_C` and `WVF_C`.
- Design the next research-only lifecycle/application tracer around the
  `Load Save`/`ClientRestart` to weapon-visible interval, using the known WVF
  classes as anchors.
- Record event order, timestamps, FullName, Class, Owner and Outer only when
  safely accessible.

## Explicit non-goals

- No repeated confirmation that vanilla is BROKEN or WVF_0 is CORRECT.
- No repeated O0/O20/O40 matrix.
- No repeated V-S3/W-S3 snapshots except when required as a lifecycle anchor.
- No repeated AnimInstance/AimingData field dumping.
- No identity-only reload test whose result cannot change the model.
- No repeated search for already confirmed `WVF_C`/`WVF_Actor_C` existence.
- No camera/PCM research, custom weapons research or WVF property mutation.
- No UObject writes, production ASI/source changes, builds or release changes.
- No polling, arbitrary timers or hotkeys below `F7`.

## Expected files or areas

- `research/ue4ss/AnimScriptInstanceS0S4Capture/Scripts/main.lua`
- `research/ue4ss/WVFRuntimeDiscovery/Scripts/main.lua`
- Future separate lifecycle tracer under `research/ue4ss/` only after its own
  bounded plan is approved.
- User-side copies in the installed UE4SS `Mods` directory for disposable
  runtime captures.

## Completed batches and validation

### Batch 1 — Bounded S1–S5 capture

Completed. The script safely handles weapon-absent S1/S2 states and records
weapon mesh, AnimScriptInstance and nested `AimingData` for S3–S5.

### Batch 2 — F9-aligned WVF discovery

Completed. The prior init-time scan was removed. The installed log confirms:

```text
WVFRuntimeDiscovery loaded:
F9=manual exact WVF discovery; no init-time scan
```

The F9 probe found one live `WVF_Actor_C` and one live `WVF_C` with WVF_0 and
found no live matches at the corresponding vanilla S3 point.

### Batch 3 — Vanilla save-load control

Completed. Same-process `ClientRestart` produced new vanilla character,
weapon-mesh and AnimScriptInstance identities, and the next post-cinematic
state was broken again. WVF objects remained not found.

## Next bounded batch — lifecycle/application tracing

This is the only active research direction after the completed A/B.

### Goal

Determine whether WVF initialization/application occurs before or after
`ClientRestart`, and identify the earliest observable WVF callback/object event
that could lead to correct post-cinematic framing.

### Candidate anchors

```text
ClientRestart / Load Save
→ WVF_C: OnWorldBeginPlay
→ WVF_Actor_C: ReceiveBeginPlay
→ WVF_Actor_C: On Event Watcher
→ weapon lifecycle
→ F9/S3 observation
```

These are investigation targets, not assumed execution order. Callback
arguments must be handled defensively because an earlier watcher hook attempted
an unsupported `GetFullName` call.

### Success criteria

- A timestamped, reproducible WVF lifecycle event or callback is observed
  relative to `ClientRestart`, weapon creation and F9/S3; or
- a concrete external state/function touched by WVF is identified.

### Not enough

- Different UObject IDs after reload;
- another generic FOV field;
- another visual confirmation already established;
- static class names without a runtime event or owner;
- a speculative causal chain.

## Risks and rollback / safe-failure behavior

- All probes remain read-only and research-only.
- Missing objects are logged as not found; they are not substituted.
- Unsafe reflected access must be reported and skipped rather than retried
  through broad enumeration.
- If callback signatures cannot be handled safely, stop the tracer batch and
  preserve the failure as a tooling limitation.
- Disable or remove only the exact disposable research-mod copies; production
  files remain untouched.

## Stop conditions and phase gates

- Do not run another behavior or identity-only test unless it distinguishes
  two still-open hypotheses.
- Stop lifecycle tracing when a concrete runtime event/state/function is
  correlated with the WVF path, or when the available UE4SS reflection surface
  cannot safely expose it.
- Do not implement an ASI probe until runtime ownership and visible effect are
  both demonstrated.

## Expected final Git review

For the documentation update, perform a lightweight path/status review only.
For any future tracer implementation, review the exact research paths with
`git status` and relevant diff, and report completed, remaining, deferred and
not-runtime-validated items. Do not stage or commit.
