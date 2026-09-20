# Wideboy ADS lifecycle diagnostic task plan

## Objective

Build one diagnostic-only ASI that observes the two identity-gated Wideboy v10
ADS candidate boundaries in Steam 2.0.5 and records read-only edge telemetry.

## Established evidence and current state

- Steam 2.0.5 identity gate passed: SHA-256
  `E7B481A97C02D80581FAB0BECE940214A88EBE30211088A00129845A039F9293`,
  image base `0x140000000`, `.text` size `0x7CCD000`.
- ADS IN signature is unique at RVA `0x604EEC`; ADS OUT is unique at RVA
  `0x605092`.
- Both are in `FUN_140604C0A` with paired `[RAX+0x4C]/[RAX+0x50]` and
  `[RSI+0x138]/[RSI+0x13C]` topology.
- Exact ADS lifecycle semantics remain runtime-unvalidated.

## Approved scope

- Add compile-time diagnostic signatures and a fail-closed resolver.
- Install two non-mutating `SafetyHookMid` observers after validating the
  identity hash, unique matches, and the post-load `UCOMISS` bytes.
- Log change-driven IN/OUT records with register/object identities, paired
  fields, and already available FOV values.
- Build a separate diagnostic ASI artifact.
- Run static harness/build validation only; the user performs the game run.

## Explicit non-goals

- No Dialogue classifier or hotkey changes.
- No HorPlus, AspectRecalculation, Cinematics, or camera/FOV writes.
- No state mutation, replay, suppression, or production hook changes.
- No game launch or runtime test by the agent.

## Expected files or areas

- `src/plugin/runtime.cpp`
- `src/hooks/signatures/signature_definitions.hpp`
- `build-wideboy-ads-diagnostic.cmd`
- `research/reports/ADS_LIFECYCLE_WIDEBOY_V10_STATIC_AUDIT_205.md`

## Batches and validation

### Batch 1 — Diagnostic resolver and observer

Add the guarded observer and compile-time definitions. Validate with the
existing test harness and `git diff --check`.

### Batch 2 — Diagnostic build

Build only the separate Wideboy diagnostic ASI. Confirm the artifact name and
that the normal production output is not replaced.

### Final review

Inspect status and diff against this plan. Report source/build/runtime status
separately. Runtime remains user-operated and not validated by this task.

## Risks and safe failure

- Any hash mismatch, non-unique signature, or instruction mismatch rejects
  installation and leaves native behavior untouched.
- The observers never write registers or game memory.
- Logging is bounded to changes in the observed edge snapshot; repeated hits
  are counted but not emitted as per-hit records.
- Hook teardown occurs through the existing runtime resource reset path.

## Stop conditions and phase gates

- Stop if the current executable identity does not match.
- Stop if either signature is ambiguous or the validated post-load bytes do
  not match.
- Stop before runtime if build or static checks fail.

## Expected final Git review

Only the guarded diagnostic implementation, its build wrapper, and this
research plan/report may be attributed to this task. Existing user changes and
unrelated dirty paths must remain untouched.
