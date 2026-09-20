# Wideboy v10 ADS lifecycle anchors — Steam 2.0.5 static audit

Date: 2026-09-18  
Scope: bounded, read-only validation of the externally supplied Wideboy v10
ADS IN/OUT signatures. No production source, ASI or runtime behavior changed.

## Identity gate

| Item | Expected | Observed | Result |
|---|---:|---:|---|
| Executable SHA-256 | `E7B481A97C02D80581FAB0BECE940214A88EBE30211088A00129845A039F9293` | same | PASS |
| `.text` size | `0x7CCD000` | `0x7CCD000` in the matching Ghidra program | PASS |
| Image base | `0x140000000` | `0x140000000` | PASS |
| Known current image | Steam 2.0.5 | matching Ghidra program | PASS |

Two older program entries were rejected by the script because their `.text`
sizes did not match. No conclusions were taken from those entries. Static
interpretation below uses only the identity-passing 2.0.5 program.

## External candidate signatures

```text
ADS IN:
F3 0F 10 40 4C
F3 0F 10 8E 38 01 00 00
0F 2E C8

ADS OUT:
F3 0F 10 40 50
F3 0F 10 8E 3C 01 00 00
0F 2E C8
```

## Resolution

| Candidate | Matches | Anchor RVA | Decoded anchor | Containing function |
|---|---:|---:|---|---|
| ADS IN | 1 | `0x604EEC` | `MOVSS XMM0,[RAX+0x4C]` | `FUN_140604C0A` |
| ADS OUT | 1 | `0x605092` | `MOVSS XMM0,[RAX+0x50]` | `FUN_140604C0A` |

Both patterns are unique in executable memory. They are not two unrelated
functions: both anchors are inside the same native function, with the same
prologue, register setup and surrounding update/dispatch topology.

## Structural evidence

The matched instruction layouts establish the following current-image facts:

```text
ADS IN candidate:
    [RAX+0x4C]
    [RSI+0x138]
    compare via UCOMISS XMM0,XMM1

ADS OUT candidate:
    [RAX+0x50]
    [RSI+0x13C]
    compare via UCOMISS XMM0,XMM1
```

The two blocks share the same native owner and the same surrounding paths,
including updates through `[RAX+0x3C]`, `[RAX+0x40]`, common helper calls and
the same terminal dispatch structure. This is strong evidence that the two
signatures are a paired state-transition topology rather than unrelated FOV
comparisons.

The external labels `ADS IN` and `ADS OUT` are not independently proven by
this static pass. The fields are therefore described by offsets and register
provenance, not assigned native semantic names.

## Data-flow classification

- `RAX` is loaded from `[RCX+0x8]` after the function receives its primary
  object in `RCX`.
- The IN and OUT anchors read distinct paired fields from that `RAX` object:
  `+0x4C` and `+0x50`.
- The matched byte sequences also compare against distinct transition inputs
  at `RSI+0x138` and `RSI+0x13C`.
- The common function performs finite/clamped-looking float comparison and
  update work, writes a derived scalar at `[RAX+0x3C]`, updates a byte at
  `[RAX+0x40]`, and continues through shared helper/dispatch paths.

What is not established statically:

- whether `+0x4C/+0x50` are specifically ADS begin/end values;
- whether `+0x138/+0x13C` are current/target/start/end values in gameplay
  terminology;
- whether this state machine is the only player ADS path;
- whether the transitions are reached for every weapon, scope and camera mode.

## Resolver feasibility

The pair is sufficient to justify a bounded, fail-closed diagnostic resolver:

- exactly one executable match for each signature;
- exact decoded operands and displacements;
- same containing function;
- paired IN/OUT topology;
- current-image identity gate available.

It is not yet sufficient to change Dialogue production behavior or to claim
standalone ADS ownership. Runtime semantics remain unvalidated for this exact
2.0.5 pair.

## Recommended next diagnostic

`YES` — one targeted read-only runtime diagnostic is justified.

It should install only after both unique matches and operand validation pass,
and record:

```text
ADS_IN / ADS_OUT edge
timestamp and sequence
thread id
RCX / RSI / RAX identity
[RAX+0x4C] / [RAX+0x50]
[RSI+0x138] / [RSI+0x13C]
relevant native FOV values already available at the validated camera boundary
```

The diagnostic must not write camera/FOV/state values, change Dialogue
classification, or alter ADS behavior. One run should cover ordinary gameplay,
ADS hold/release, and a post-cinematic transition if practical. The goal is to
confirm whether the edges line up with actual ADS enter/exit and whether the
same source/object pair persists through the transition.

## Final classification

```yaml
Identity gate: PASS
ADS IN signature unique in Steam 2.0.5: CONFIRMED
ADS OUT signature unique in Steam 2.0.5: CONFIRMED
Shared native containing function: STATICALLY ESTABLISHED
Paired IN/OUT topology: STATICALLY ESTABLISHED
Exact field semantic names: NOT ESTABLISHED
ADS lifecycle ownership: CANDIDATE, pending runtime confirmation
Fail-closed diagnostic resolver: JUSTIFIED
Production Dialogue integration: NOT AUTHORIZED by this audit
ASI build: NONE
Runtime: NONE
```

## Diagnostic build status

The bounded read-only diagnostic observer was implemented behind
`WIDEBOY_ADS_DIAGNOSTIC`. It resolves both unique signatures, validates the
post-load `UCOMISS XMM0,XMM1` bytes, hooks after both paired loads, and never
writes registers or game memory. Repeated hits are counted while only changed
edge snapshots are logged.

Validation:

- `test.cmd`: PASS, including `post_cinematic_exclusion=PASS`.
- `build-wideboy-ads-diagnostic.cmd`: PASS; only the known external Zydis
  C4201 warnings were emitted.
- Diagnostic artifact:
  `STALKER2CameraTweaks_WideboyAdsDiagnostic.asi`.
- Stable `STALKER2CameraTweaks.asi`: not replaced by this build.
- Runtime log reviewed: `STALKER2CameraTweaks.log` from the diagnostic run.

## Runtime result

The run produced 371 observer records grouped into 36 contiguous transition
runs: 18 `IN` runs followed by 18 `OUT` runs. Each run kept the same object
identity and the paired fields formed a monotonic blend:

```text
IN:  rax4c 0.0 → 1.0, rax50 1.0 → 0.0
OUT: rax4c 1.0 → 0.0, rax50 0.0 → 1.0
```

The same paired transition shape was repeated after rapid ADS actions and
after the cinematic sequence. The log starts with `IN` and ends with `OUT`,
with no orphan direction in the observed sequence. During the recorded
cinematic EXIT/recovery window there were no `WIDEBOY_ADS` records, while
`AtomicReplayApplied` and the cinematic recovery markers were present.

This is strong runtime confirmation that the two anchors observe a paired ADS
blend/update path in the tested scenario. It does not yet prove coverage for
every weapon, optic, or other camera transition.

The observer also revealed that the matched blocks execute on every blend
sample, not once per physical input edge. Therefore `WIDEBOY_ADS edge=IN/OUT`
in this diagnostic log means a direction-specific transition sample; a future
production integration must derive one logical lifecycle edge from the stable
transition state rather than treating every callback as a new ADS event.

Runtime classification:

```yaml
ADS IN/OUT paired transition: CONFIRMED in tested scenario
18 IN / 18 OUT runs: OBSERVED
Rapid ADS pairing: OBSERVED
Cinematic EXIT masquerading as ADS: NOT OBSERVED
Idle false ADS edges: NOT OBSERVED in supplied log
All weapon/optic coverage: NOT ESTABLISHED
Production integration: NOT AUTHORIZED by this diagnostic alone
```

## Artifacts

- `02-Research/Ghidra/ghidra-scripts/AuditWideboyAdsAnchors205.java`
- `02-Research/Ghidra/run-audit-wideboy-ads-205.cmd`
- `02-Research/evidence/wideboy-ads-anchors-205-headless.txt`
