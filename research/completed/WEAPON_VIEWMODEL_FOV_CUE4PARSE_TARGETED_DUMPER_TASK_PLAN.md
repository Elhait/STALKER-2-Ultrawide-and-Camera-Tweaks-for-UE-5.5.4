# Weapon Viewmodel FOV — Targeted CUE4Parse Dumper

## Objective

Read the disposable standard Weapon Viewmodel FOV N IoStore container directly with CUE4Parse and recover bounded package evidence for `/Weapon_Viewmodel_FOV/WVF`, prioritizing imports, exports, direct dependencies and, only if serialization succeeds, `WVF_C` Kismet bytecode around `DeferredActorSpawnFromClass`.

## Established evidence and current state

- Component isolation established `N + O` as the minimum tested working set for post-cinematic framing repair.
- N contains `WVF`, `WVF_Actor`, `Weapon_Viewmodel_FOV` and all profile assets.
- UAssetToolRivals v1.5.6 indexed the game and N container but extraction was blocked by its Oodle integration; this does not test STALKER serialization.
- retoc 0.1.5 listed N but produced no usable legacy assets.
- Exact-name UE4SS discovery did not expose reflected WVF types.
- Direct CUE4Parse work progressed beyond container loading: the WVF export data chunk was identified (`chunk type 1`, index `0`, `5839` bytes), global data was loaded from the game's `global.utoc`, and `IoPackage` now deserializes with the UE4SS-generated mapping file. The package exposes four exports: `WVF_C`, `ExecuteUbergraph_WVF`, `OnWorldBeginPlay` and `Default__WVF_C`; `WVF_C.UberGraphFunction` resolves explicitly to `ExecuteUbergraph_WVF`. The generated mapping file was found from `UE4SS.log`, copied to the research artifacts directory, and hashed. This is a successful package/export gate. `UFunction.ScriptBytecode` remains empty in the current bounded dump, so Kismet recovery is still pending; this is not yet a concrete runtime-owner result.

## Approved scope

- Create a research-only targeted CUE4Parse/.NET dumper.
- Use only the disposable N copy at `research-artifacts/weapon-fov-isolation-backup`.
- Initialize IoStore/Zen provider and Oodle support; locate/load only the WVF package and directly required package data.
- Report tool/package versions, input identity, imports, exports, types/names, direct dependencies and bounded Kismet findings.

## Explicit non-goals

- No changes to production source, ASI, release assets, installed game files or installed reference mod.
- No runtime writes, game launch, UE4SS changes or broad UObject enumeration.
- No Custom Weapons, O-profile matrix, CameraComponent/PCM or AnimInstance research.
- No full Blueprint decompilation or graph reconstruction.

## Expected files or areas

- Research tool and source: `research/tools/CUE4ParseWVF/`.
- Inputs: `research-artifacts/weapon-fov-isolation-backup/Weapon_Viewmodel_FOV_0_N.*`.
- Evidence output: `research-artifacts/batch2-cue4parse/`.
- Durable report only after a reproducible result: `research/reports/` or `research/evidence/`.

## Batches

### Batch 1 — Toolchain gate

Verify .NET and CUE4Parse availability/version and establish the smallest provider initialization path with Oodle support.

Validation: provider starts without modifying game files; exact failure and missing dependency are recorded.

### Batch 2 — Targeted package load

Locate and load only `/Weapon_Viewmodel_FOV/WVF` from N. Enumerate imports, exports, export types/names and direct dependencies.

Validation: `SUCCESS` if the package and serialized exports are readable; `PARTIAL` if package structure loads but exports cannot be serialized; `FAILED/DEFER` on reproducible provider/package failure. Current result: `SUCCESS` with UE4SS-generated mappings. The two unrelated unknown-format warnings from installed mod `.pak` files do not prevent the direct N-container path.

### Batch 3 — Conditional Kismet inspection

With Batch 2 now successful, inspect `WVF_C`/`ExecuteUbergraph` for serialized script/Kismet data, `DeferredActorSpawnFromClass` and the concrete class/object input if serialized evidence supports it. The first bounded probe recovered the explicit `UberGraphFunction` reference but no populated `ScriptBytecode`; this remains `PARTIAL`.

Validation: concrete runtime anchor, unresolved partial bytecode, or reproducible serialization failure.

## Risks and rollback / safe-failure behavior

- CUE4Parse version, UE5.5.4 Zen format, Oodle and unversioned properties may be incompatible. Stop at the first reliable boundary and preserve diagnostics.
- Research source/output is disposable and isolated from production. No cleanup or deletion beyond exact newly-created research output is authorized.
- Static package evidence does not prove runtime execution or visual ownership.

## Stop conditions and phase gates

- Stop when a concrete runtime anchor is recovered or when CUE4Parse cannot reliably load/serialize the target package.
- Do not guess Blueprint semantics from names or partial bytecode.
- Production implementation requires a separate plan and explicit authorization.

## Expected final Git review

Perform a read-only Git review of all changed paths against this plan. Report completed, remaining, deferred, blocked and not-runtime-validated items. Do not stage or commit.
