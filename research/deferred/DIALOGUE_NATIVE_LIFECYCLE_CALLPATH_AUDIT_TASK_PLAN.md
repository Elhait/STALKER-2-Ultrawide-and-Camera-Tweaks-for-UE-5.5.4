# Dialogue native lifecycle call-path audit — task plan

## Objective

Perform a bounded read-only Steam 2.0.5 static audit of the reflected `XPlayDialog*` command anchors, their executable evidence and any convergence toward `UDialogManager` or the state queried by `APC::IsInStaticDialog()`.

## Established evidence and current state

- `APC::IsInStaticDialog()` is runtime-confirmed as an oracle for the tested static-dialogue path.
- Standalone ASI access to that query is not established.
- `UDialogManager` is present in the UE4SS dump but exposes no lifecycle methods there.
- `XPlayDialogLine`, `XPlayDialogFromPool`, `XRestartCurrentDialog` and `XClearDialogQueue` are reflected console/debug anchors only; native ownership is not established.
- The current production Dialogue hook is a generic FOV boundary and remains unchanged.
- Steam 2.0.5 identity: SHA-256 `E7B481A97C02D80581FAB0BECE940214A88EBE30211088A00129845A039F9293`, image base `0x140000000`, `.text` size `0x7CCD000`.

## Approved scope

- Read-only inspection of the identity-matching Ghidra program.
- Search exact reflected names and bounded executable references/callers.
- Record native entry/callsite, direct downstream calls, object/register provenance where the database supports it, and confidence.
- Compare convergence among the four `XPlayDialog*` anchors and `UDialogManager` evidence.
- Produce a factual report in `research/reports/`.

## Explicit non-goals

- No production C++ changes.
- No ASI build or injection.
- No game launch or runtime test.
- No new diagnostic hook.
- No broad FOV/RSI/UI/subtitle/audio/animation scan.
- No claim that reflected names or arbitrary bool writes are ownership seams.

## Expected files/areas

- `02-Research/Ghidra/ghidra-scripts/FindDialogueNativeLifecycleAnchors205.java`
- `02-Research/evidence/dialogue-native-lifecycle-anchors-205-headless.txt`
- `research/reports/DIALOGUE_NATIVE_LIFECYCLE_CALLPATH_AUDIT_205.md`

## Batches and validation

### Batch 1 — identity-gated anchor search

Search exact ASCII names for `XPlayDialogLine`, `XPlayDialogFromPool`, `XRestartCurrentDialog`, `XClearDialogQueue` and `UDialogManager`. Print all references, containing functions and direct callers where available.

Validation: identity must be `PASS`; non-matching program images must be skipped.

### Batch 2 — bounded convergence inspection

Compare the recovered executable evidence for common callees, manager/vtable references and state-writer candidates. Stop paths when they leave the Dialogue-related subsystem without supported semantic evidence.

Validation: report evidence type and confidence for every claimed edge.

### Batch 3 — final report and review

Write the required report with `CONFIRMED`, `STATICALLY ESTABLISHED`, `CANDIDATE` and `NOT ESTABLISHED` classifications, then run `git diff --check` and a read-only status/diff review.

## Risks and safe failure

- Reflected names may have no executable references; classify them as metadata-only.
- Ghidra may expose an incomplete or indirect call graph; do not infer absence of a path from missing XREFs alone.
- If the identity gate fails or the project is locked by another process, stop without interpreting results.
- If no convergence is established, preserve the direct-state path as deferred and do not broaden the search.

## Stop conditions and phase gates

- Stop before analysis on any identity mismatch.
- Stop after ranks 2–3 fail to yield a validated convergent anchor.
- Do not propose runtime instrumentation unless a specific candidate ENTER/EXIT seam is established.

## Expected final Git review

Confirm only the approved research plan, helper, evidence output and report are attributable to this batch; production source, build outputs and runtime configuration must remain untouched.
