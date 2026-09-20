# Weapon Viewmodel FOV — MPC TanFOV Downstream Consumer Batch 4

## Objective

Resolve the downstream static consumers of
`/Game/_Stalker_2/Materials/MPC/MPC_FOV.MPC_FOV` parameter `TanFOV` and
determine whether the parameter participates in first-person weapon/viewmodel
projection or only in an unrelated material effect.

## Established evidence and current state

- Vanilla cinematic EXIT leaves weapon framing broken; vanilla ADS repairs it.
- `Weapon Viewmodel FOV - 0` preserves correct post-cinematic framing.
- `WVF_C::ExecuteUbergraph_WVF(15)` statically spawns both
  `S2Dev_Event_Watcher_C` and `WVF_Actor_C`.
- `WVF_Actor_C` discovers `.wvf` profiles, parses numeric values and builds
  profile maps.
- Profile-map lookup plus viewport/FOV math reaches eight static
  `SetScalarParameterValue` call sites.
- All eight calls target
  `/Game/_Stalker_2/Materials/MPC/MPC_FOV.MPC_FOV` with parameter `TanFOV`.
- The downstream material/rendering consumer and visible weapon-framing
  ownership are not proven.

## Approved scope

- Perform offline static inspection only.
- Search the available cooked game/reference asset indexes and package
  metadata for references to `MPC_FOV` and `TanFOV`.
- Load only bounded candidate material, material-function and first-person
  weapon/viewmodel assets needed to establish a direct reference edge.
- Record exact package paths, object names, expression types and dependency
  edges.
- Keep the existing runtime-generated `.usmap` and provider-backed CUE4Parse
  path.

## Explicit non-goals

- No game launch, UE4SS probe or runtime hook.
- No production ASI/source/release changes.
- No return to WVF profile parsing, spawn identity, Camera/PCM, AnimInstance,
  F_99_P curves or O-profile behavior.
- Do not infer material ownership from asset names alone.
- Do not claim `TanFOV` controls weapon framing without a direct material or
  runtime ownership edge.

## Expected files or areas

- `research/tools/CUE4ParseWVF/Program.cs` only if a bounded static search
  capability is required.
- This plan file.
- No production files or release artifacts.

## Batches and validation

### Batch 4.1 — Parameter and material-graph references

Locate cooked references to `MPC_FOV` and `TanFOV`. Determine whether the
parameter is consumed by material expressions, material functions, collection
parameter nodes or other serialized material state.

#### Batch 4.1 result

The provider-backed index resolves the collection asset itself:

```text
Stalker2/Content/_Stalker_2/Materials/MPC/MPC_FOV.uasset
```

The available `DefaultFileProvider` exposes forward package-reference
inspection (`ScanForPackageRefs`) but no reverse-reference or dependency-index
API for asking which cooked materials reference this collection. The bounded
index search found only the collection asset path; it did not recover any
consumer edge. A full cooked-material crawl would exceed this plan's scope and
would produce unbounded noise.

Result: `PARTIAL/DEFERRED` for 4.1. `MPC_FOV.TanFOV` remains a confirmed write
target, but its downstream material consumer is not recoverable from the
available reverse-reference metadata.

### Batch 4.2 — First-person/viewmodel classification

For each resolved consumer, compare its package/object path against known
first-person weapon/viewmodel assets and record direct dependency evidence.
Classify the result as shader-side viewmodel projection, unrelated material
effect, or unresolved.

Status: not started because Batch 4.1 did not produce a bounded consumer set.

### Batch 4.3 — Static classification

```text
SUCCESS  — direct TanFOV consumer and first-person/viewmodel relationship are
            both recovered.
PARTIAL  — TanFOV consumer is recovered but its rendering ownership remains
            unresolved.
DEFERRED — available cooked metadata cannot recover downstream references
            reliably.
```

The batch stops at the first concrete downstream edge; it must not expand into
unbounded material or shader reverse engineering.

## Risks and rollback / safe-failure behavior

- Keep package loading and reflection bounded and exception-safe.
- Preserve the working provider-backed IoStore mount and direct-reader
  fallback.
- Treat missing reverse references, null dependencies and parser failures as
  tooling limitations, not gameplay evidence.
- If the available package index cannot provide reverse references, record the
  exact limitation and stop rather than scanning all cooked assets blindly.
- No production rollback is needed because production paths are out of scope.

## Stop conditions and phase gates

- Stop when a direct `TanFOV` consumer is recovered and classified, or when
  the available cooked metadata is demonstrably insufficient.
- Do not run runtime validation in this batch.
- Do not propose ASI implementation until a separate targeted runtime check
  confirms the static ownership candidate affects visible framing.

## Expected final Git review

Perform a read-only review of the bounded research tool, this plan and any
research evidence paths. Report completed, remaining, deferred, blocked and
not-runtime-validated items. Do not stage or commit.
