# CameraStateSnapshot Batch 3 — Diagnostic Truthfulness

Date: 2026-09-18

## Scope

This batch corrected diagnostic truthfulness only. It did not change Dialogue,
ZOOM, HorPlus, Cinematics, AspectRecalculation, Candidate handling or any
runtime camera owner.

## Findings addressed

- The snapshot logger read `source.dialogueBlend` instead of the canonical
  `dialogue.source`, allowing a real Dialogue source to be printed as `0x0`.
- `SourceProvenance` held independent mutable Dialogue and Zoom source copies
  even though the typed Dialogue and Zoom snapshots already owned those
  observations.
- `zoom=...` and `zoomSeq=...` did not state that they represented the last
  native observation rather than a current persistent camera state.
- The harness checked source separation indirectly instead of asserting the
  exact source tokens carried by each typed observation.

## Implementation

- `SourceProvenance` now contains only the gameplay-writer source token.
- The logger reads `DialogueSnapshot::source` and
  `ZoomEvidenceSnapshot::source` directly.
- ZOOM telemetry now uses `lastZoomDirection`, `lastZoomProvenance`,
  `lastZoomSequence` and `zoomObservationAvailable`, with explicit source
  availability fields.
- The camera harness asserts exact gameplay, Dialogue and Zoom source values,
  and verifies that a ZOOM record is a last observation.
- Candidate remains a hypothesis and was not invalidated or reclassified.

## Validation

- Full `cmd /c test.cmd`: PASS, including `camera_state_snapshot=PASS`.
- `cmd /c build.cmd`: PASS.
- `cmd /c build-horplus-fov-state-diagnostic.cmd`: PASS.
- `git diff --check`: PASS with the repository's normal line-ending warnings.
- Known external Zydis C4201 warnings remain unchanged.
- No game launch, runtime claim, Ghidra analysis or executable support claim
  was made.

## Git review and limits

The implementation was reviewed against the bounded task plan. The affected
behavioral code remains outside this diagnostic-only change; the broader
working tree contains pre-existing user changes and historical research paths
that were intentionally preserved. The corrected telemetry still needs one
combined runtime sanity run before it can be called runtime-validated.

## Status

- Completed: canonical source wiring, unambiguous ZOOM labels, exact harness
  contracts and source-duplication removal.
- Remaining: combined runtime sanity validation of the corrected logger.
- Deferred: source-scoped Candidate invalidation and interpretation of the
  observed `0.25` zoom parameter.
- Blocked: none within this batch.
- Not runtime-validated: all post-patch in-game telemetry.

## Patch summary

Made CameraStateSnapshot diagnostics report the actual typed Dialogue and ZOOM
observations and removed duplicate mutable source fields.

## Changelog summary

Diagnostic logs now distinguish last ZOOM observations from current ownership
and print the canonical Dialogue source instead of a stale duplicate field.
