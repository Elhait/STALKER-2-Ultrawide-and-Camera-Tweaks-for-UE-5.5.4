# Weapon Viewmodel FOV — MPC TanFOV Causal Validation Batch 5

## Objective

Determine whether the confirmed `MPC_FOV.TanFOV` write path controls visible
post-cinematic weapon/viewmodel framing, using only one of two bounded routes:

1. an addressable static method for recovering direct consumers of the
   `MPC_FOV` collection or `TanFOV` parameter; or
2. one targeted causal runtime check that can distinguish visible-framing
   ownership from an incidental material effect.

## Established evidence and current state

- Vanilla cinematic EXIT produces broken weapon framing; vanilla ADS repairs
  it.
- `Weapon Viewmodel FOV - 0` preserves correct post-cinematic framing.
- `WVF_C::ExecuteUbergraph_WVF(15)` spawns
  `S2Dev_Event_Watcher_C` and `WVF_Actor_C`.
- `WVF_Actor_C` parses `.wvf` profiles and builds profile maps.
- Profile selection plus viewport/FOV math reaches eight static
  `SetScalarParameterValue` call sites.
- All eight writes target
  `/Game/_Stalker_2/Materials/MPC/MPC_FOV.MPC_FOV`, parameter `TanFOV`.
- The downstream consumer of `TanFOV` and visible-framing ownership remain
  unknown.
- Batch 4.1 reached the documented CUE4Parse reverse-reference boundary;
  broad material crawling is rejected.

## Falsifiable hypothesis

```text
If MPC_FOV.TanFOV controls visible weapon/viewmodel framing, then a controlled
mutation of that specific active collection parameter at the relevant
post-EXIT boundary must produce a corresponding, reproducible framing change
while the weapon mesh and AnimScriptInstance remain otherwise equivalent.

```

Observation alone is not causal evidence. A runtime success requires:

```text
known state
  → controlled TanFOV mutation
  → confirmed active-parameter/state change
  → corresponding visible framing change
  → no weapon/AnimScriptInstance lifecycle change
```

The alternative outcome is that `TanFOV` is incidental or unrelated to the
visible framing mechanism.

## Approved scope

- First evaluate whether a bounded static route can recover direct consumers
  without scanning all cooked materials.
- If no such route exists, design and run at most one targeted runtime causal
  check centered on `MPC_FOV.TanFOV`.
- Use the already established WVF_0/vanilla behavioral anchors only as control
  context; do not repeat their visual A/B tests.
- Preserve stable production source, ASI and release artifacts.

## Explicit non-goals

- No broad material or shader crawl.
- No new generic UObject enumeration.
- No return to CameraComponent/PlayerCameraManager, AnimInstance fields,
  O-profile matrix or F_99_P curves.
- No production implementation, hook promotion or release change.
- No claim that a material parameter controls weapon framing from naming or
  upstream writes alone.

## Expected files or areas

- A new bounded research-only static helper or runtime probe, only if required
  by the selected route.
- This plan and its evidence notes.
- No production files or release artifacts.

## Batches and validation

### Batch 5.1 — Bounded static feasibility

Check only direct, addressable metadata sources for reverse consumers of
`MPC_FOV`/`TanFOV` (for example an existing dependency table, asset-registry
record or package-level reverse index). If none is available, record the exact
limitation and stop this route.

#### Batch 5.1 result

No installed `AssetRegistry.bin` or equivalent bounded asset-registry file was
found under the game directory. The current CUE4Parse provider exposes
forward `ScanForPackageRefs(GameFile)` but no reverse-reference/dependency
index for locating all cooked consumers of `MPC_FOV`. This confirms the Batch
4.1 tooling boundary; no material-wide crawl was performed.

Result: `DEFERRED` for 5.1. The next eligible route is the single causal
intervention in 5.2, subject to a separate execution decision.

### Batch 5.2 — One causal runtime check, only if 5.1 is unavailable

Use one research-only targeted probe with a clean control and one predetermined
parameter mutation. Independently confirm that the write reached the active
MPC parameter/state before interpreting the visual result. The result must
directly classify:

The prepared disposable probe is
`research/ue4ss/MPC_TanFOVCausalProbe/Scripts/main.lua`. It binds to F9,
resolves the exact collection and `TanFOV`, reads the baseline, writes
`baseline × 1.25` (with a small fallback delta only for a zero baseline),
reads the value back, and records weapon/AnimScript identity before and after.
It does not poll or restore the value. It has not been installed into the game
or runtime-validated yet.

```text
TanFOV controls visible framing
OR
TanFOV is incidental/unrelated
```

The probe must not modify the production ASI or stable gameplay/cinematic
implementation. Any unsafe access or ambiguous visual result is a failed
instrumentation/validation attempt, not gameplay evidence.

#### Batch 5.2 result

The single F9 intervention completed with independent readback:

```text
TanFOV.Before   = 1.678197145462
TanFOV.Requested= 2.0977464318275
TanFOV.After    = 2.0977463722229
MutationState   = CONFIRMED
```

The weapon mesh and `AnimBP_pm_fp_C` AnimScriptInstance full names were
identical before and after the write. The user observed a distinct framing
change immediately after F9, before opening the menu. A later third framing
change occurred after the menu opened; that is a separate refresh/reapply
event and does not weaken the confirmed F9 intervention result.

Result: `SUCCESS` for causal control of visible framing by the active
`MPC_FOV.TanFOV` state. This does not yet identify the downstream shader
consumer or authorize production implementation.

### Batch 5.3 — Classification

```text
  SUCCESS  — direct consumer or confirmed TanFOV intervention produces the
             corresponding visible-framing change.
PARTIAL  — TanFOV is located but the mutation/state cannot be independently
           confirmed, or the visual result is instrumentation-inconclusive.
REJECTED — confirmed active TanFOV mutation produces no corresponding framing
           change.
DEFERRED — neither bounded static nor safe targeted runtime validation is
           available without expanding scope.
```

## Risks and rollback / safe-failure behavior

- No production mutation is allowed.
- Runtime mutation, if eventually authorized within 5.2, must be confined to
  a disposable research probe and have a safe no-op/failure path.
- Do not infer causality from a crash, missing object, parser failure or
  unrelated visual change.
- Stop immediately if the proposed method requires broad scanning or a new
  unbounded hook surface.

## Stop conditions and phase gates

- Stop 5.1 when no bounded reverse-reference source is available.
- Run at most one targeted runtime causal check in 5.2.
- Stop after a classified result; do not iterate exploratory variants in the
  same batch.
- No production seam discussion until causal ownership is demonstrated.

## Expected final Git review

Perform a read-only review of any research-only helper, this plan and the
evidence notes. Report completed, remaining, deferred, blocked and
not-runtime-validated items. Do not stage or commit.
