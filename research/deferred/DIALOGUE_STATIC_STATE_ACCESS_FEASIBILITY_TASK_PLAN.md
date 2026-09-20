# Dialogue static-state standalone access feasibility

## Objective

Perform a read-only current-image reverse-engineering audit of the UE4SS-confirmed
`APC::IsInStaticDialog()` state query. Establish, if possible, its native
implementation or existing call path and evaluate safe standalone ASI observation
strategies without changing production code.

## Established evidence and current state

- UE4SS CXXHeaderDump declares `bool APC::IsInStaticDialog();`.
- UE4SS runtime observation confirmed `false -> true -> false` across one tested
  static dialogue lifecycle.
- Current Steam 2.0.5 executable identity is established: SHA-256
  `E7B481A97C02D80581FAB0BECE940214A88EBE30211088A00129845A039F9293`, image
  base `0x140000000`, `.text` size `0x7CCD000`.
- The standalone ASI has no validated UObject, ProcessEvent, player-object, or
  native UE-function invocation bridge.

## Approved scope

- Read-only CXXHeaderDump, current executable metadata and matching Ghidra project.
- One read-only Ghidra helper that locates the exact string/name evidence and prints
  references, candidate functions, and direct callers without changing analysis.
- Static comparison of direct-field, native-call, native-observation and reflection
  access paths.

## Explicit non-goals

- No production-source, config, hook, resolver or Dialogue-classifier changes.
- No ASI or UE4SS probe build.
- No game launch or new runtime testing.
- No guessed RVA, object layout, field offset, ABI or ProcessEvent invocation.
- No investigation of FOV heuristics, `[RSI+0x2C]`, HorPlus lifecycle, hotkeys or
  dialogue coverage beyond the already-tested static-dialogue path.

## Expected files or areas

- `02-Research/Ghidra/ghidra-scripts/` for one read-only helper.
- `02-Research/Ghidra/run-find-is-in-static-dialog-native-path-205.cmd` for its
  tracked `-readOnly -noanalysis` launcher and evidence capture.
- `research/active/` for this plan; a report only if a durable result is established.
- Existing source and game files remain read-only.

## Batches

### Batch 1 — Identity and metadata

Reconfirm executable identity and inspect the CXXHeaderDump declaration and UE4SS
provenance.

Validation: SHA-256, image base, `.text` size, section layout and known current
writer anchor evidence must agree before static interpretation.

### Batch 2 — Native implementation/call-path search

Run the read-only Ghidra helper against the matching program. Search exact
`IsInStaticDialog` name/string evidence, associated registration/name references,
candidate native functions and direct callers.

Validation: report only exact references and decoded locations; ambiguous matches
are not treated as the implementation.

### Batch 3 — Access-path classification

Classify direct field read, resolved native call, existing native transition
observation and UE reflection bridge by feasibility, safety, invasiveness, patch
resilience, APC acquisition and fail-closed behavior.

## Risks and safe-failure behavior

- A reflected declaration may not map directly to a discoverable native symbol.
- A string or name reference is not function ownership evidence.
- The audit stops rather than inferring field offsets, object chains, ABI, or native
  callability from incomplete evidence.
- The helper is read-only and must not rename functions, write analysis, or mutate
  the executable.

## Stop conditions and phase gates

- Stop all static interpretation if executable identity fails.
- Stop at `NOT ESTABLISHED` if an exact implementation/call path cannot be
  distinguished.
- Do not proceed to production design or implementation unless a standalone access
  path is validated statically and remains fail-closed.

## Final review

Review changed paths against this plan, inspect the read-only Git status/diff where
available, and report confirmed, statically established, not established, deferred
and not-runtime-validated conclusions. Archive the plan only after the audit is
complete.
