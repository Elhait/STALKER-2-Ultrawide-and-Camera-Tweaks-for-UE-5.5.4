# Weapon Viewmodel FOV — Spawn Identity and Profile Data Flow Batch 3

## Objective

Continue the offline static investigation from the successful Kismet Batch 2.
Resolve the two `BeginDeferredActorSpawnFromClass` class arguments in
`WVF_C::ExecuteUbergraph_WVF(15)` and trace the profile-value data flow inside
`WVF_Actor_C` from `.wvf` discovery through the first external consumer.

## Established evidence and current state

- Vanilla post-cinematic weapon framing is broken; `Weapon Viewmodel FOV - 0`
  preserves correct framing. Vanilla ADS independently repairs the framing.
- `WVF_C` and `WVF_Actor_C` are live only with the reference mod and their
  runtime lifecycle/execution is confirmed.
- CUE4Parse with the runtime-generated `.usmap` successfully deserializes the
  targeted packages and Kismet bytecode.
- `WVF_C::OnWorldBeginPlay` calls `ExecuteUbergraph_WVF(1423)`.
- `ExecuteUbergraph_WVF(15)` begins with `GetPlayerPawn` and contains two
  `BeginDeferredActorSpawnFromClass → FinishSpawningActor` chains.
- Their class arguments are unresolved package imports represented as
  `FPackageIndex -1` and `FPackageIndex -3`.
- `WVF_Actor_C::UserConstructionScript` contains `.wvf` profile strings,
  `all-0.wvf`, `FindFiles`, string-to-double conversion and Blueprint map
  insertion.
- `WVF_Actor_C::ReceiveBeginPlay` dispatches entry `3903`; that path reaches
  `SetScalarParameterValue`, but material ownership of visible weapon FOV is
  not established.

## Batch 3 static results to date

### Batch 3.1 — Spawn class resolution

The two class arguments were initially unresolved in the manually constructed
IoPackage because the direct reader was not registered with the provider. The
bounded fix was to register the exact `N.utoc` with `DefaultFileProvider` and
load the package through the provider path before resolving Kismet references.

The import references are:

```text
FPackageIndex -1 → PackageImportIndex=1, HashIndex=0
FPackageIndex -3 → PackageImportIndex=0, HashIndex=1
```

The provider-backed package now resolves the imports and the two spawn class
arguments exactly:

```text
FPackageIndex -1
  → BlueprintGeneratedClass '/S2Dev_Library/S2Dev_Event_Watcher.S2Dev_Event_Watcher_C'

FPackageIndex -3
  → BlueprintGeneratedClass '/Weapon_Viewmodel_FOV/WVF_Actor.WVF_Actor_C'
```

The direct reader's `ImportedPackages[0..2]` remains null when constructed
without provider registration; that is a tooling-path limitation, not game
evidence. The provider-backed `ImportMap`/`ResolveObjectIndex` path is the
authoritative result for this batch.

Result: `SUCCESS` for 3.1.

### Batch 3.2 — Profile-value data flow

The actor dispatcher contains the following concrete static path:

```text
ExecuteUbergraph_WVF_Actor entry 15
  → S2Dev - Get Main Hand Item Data
  → BlueprintMapLibrary:Map_Find (instance profile map)
  → SelectFloat / viewport and FOV-related numeric operations
  → KismetMaterialLibrary:SetScalarParameterValue branches
```

`UserConstructionScript` independently establishes the map population path:

```text
.wvf / all-0.wvf discovery
  → ParseIntoArray("-")
  → Conv_StringToDouble
  → BlueprintMapLibrary:Map_Add
```

This is a concrete static application candidate, but the material scalar is
not yet proven to own visible weapon-FOV framing. The bounded provider-backed
Kismet pass now resolves the call arguments for eight branches in
`ExecuteUbergraph_WVF_Actor`:

```text
SetScalarParameterValue target:
  MaterialParameterCollection
    /Game/_Stalker_2/Materials/MPC/MPC_FOV.MPC_FOV
  parameter name:
    TanFOV
  value:
    local float produced by the preceding profile/viewport/FOV math branch
```

Observed serialized call statement indices are `1048`, `1862`, `2842`,
`3356`, `3869`, `4701`, `5165` and `5675`; the corresponding value inputs are
local variables at `1071`, `1885`, `2865`, `3379`, `3892`, `4724`, `5188` and
`5698`. These are static Kismet call arguments, not proof that the material
parameter controls the visible weapon projection.

Result: `PARTIAL` for 3.2; profile map read and downstream call path are
recovered through a concrete MPC parameter write, final downstream consumer
and visible/native ownership remain open.

Overall Batch 3 status: `PARTIAL` (`3.1 SUCCESS`, `3.2 PARTIAL`).

The import-resolution blocker is closed for the provider-backed analysis path.
The direct-reader fallback remains bounded and diagnostic only. No runtime
testing, new UE4SS probes or production changes are authorized by this batch.

## Approved scope

- Modify only the targeted CUE4Parse research dumper and this plan.
- Inspect only `WVF.uasset`, `WVF_Actor.uasset` and the directly referenced
  package/import metadata needed to resolve the two spawn classes.
- Trace only the bounded Kismet expressions for spawn arguments, profile-file
  parsing, map writes/reads and direct external calls.
- Preserve the existing runtime-generated `.usmap` and IoStore transport path.

## Explicit non-goals

- No game launch, UE4SS probe, runtime hook or new visual test.
- No production ASI/source/release changes.
- No changes to camera/PCM, AnimScriptInstance, F_99_P curves or O-profile
  behavior already established.
- Do not classify `SetScalarParameterValue` as the weapon-FOV owner without
  direct static or runtime ownership evidence.
- Do not infer a class name from package order, import index or naming alone.

## Expected files or areas

- `research/tools/CUE4ParseWVF/Program.cs`
- `backlog/active/WVF_APPLICATION_PATH_BATCH3_SPAWN_PROFILE_TASK_PLAN.md`
- No production files or release artifacts.

## Batches and validation

### Batch 3.1 — Spawn class resolution

Resolve `FPackageIndex -1` and `-3` through the provider-backed package import
references and record exact object/package evidence. The exact `N.utoc` must be
registered with `DefaultFileProvider`; a direct reader alone is insufficient
for reliable package-import resolution.

### Batch 3.2 — Profile-value data flow

Trace the bounded `UserConstructionScript` and related actor functions from
`.wvf`/`all-0.wvf` discovery through string parsing, map insertion, map lookup
and the first external/native or object-property consumer. Record exact
function names, object paths, property paths and Kismet statement offsets.

### Batch 3.3 — Static classification

```text
SUCCESS  — spawn identity and/or a concrete profile-value consumer is recovered
  PARTIAL  — one bounded edge is recovered but the next owner remains unresolved
DEFERRED — current CUE4Parse/import metadata cannot resolve the requested edge
```

The stop condition applies to the current sub-batch only. A successful
Batch 3.1 spawn-class resolution does not close Batch 3; Batch 3.2 remains
required unless it reaches its own stop condition or a technical blocker.

## Risks and rollback / safe-failure behavior

- Keep all package/import resolution and reflection access bounded and guarded.
- Preserve the working CUE4Parse mount and `.usmap` path.
- Do not treat unresolved imports, null resolved objects or parser exceptions
  as evidence about the game or the reference mod.
- If a tool invocation fails, record the exact error and stop that sub-batch.
- No production rollback is needed because production paths are out of scope.

## Stop conditions and phase gates

- Stop the current sub-batch when one spawn class is resolved or one concrete
  profile-value consumer is mapped; do not broaden that sub-batch
  automatically. Continue with the other planned sub-batch afterward.
- Stop on serialization/import-resolution failure rather than guessing.
- Do not run the game in this batch.
- Do not propose ASI implementation until a concrete runtime owner is still
  demonstrated by separate runtime evidence.

## Expected final Git review

Perform a read-only review of the exact dumper, plan and research paths.
Report completed, remaining, deferred, blocked and not-runtime-validated
items. Do not stage or commit.
