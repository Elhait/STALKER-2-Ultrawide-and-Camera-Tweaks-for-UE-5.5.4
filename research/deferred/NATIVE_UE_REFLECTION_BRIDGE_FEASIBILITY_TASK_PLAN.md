# Native UE Reflection Bridge Feasibility — TanFOV Access

## Objective

Determine whether the existing standalone ASI architecture can establish a
guarded, patch-resilient native correspondence path to the UE reflection
machinery required for `MPC_FOV.TanFOV`, without guessing RVAs, UObject
layouts or callable addresses.

## Established evidence and current state

- `MPC_FOV.TanFOV → visible weapon/viewmodel framing` is causally confirmed
  through one controlled UE4SS intervention with readback.
- UE4SS can resolve, read and write the exact MPC parameter.
- The stable standalone ASI has no existing safe bridge for `UObject`,
  `UFunction`, `StaticFindObject` or `ProcessEvent`.
- Repair timing/value research is deferred until an independent access seam
  exists.
- Native offsets, guessed object layouts and undocumented handle invocation
  are not evidence and are out of scope.

## Approved scope

- Offline read-only audit of the current executable identity, existing ASI
  resolver infrastructure and available UE reflection metadata/correspondence.
- Identify whether a guarded correspondence can be built for:

```text
UObject/UClass identity
UFunction identity
MPC_FOV asset identity
KismetMaterialLibrary getter/setter identity
safe invocation mechanism
```

- Reuse only existing validated resolver/decoder patterns where applicable.
- Record the exact executable/version provenance for every static conclusion.

## Explicit non-goals

- No `ProcessEvent` call, native invocation or `TanFOV` write.
- No guessed RVA, vtable slot, UObject offset or property layout.
- No production ASI/source/release modification.
- No game launch or UE4SS runtime test.
- No repair timing/value work and no Custom Weapon FOV work.

## Expected files or areas

- Existing standalone ASI source and resolver infrastructure, read-only.
- Existing Ghidra/static-analysis provenance, only after executable identity
  passes the workspace hard gate.
- This plan and a bounded research report if a result is established.
- No production files should be modified.

## Batches and validation

### Bridge 1 — Architecture inventory

Confirm whether the current ASI already contains any reusable UE reflection
entry point, global object registry correspondence, or validated native
function-call abstraction. Reconcile against the prior reflection audit.

#### Bridge 1 result

The current standalone ASI/research C++ contains SafetyHook/Zydis instruction
hooks and game-specific native traces, but no grounded UE reflection entry
point, UObject registry correspondence or validated invocation abstraction.
The existing reflection audit remains consistent:

```text
Existing UObject/UClass bridge        NONE
Existing UFunction bridge             NONE
Validated ProcessEvent path           NONE
Validated call-handle ABI             NONE
```

No new Ghidra search was started because the architecture inventory provided
no grounded entry point to guide it. Bridge 2 is therefore not authorized by
this plan.

Result: `DEFERRED` under the early Bridge 1 stop condition.

### Bridge 2 — Native correspondence feasibility

Only if Bridge 1 identifies a grounded path, establish read-only identities for
the required UE objects/functions against the current executable. Require image
identity, section layout and known-anchor validation before static conclusions.

### Bridge 3 — Classification

```text
PROMISING  — guarded read-only correspondence for the required UE machinery
             is established without guessed addresses/layouts.
PARTIAL    — metadata correspondence exists, but callable/access semantics
             remain unproven.
DEFERRED   — the path requires guessed RVAs/layouts or a new unvalidated
             reflection bridge.
REJECTED   — current standalone ASI architecture cannot provide a safe seam
             within the supported executable contract.
```

## Risks and rollback / safe-failure behavior

- Static analysis must stop on image-identity mismatch or unknown provenance.
- No runtime mutation is allowed in this batch.
- A successful pattern match is not function-call proof.
- Any ambiguity in object/function identity fails closed and remains deferred.
- Production source and release artifacts must remain untouched.

## Stop conditions and phase gates

- Stop after the architecture inventory if no existing reflection entry point
  is present.
- Stop before any invocation attempt; invocation is a later, separately
  approved batch.
- Treat guessed offsets, opaque undocumented wrappers and mismatched images as
  blockers, not implementation candidates.
- Do not reopen repair timing/value research until this bridge reaches
  `PROMISING`.

## Expected final Git review

Perform a read-only review of any research report and this plan. Report
completed, remaining, deferred, blocked and not-runtime-validated items. Do
not stage or commit.
