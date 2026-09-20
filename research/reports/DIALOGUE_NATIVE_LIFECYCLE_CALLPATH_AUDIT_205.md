# Dialogue native lifecycle call-path audit — Steam 2.0.5

Date: 2026-09-18  
Scope: bounded read-only Ghidra audit of reflected `XPlayDialog*` anchors and `UDialogManager`; no production changes, ASI build or runtime launch.

## Identity gate

The audit interpreted only the matching Steam 2.0.5 Ghidra program:

| Field | Expected | Observed | Result |
| --- | --- | --- | --- |
| Executable SHA-256 | `E7B481A97C02D80581FAB0BECE940214A88EBE30211088A00129845A039F9293` | same | PASS |
| Image base | `0x140000000` | same | PASS |
| `.text` size | `0x7CCD000` / `130863104` | same | PASS |

Older 2.0.2, 2.0.3, 2.0.4 and v1.9 program images were present in the project, but the identity-gated script reported `IDENTITY=FAIL` for each and did not interpret them.

Evidence file:
`02-Research/evidence/dialogue-native-lifecycle-anchors-205-headless.txt`

## Executive result

The four `XPlayDialog*` names are present only as reflected metadata records. Their string references point to data/name-registration records with no containing executable function. `UDialogManager` appears as one `.data` name/type record with no Ghidra reference. No executable entry, caller, downstream callee, manager dispatch, state writer or paired ENTER/EXIT topology was recovered from this bounded anchor batch.

The stop condition is therefore met:

```text
XPlayDialog* reflected names
→ metadata only
→ no validated executable anchor
→ no downstream convergence
→ stop; do not broaden into another memory/FOV/UI scan
```

## XPlayDialog executable anchors

| Reflected anchor | String VA | Reference VA | Executable function/caller | Result |
| --- | ---: | ---: | --- | --- |
| `XPlayDialogLine` | `0x1494774E8` | `0x148C55D20` | none | metadata-only |
| `XPlayDialogFromPool` | `0x149419139` | `0x148C55D10` | none | metadata-only |
| `XRestartCurrentDialog` | `0x14943A96B` | `0x148C55D70` | none | metadata-only |
| `XClearDialogQueue` | `0x1494533FD` | `0x148C55BE0` | none | metadata-only |

For each record, Ghidra reported one data reference to the string and no containing function. The references do not establish a callable native implementation, registration-to-function mapping, ABI or downstream call path.

## Downstream convergence

No executable `XPlayDialog*` entry was recovered, so there is no supported caller/callee graph to compare. In particular, this batch did not establish convergence on:

- a common native function;
- a common manager object or vtable dispatch;
- a queue/activation routine;
- a state setter associated with `APC::IsInStaticDialog()`;
- an ENTER/EXIT pair.

This is a bounded negative result, not proof that the game has no such path. It means the reflected names do not provide a usable executable anchor in the current Ghidra reference state.

## `UDialogManager` evidence

The exact `UDialogManager` name was found once at `0x149EEF624` in `.data` with zero Ghidra references. The CXXHeaderDump declaration remains:

```cpp
class UDialogManager : public UBaseTickableManager
{
};
```

No native tick, dispatch, construction, retrieval, queue processing or begin/end method was exposed by this batch. The manager remains a semantic domain candidate only, not a hook target.

## State writer candidates

None reached the evidence threshold. No bool/bitfield/enum write was followed because there was no validated Dialog-related executable path from which to constrain the search. The following remain unproven and must not be promoted:

- arbitrary writes to animation `bInDialog` fields;
- `[RSI+0x2C]` FOV target values;
- fixed targets such as `70`;
- generic FOV descent;
- reflected UI, subtitle or audio declarations.

## APC/state connection

The runtime oracle remains valuable:

```text
APC::IsInStaticDialog()
false → true → false
```

But this batch did not recover its native implementation, a field offset, APC object acquisition, a state writer or a safe standalone call path. The prior feasibility result remains unchanged: direct standalone ASI access is deferred, not rejected.

## ENTER/EXIT topology

No paired native Dialogue ENTER/EXIT topology was found. Consequently there is currently no basis for a fail-closed resolver analogous to the validated Cinematic ENTER/EXIT resolver, and no basis for a targeted runtime diagnostic hook.

## Standalone hook feasibility

| Candidate | Native evidence | Dialog ownership evidence | ENTER/EXIT pairing | Hook feasibility | Status |
| --- | --- | --- | --- | --- | --- |
| `XPlayDialogLine` | reflected string/data record only | none | none | unsafe/unavailable | NOT ESTABLISHED |
| `XPlayDialogFromPool` | reflected string/data record only | none | none | unsafe/unavailable | NOT ESTABLISHED |
| `XRestartCurrentDialog` | reflected string/data record only | none | none | unsafe/unavailable | NOT ESTABLISHED |
| `XClearDialogQueue` | reflected string/data record only | none | none | unsafe/unavailable | NOT ESTABLISHED |
| `UDialogManager` | one unreferenced `.data` record | semantic class name only | none | unsafe/unavailable | NOT ESTABLISHED |
| `APC::IsInStaticDialog()` | isolated metadata evidence; no native entry | runtime oracle for tested path | none | no standalone ABI/object path | DEFERRED |

## Answers to the required questions

1. **Did `XPlayDialog*` expose a useful native Dialog subsystem anchor?**  
   No. They exposed reflected metadata only.

2. **Was `UDialogManager` connected to that path?**  
   No. Its one `.data` record had zero Ghidra references, and no manager dispatch was recovered.

3. **Was any state writer connected to `IsInStaticDialog()` semantics?**  
   No.

4. **Was a paired ENTER/EXIT lifecycle seam found?**  
   No.

5. **Is there enough evidence for one targeted runtime diagnostic?**  
   No. There is no validated candidate hook or seam to instrument. Do not build or run a new diagnostic from this batch.

## Final classification

```yaml
CONFIRMED:
  - Steam 2.0.5 identity gate passed for the interpreted program.
  - XPlayDialogLine, XPlayDialogFromPool, XRestartCurrentDialog and XClearDialogQueue names exist in reflected metadata.
  - UDialogManager name/type record exists in the dump/program data.
  - IsInStaticDialog remains a runtime-confirmed oracle for the tested static-dialogue path.

STATICALLY ESTABLISHED:
  - The four XPlayDialog names have no containing executable function in the matching Ghidra database.
  - UDialogManager has no Ghidra reference from its discovered data record.
  - No convergent executable Dialog call path was recovered in this bounded batch.

CANDIDATE:
  - UDialogManager remains the conceptual ownership domain.
  - IsInStaticDialog remains the best ground-truth signal.

NOT ESTABLISHED:
  - Native implementations or ABIs for XPlayDialog*.
  - UDialogManager native lifecycle/dispatch path.
  - Any state writer connected to IsInStaticDialog semantics.
  - Paired Dialogue ENTER/EXIT seam.
  - A safe standalone ASI hook or targeted runtime diagnostic.

PRODUCTION CHANGES: NONE
ASI BUILD: NONE
RUNTIME VALIDATION: NONE
```
