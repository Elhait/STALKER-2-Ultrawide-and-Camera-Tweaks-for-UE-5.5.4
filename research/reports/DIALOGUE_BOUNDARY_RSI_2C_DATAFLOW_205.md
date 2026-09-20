# DialogueBoundary `RSI+0x2C` data-flow audit — Steam 2.0.5

## Scope

Read-only static audit of the validated `DialogueBoundary` signature and the
`[RSI+0x2C]` value correlated at runtime with tested static dialogue. No
production source, configuration, ASI, hook, or runtime behavior was changed.

## Identity gate

| Item | Expected | Observed | Status |
|---|---|---|---|
| Executable SHA-256 | `e7b481a97c02d80581fab0bece940214a88ebe30211088a00129845a039f9293` | same | PASS |
| `.text` size | `0x7ccd000` | `0x7ccd000` | PASS |
| Image base | `0x140000000` | `0x140000000` | PASS |
| Known DialogueBoundary signature | one complete match | `0x140D08E50` | PASS |
| Signature-containing function | — | `FUN_140D08CE0` | ESTABLISHED |

The Ghidra project also contains older programs; the script reported them as
identity failures and did not analyze them.

## Confirmed

- The current validated signature matches once at `0x140D08E50` (RVA
  `0xD08E50`) inside `FUN_140D08CE0`.
- The function receives a context object as its first argument. It copies that
  argument into `RSI` at `0x140D08CF8`; therefore the `RSI` used by the
  DialogueBoundary hook is the function's `param_1` object.
- The function reads `[RSI+0x2C]` at `0x140D08DDE`, `0x140D08E5F`,
  `0x140D08ECA`, `0x140D08EEC`, `0x140D08F8E` and `0x140D08FCD`.
- `[RSI+0x2C]` participates in the native FOV blend calculation. It is the
  target/end value paired with `[RSI+0x30]` during interpolation, not a
  boolean or dialogue-state field.
- The same function reads `[RSI+0x28]` and `[RSI+0x30]`, writes the running
  blend value back to `[RSI+0x28]`, and uses `[RSI+0x1C]`/`[RSI+0x18]` for a
  later lookup/dispatch. This is a camera/FOV blend context.
- The native function has no write to `[RSI+0x2C]` in its own body. The ASI's
  diagnostic read observes the field without mutating it.
- The native function resolves/validates a related object through
  `FUN_140D08FE8(param_1 - 0x28)`; its mode byte at related-object `+0x2CEC`
  selects blend behavior and `+0x2CE8`/`+0x2CF0` supply parameters.

## Statically established

| Location | Operation | Semantic classification | Confidence |
|---|---|---|---|
| `0x140D08CF8` | `RSI = RCX` | context-object provenance | high |
| `0x140D08D2D` | read `[RSI+0x28]` | current/running blend value | high |
| `0x140D08D36` | write `[RSI+0x28]` | native blend-state update | high |
| `0x140D08DDE` | read `[RSI+0x2C]` | FOV blend target/end value | high |
| `0x140D08DE3` | read `[RSI+0x30]` | paired FOV blend value | high |
| `0x140D08DE8–0x140D08DF1` | arithmetic using `+0x2C/+0x30` | interpolation toward target | high |
| `0x14004C5CE` | copies source `+0x2C` into destination `+0x2C`, alongside `+0x28`, `+0x38`, and masked `+0x3C` | generic camera/blend-context copy candidate | medium |

`FUN_14004C5CE` is the strongest writer candidate because its field cluster
matches a state-copy operation. Its only indexed reference in the current
program is a DATA/vtable reference at `0x14B1BB364`; no direct code caller was
recovered by the current no-analysis Ghidra program. This does not establish
that it writes the same live object instance used by `FUN_140D08CE0`.

A broad inventory found many unrelated `+0x2C` reads/writes across the image,
including integers, pointers, stack locals and unrelated object types. Offset
equality alone is not object identity or semantic evidence.

## Not established

- `[RSI+0x2C]` is not established as a Dialogue ownership flag or positive
  dialogue discriminator.
- The field's allocation/type name, UE class, or owning reflection object is
  not established from the current executable evidence.
- A complete writer chain from a dialogue manager/state transition to the live
  `RSI` object is not established.
- It is not established that every non-dialogue camera transition can never
  produce a target different from ordinary gameplay FOV.

## Implications

1. The runtime correlation (`70` in tested dialogue versus `90` in tested
   post-cinematic/recovery context) is useful evidence, but remains a
   correlation against a generic FOV target field.
2. A production classifier of `target != gameplayFov` or `target == 70` is not
   justified by this audit.
3. The existing FOV-shape heuristic remains current production behavior; this
   task did not repair it.
4. If research resumes, the lowest-risk path is object-identity and
   writer/caller correlation for the live context object against the UE4SS
   `IsInStaticDialog()` oracle. No new production hook is justified here.

## Final classification

```text
RSI+0x2C is native FOV-blend target          CONFIRMED
RSI provenance from DialogueBoundary         STATICALLY ESTABLISHED
Dialogue ownership semantics                 NOT ESTABLISHED
Same-object writer chain                     NOT ESTABLISHED
Safe positive Dialogue discriminator          NOT ESTABLISHED
Production repair                            NONE
Runtime validation                           NOT RUN (by scope)
```

## Reproducibility

Headless read-only evidence:

- `research/evidence/audit-dialogue-boundary-rsi-2c-205-headless.txt`
- `research/evidence/decompile-dialogue-offset-writers-205-headless.txt`
- `research/evidence/trace-dialogue-offset-writer-callers-205-headless.txt`

Scripts used:

- `02-Research/Ghidra/ghidra-scripts/AuditDialogueBoundaryRsi2c205.java`
- `02-Research/Ghidra/ghidra-scripts/DecompileDialogueOffsetWriters205.java`
- `02-Research/Ghidra/ghidra-scripts/TraceDialogueOffsetWriterCallers205.java`
- `02-Research/Ghidra/ghidra-scripts/ScanOffset2cWriters205.java`
