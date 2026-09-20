# Weapon Viewmodel FOV — Kismet EntryPoint Recovery Batch 2

## Objective

Use the existing CUE4Parse IoStore + `.usmap` pipeline to map the confirmed
runtime Blueprint dispatcher entry points to cooked Kismet structure:

```text
WVF_C::ExecuteUbergraph_WVF          EntryPoint 1423, 15
WVF_Actor_C::ExecuteUbergraph_WVF_Actor  EntryPoint 3903
```

Recover only the nearby bytecode/instruction context and direct Blueprint or
native call references needed to identify profile selection, actor spawning or
external weapon-state application.

## Established evidence and current state

- CUE4Parse can mount the STALKER 2 UE5.5.4 IoStore container.
- The runtime-generated `.usmap` allows the WVF package to deserialize with
  `canDeserialize=True`, exports and names available.
- `WVF_C`, `ExecuteUbergraph_WVF`, `OnWorldBeginPlay` and
  `WVF_Actor_C` exports/classes are confirmed.
- Runtime tracing confirmed EP `1423`, EP `15` and EP `3903`.
- The current dumper now deserializes `ScriptBytecode` for the targeted
  functions using the runtime-generated `.usmap`.

## Established static results

- `WVF.uasset`: data chunk `5839` bytes; `canDeserialize=True`; `exports=4`.
- `WVF_Actor.uasset`: data chunk `25866` bytes; `canDeserialize=True`;
  `exports=9`.
- `WVF_C::OnWorldBeginPlay` calls
  `WVF_C::ExecuteUbergraph_WVF(1423)`.
- Runtime `EntryPoint=1423` maps to serialized `StatementIndex=1423`, an
  `EX_Jump` into the `AkGameplayStatics:IsEditor` / display-name branch.
  It is not the actor-spawn branch.
- Runtime `EntryPoint=15` maps to serialized `StatementIndex=15`, beginning
  with `GameplayStatics:GetPlayerPawn`. The dispatcher contains two concrete
  chains of `BeginDeferredActorSpawnFromClass` followed by
  `FinishSpawningActor`.
- The two spawn class arguments are package-import references (`FPackageIndex
  -1` and `-3`) that the current manually mounted CUE4Parse context does not
  resolve to names. No class identity is inferred.
- `WVF_Actor_C::ReceiveBeginPlay` calls
  `ExecuteUbergraph_WVF_Actor(3903)`; `On Event Watcher` calls entry `3908`.
- Runtime `EntryPoint=3903` maps to serialized `StatementIndex=3903`; that
  branch performs byte-state checks and reaches
  `KismetMaterialLibrary:SetScalarParameterValue`. This is a material
  parameter path, not yet proof of visible weapon-FOV ownership.
- `WVF_Actor_C::UserConstructionScript` binds `On Event Watcher` and contains
  `GunAK74_ST-20.wvf`, `GunM16_ST-40.wvf`, `all-0.wvf`,
  `BlueprintFileUtils:FindFiles`, string-to-double conversion and Blueprint
  map insertion.

Current classification:

```text
Package/Kismet deserialization       SUCCESS
Concrete lifecycle/call edges        SUCCESS
Spawn class identity                 UNRESOLVED
Visible weapon-FOV ownership         UNPROVEN
Runtime causal proof                 OPEN
Production implementation            NONE
```

## Approved scope

- Modify only `research/tools/CUE4ParseWVF/Program.cs` and this plan.
- Reuse the existing CUE4Parse version and runtime-generated `.usmap`.
- Inspect only `/Weapon_Viewmodel_FOV/WVF` and
  `/Weapon_Viewmodel_FOV/WVF_Actor` package data.
- Enumerate export fields/properties/methods and serialized function data to
  locate Kismet bytecode, script arrays, UberGraph frame data or equivalent
  instruction storage.
- Decode or dump bounded context around EP `15`, `1423` and `3903` if the
  representation can be recovered.

## Explicit non-goals

- No game launch or UE4SS runtime test.
- No production ASI/source/release changes.
- No broad N-container dump, generic package crawler or unrelated assets.
- No semantic assignment based on names or entry-point numbers alone.
- No guessed bytecode offsets, fake `.usmap` mappings or manual property
  values.

## Expected files or areas

- `research/tools/CUE4ParseWVF/Program.cs`
- `research/tools/CUE4ParseWVF/CUE4ParseWVF.csproj` only if a package API
  compatibility adjustment is strictly required; otherwise untouched.
- `research-artifacts/batch2-cue4parse/` for static output and evidence.

## Batches and validation

### Batch 1 — Function serialization inventory

Inspect the current `IoPackage` function exports and reflection surface for
bytecode-bearing fields, properties, nested objects and serialization methods.
Run against the exact WVF package with the existing mapping file.

### Batch 2 — Targeted EntryPoint mapping

If bytecode or equivalent instructions are available, dump only the bounded
context around EP `15`, `1423` and `3903`, including direct call/object/name
references.

Completed: all three entry points now map to serialized statements and direct
call edges. The spawn class imports remain unresolved in the current direct
IoStore context.

### Batch 3 — Static result classification

Classify the outcome as:

```text
SUCCESS  — at least one EntryPoint maps to a concrete instruction/call edge
PARTIAL  — function data is decoded but the target edge is unresolved
DEFERRED — CUE4Parse exposes no usable bytecode representation
```

Batch result: `SUCCESS` for the bounded EntryPoint-to-call-edge objective;
spawn class identity and visible framing ownership remain open for a separate
future batch.

## Risks and rollback / safe-failure behavior

- Preserve the existing working transport/mount path.
- Keep all reflection probes guarded and report exceptions with type/message.
- Do not reinterpret empty `ScriptBytecode` as empty Blueprint logic.
- If the package representation is not exposed by the installed CUE4Parse API,
  stop and record the exact missing API rather than guessing serialization.
- Preserve all existing research outputs and user changes.

## Stop conditions and phase gates

- Stop when one concrete EntryPoint → Kismet instruction/call edge is mapped.
- This stop condition is met. No further game launch is authorized by this
  batch.
- Stop and classify `DEFERRED` if the bounded reflection inventory cannot find
  serialized bytecode or an equivalent representation.
- Do not return to runtime testing in this batch.
- Do not implement an ASI change from static evidence alone.

## Expected final Git review

Perform a read-only review of the exact tool, plan and research-artifact paths.
Report completed, remaining, deferred, blocked and not-runtime-validated
items. Do not stage or commit.
