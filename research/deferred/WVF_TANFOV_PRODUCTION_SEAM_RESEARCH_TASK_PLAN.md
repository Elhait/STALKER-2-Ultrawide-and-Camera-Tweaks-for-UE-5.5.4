# Weapon Viewmodel FOV — TanFOV Production-Seam Research

## Objective

Determine how to reproduce the confirmed `MPC_FOV.TanFOV` viewmodel-framing
effect for post-cinematic repair and possible Custom Weapon FOV support,
without depending on the reference WVF Blueprint implementation.

## Established evidence and current state

- Vanilla cinematic EXIT can leave weapon/viewmodel framing broken.
- Vanilla ADS independently repairs the framing.
- `Weapon Viewmodel FOV - 0` preserves correct post-cinematic framing.
- The reference path is statically established:

```text
all-0.wvf
  → profile parsing/map
  → main-hand profile selection
  → viewport/FOV math
  → MPC_FOV.TanFOV
```

- `WVF_C::ExecuteUbergraph_WVF(15)` spawns
  `S2Dev_Event_Watcher_C` and `WVF_Actor_C`.
- Eight static writes target
  `/Game/_Stalker_2/Materials/MPC/MPC_FOV.MPC_FOV`, parameter `TanFOV`.
- One controlled F9 intervention changed the active value from
  `1.678197145462` to `2.0977463722229` with confirmed readback.
- Weapon mesh and `AnimBP_pm_fp_C` identity were unchanged before/after.
- The user observed a corresponding visible framing change immediately after
  the confirmed intervention.
- Therefore `TanFOV → visible weapon/viewmodel framing` is causally confirmed.
- The downstream shader/material consumer is still unknown.

## Approved scope

- Research only; no stable production mutation in the first phase.
- Identify the cleanest independent way to write or reapply active
  `MPC_FOV.TanFOV` from a future ASI/research bridge.
- Determine whether the repair can be expressed as a targeted post-cinematic
  reapply using the confirmed MPC state, without reproducing the full WVF
  Blueprint/profile parser.
- Keep Custom Weapon FOV as a separate feasibility question after the repair
  primitive is understood.

## Explicit non-goals

- Do not modify or rebuild the stable production ASI in this batch.
- Do not integrate a hook based only on the causal probe.
- Do not claim the downstream shader consumer is known.
- Do not infer the correct repair timing from the post-cinematic or menu visual
  transitions alone.
- Do not revive Camera/PCM, AnimInstance, O-profile or F_99_P branches.
- Do not depend on the reference mod for the final implementation.

## Expected files or areas

- Research-only notes or bridge under `research/`.
- Existing stable source remains untouched until a later approved plan.
- No release artifacts.

## Batches and validation

### Seam 1 — Active MPC access feasibility

Establish whether a research-only native/UE4SS bridge can resolve the exact
`MPC_FOV` collection and safely read/write `TanFOV` at runtime, with explicit
readback and no weapon/AnimScript recreation.

#### Seam 1 result

The UE4SS research path is confirmed feasible: the disposable probe resolved
the exact collection and parameter, performed a controlled write and read back
the active value. The stable ASI/research C++ source has no existing safe UE
reflection bridge for `UObject`, `UFunction`, `StaticFindObject` or
`ProcessEvent`; the prior reflection architecture audit classified that path
as blocked without a new native correspondence step.

Result: `PARTIAL/BLOCKED` for production-compatible access. UE4SS access is
confirmed, but no native ASI seam is authorized or established.

### Seam 2 — Post-cinematic repair timing feasibility

Using the confirmed primitive, determine one bounded lifecycle boundary at
which a reapply would be meaningful. This is not permission to guess from
visual timing; the boundary must be tied to an existing confirmed runtime
event or state transition.

### Seam 3 — Research implementation decision

Classify the result:

```text
PROMISING  — independent bridge can safely reapply TanFOV at a bounded
             post-cinematic boundary.
PARTIAL    — active MPC access works, but timing/ownership remains unresolved.
DEFERRED   — only reference WVF runtime can safely provide the needed state.
REJECTED   — independent reapply cannot be made safe or stable.
```

Custom Weapon FOV remains out of scope until the post-cinematic repair
primitive reaches `PROMISING`.

## Risks and rollback / safe-failure behavior

- Stable production source and ASI must remain unchanged.
- Every research write must have explicit readback and a safe no-op path.
- Never mutate the active MPC from an unvalidated broad hook or polling loop.
- If timing or ownership is ambiguous, stop at `PARTIAL`/`DEFERRED`.
- No release or packaging changes are allowed.

## Stop conditions and phase gates

- Stop after active MPC access feasibility is established or blocked.
- Do not implement a production hook until a separate plan is approved.
- Do not add Custom Weapon FOV logic before the repair primitive is bounded.
- Any contradictory runtime evidence reopens the causal validation phase before
  implementation work.

## Expected final Git review

Perform a read-only review of research-only files and the plan. Report
completed, remaining, deferred, blocked and not-runtime-validated items. Do
not stage or commit.
