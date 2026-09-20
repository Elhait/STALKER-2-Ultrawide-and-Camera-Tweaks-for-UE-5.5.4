# Weapon Viewmodel FOV — Cooked Blueprint Batch 2

## Objective

Recover concrete serialized Blueprint/Kismet evidence from the standard Weapon Viewmodel FOV reference package, limited to `WVF.WVF_C`, with the goal of identifying the class argument passed to `DeferredActorSpawnFromClass` or another concrete runtime object/function anchor.

## Established evidence and current state

- The standard reference mod preserves correct post-cinematic weapon framing only with the tested `N + O` component set.
- `N` contains the compiled reference implementation and profile assets; `O` supplies the profile marker (`all-0.wvf`, with `all-20.wvf`/`all-40.wvf` behaviorally affecting magnitude).
- Exact cooked Blueprint serialization and the spawned runtime helper remain unknown.
- Exact-name UE4SS discovery did not expose reflected `WVF`/`WVF_Actor`/`Weapon_Viewmodel_FOV` types.
- A local `retoc 0.1.5` exists. UAssetToolRivals is not currently present in the workspace.
- `retoc 0.1.5` can list the N container and exposes the target WVF/profile paths, but its `to-legacy --version UE5_5 --filter WVF` conversion produced `0 extracted / 2 failed`; an exact `WVF.uasset` filter produced `0 extracted / 1 failed`. No usable legacy asset was produced.
- UAssetToolRivals v1.0.0 (release v1.5.6) was installed from the official Windows archive; the archive SHA-256 matched `16c051cbc68bef0b9050ca83a8fd3d8d997156ed1e91f4112042f41443bdabaf`.
- UAssetToolRivals indexed the game and N mod container (`51317` script objects, `279104` packages) and matched 21 WVF packages. Extraction was skipped for all 21 because its Oodle loader attempted a network download and did not accept the locally colocated `oo2core_9_win64.dll`; the DLL itself loads successfully in an independent check. No legacy assets were produced.

## Approved scope

- Research-only inspection of a disposable copy of the standard `N` package.
- Tool order: UAssetToolRivals if supplied/approved, targeted CUE4Parse dumper if needed, UAssetGUI for converted legacy assets, `retoc` as fallback.
- Inspect only the `WVF.WVF_C` package path and directly required imports/exports/dependencies.
- Record reproducible tool versions, input paths, package identity and outcomes.

## Explicit non-goals

- No changes to `STALKER2CameraTweaks.asi`, production source, release artifacts or installed reference files.
- No Custom Weapons analysis.
- No further O-profile matrix, CameraComponent/PCM research, generic UObject traversal or AnimInstance research.
- No full reference-mod decompilation.
- No runtime value writes or game launch in this batch.

## Expected files or areas

- Input: `research-artifacts/weapon-fov-isolation-backup/Weapon_Viewmodel_FOV_0_N.*` or an explicitly supplied equivalent research copy.
- Disposable output: a new research artifact directory outside the installed game and production source.
- Evidence: `research/evidence/` or `research/reports/` only if a durable result is obtained.
- This plan moves to `research/completed`, `research/deferred` or `research/rejected` after the gate is resolved.

## Batches

### Batch 2.1 — Tool and package gate

Check the supplied/approved UAssetToolRivals or CUE4Parse path and inspect available `retoc` capabilities. Do not alter production or installed files. The local `retoc` gate is now tested and failed at conversion, while container listing succeeded.

Validation: tool version/help output captured; exact input package identified; container indexing succeeded; legacy extraction is deferred due to a reproducible Oodle integration blocker, not a confirmed STALKER serialization failure.

### Batch 2.2 — Targeted extraction and serialization inspection

Using the first working tool, extract or read only `WVF.WVF_C` and required direct dependencies. Inspect exports, imports, properties and Kismet bytecode around `ExecuteUbergraph` and `DeferredActorSpawnFromClass`.

Validation: one of `SUCCESS` (concrete spawn/helper class or runtime anchor), `PARTIAL` (Blueprint decoded but class unresolved), or `FAILED/DEFER` (package cannot be decoded reliably).

Current gate result: `FAILED/DEFER` for the available UAssetToolRivals path. No Blueprint/Kismet evidence was recovered.

### Batch 2.3 — Cross-check and evidence review

Cross-check any recovered class/path against package names and, only if the result is concrete and safe, prepare a bounded future UE4SS lookup. Do not execute runtime research in this batch.

Validation: evidence is tied to the exact package/tool/version and clearly separates confirmed data from hypotheses.

## Risks and rollback / safe-failure behavior

- Zen/IoStore, Oodle, unversioned properties or UE5.5.4 Blueprint serialization may be unsupported. Stop and preserve the failure output; do not patch production.
- All extraction and conversion must target a disposable research copy. Delete nothing from the installed game or canonical source.
- A successful extraction is not proof of runtime execution or ownership.

## Stop conditions and phase gates

- Stop before any production change regardless of parser outcome.
- Stop on unsupported or ambiguous serialization rather than guessing bytecode semantics.
- Stop after a concrete class/runtime anchor is recovered, or after a reproducible parser failure establishes `DEFERRED`.
- Production implementation requires a separate plan and explicit scope.

## Expected final Git review

Perform a read-only repository review of changed paths against this plan. Report completed, remaining, deferred, blocked and not-runtime-validated items. No commit or staging is authorized.
