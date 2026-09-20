# DialogueBoundary `RSI+0x2C` data-flow audit

## Objective

Perform a read-only current-image static audit of the validated
`DialogueBoundary` instruction `ucomiss xmm6,[rsi+0x2C]`. Establish the
provenance and semantics of the object in `RSI`, the meaning and writers of
`+0x2C`, and any stronger nearby state discriminator.

## Established evidence

- Steam 2.0.5 executable identity is known and must pass the workspace hard
  gate before interpretation: SHA-256
  `E7B481A97C02D80581FAB0BECE940214A88EBE30211088A00129845A039F9293`, image
  base `0x140000000`, `.text` size `0x7CCD000`.
- Runtime correlation observed `[RSI+0x2C]=90` in a post-cinematic candidate
  and `[RSI+0x2C]=70` in tested static-dialogue intervals.
- `APC::IsInStaticDialog()` remains the UE4SS ground-truth oracle, but its
  standalone ASI access is deferred.

## Approved scope

- Matching 2.0.5 Ghidra program and existing executable evidence only.
- Re-establish the DialogueBoundary function and instruction bytes.
- Trace `RSI` provenance, bounded surrounding object accesses, `+0x2C` writers,
  comparison branches and nearby state.
- Produce a durable research report with evidence/confidence classifications.

## Explicit non-goals

- No production source, config, classifier, HorPlus or cinematic changes.
- No ASI build, game launch or UE4SS runtime probe.
- No guessed UE type, object layout, RVA, ABI or field ownership.
- No standalone `IsInStaticDialog()` bridge or ProcessEvent work.
- No Dialogue repair, hotkey snapshot repair or FOV math changes.
- No conclusions from unrelated `[reg+0x2C]` accesses or isolated `70.0f`
  literals.

## Expected files or areas

- `02-Research/Ghidra/ghidra-scripts/` — one read-only audit helper if needed.
- `02-Research/Ghidra/` — tracked launcher if needed.
- `02-Research/evidence/` — reproducible headless output.
- `research/reports/` — final audit report.
- `research/deferred/` — this plan after the audit stop point.

## Batches

### Batch 1 — Identity and DialogueBoundary

Confirm current executable identity, locate the validated DialogueBoundary and
decode the enclosing function. Confirm `ucomiss xmm6,[rsi+0x2C]` and establish
how `RSI` is supplied at the bounded entry.

### Batch 2 — RSI provenance and object model

Follow `RSI` backward as far as unambiguous static data flow permits. Record
argument/local/member/return provenance, caller context, lifetime clues and
neighboring accesses. Leave UE type unknown unless directly evidenced.

### Batch 3 — `+0x2C` writers and comparison semantics

Find validated writes for the same context, record source/control flow and
classify initialization/update/reset/copy. Decode the branches following the
comparison and classify convergence, direction, clamp or completion semantics
only where control flow supports it.

### Batch 4 — Surrounding state and runtime reconciliation

Inspect nearby fields and reconcile only after static analysis with the known
runtime values `90` and `70`. Determine whether a stronger state discriminator
is available from the same hook context.

## Validation

- Enforce SHA-256, `.text` size, image base/layout and known writer-anchor
  identity before static interpretation.
- Headless Ghidra runs are tracked, read-only and `-noanalysis`; terminate only
  the owned PID on completion/timeout and verify project locks are released.
- Run `git diff --check` for research files. Do not build or launch the game.

## Risks and safe failure

- A mid-hook register may not identify a UE object without complete provenance.
- Decompiler names and isolated offsets are not ownership proof.
- If provenance or writer identity is ambiguous, report `NOT ESTABLISHED` and
  stop without selecting a production predicate.

## Stop conditions

- Any identity mismatch stops the audit.
- Unrelated `+0x2C` accesses are excluded from the model.
- No production discriminator is selected unless the same object/context and
  semantics are statically supported.

## Final review

Compare changed paths with this plan, record confirmed/static/not-established
results and preserve all pre-existing working-tree changes. Do not stage or
commit.
