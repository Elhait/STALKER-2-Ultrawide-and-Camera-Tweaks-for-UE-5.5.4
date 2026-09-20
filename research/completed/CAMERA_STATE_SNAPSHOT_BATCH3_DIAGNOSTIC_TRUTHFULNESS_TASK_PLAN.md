# CameraStateSnapshot Batch 3 — Diagnostic Truthfulness Task Plan

## Objective

Make the read-only CameraStateSnapshot diagnostic truthful and unambiguous:
use the canonical Dialogue and Zoom observation fields, remove duplicate mutable
source fields, and make the harness assert exact source values and explicit
last-observation semantics.

## Established evidence and current state

- Batch 2 added canonical `DialogueSnapshot::source` and
  `ZoomEvidenceSnapshot::source` fields.
- The runtime snapshot logger still reads `SourceProvenance::dialogueBlend`
  for `dialogueSource`, which is not the canonical Dialogue observation.
- `SourceProvenance` still contains independent Dialogue/Zoom source copies,
  allowing telemetry truths to diverge.
- Zoom callbacks are native observations of the last transition sample; they
  are not proven to represent a current persistent camera owner/state.
- Candidate remains a classifier hypothesis and is explicitly out of scope.

## Approved scope

1. Remove duplicate Dialogue/Zoom source fields from `SourceProvenance`.
2. Route logger output through `DialogueSnapshot::source` and
   `ZoomEvidenceSnapshot::source`.
3. Rename Zoom diagnostic labels to state last-observation semantics, including
   direction, sequence, and availability.
4. Strengthen the camera snapshot harness with exact expected source values and
   last-observation assertions.
5. Update the Batch 3 report and factual task log.

## Explicit non-goals

- No Candidate invalidation or lifecycle redesign.
- No Dialogue classifier, recovery, policy, FOV, HorPlus, Cinematics,
  AspectRecalculation, or ZOOM runtime behavior changes.
- No interpretation of `0.25` as binocular ownership.
- No Ghidra/reverse-engineering work.
- No game launch or runtime validation in this batch.

## Expected files or areas

- `src/camera/camera_state_snapshot.hpp`
- `src/camera/camera_state_snapshot.cpp`
- `src/plugin/runtime.cpp` diagnostic snapshot logging
- `tests/camera/camera_state_snapshot_harness.cpp`
- `research/reports/CAMERA_STATE_SNAPSHOT_BATCH3_DIAGNOSTIC_TRUTHFULNESS.md`
- `backlog/TASKLOG.md`

## Batches and validation

### Batch 1 — Canonical source and label repair

- Remove duplicate Dialogue/Zoom fields from `SourceProvenance`.
- Update comparison logic and runtime logger to use typed canonical fields.
- Rename Zoom output to `lastZoomDirection`, `lastZoomSequence`, and
  `zoomObservationAvailable`.

Validation: source compilation, snapshot harness, production build and the
diagnostic build that contains the snapshot logger.

### Batch 2 — Exact harness contract

- Assert exact gameplay writer, Dialogue, and Zoom source tokens.
- Assert unavailable Zoom is represented as unavailable rather than a current
  state.
- Preserve numeric-only FOV animation as non-semantic snapshot change.

Validation: full harness suite and `git diff --check`.

### Batch 3 — Review and archival

- Compare changed paths to this plan.
- Record build/test limits and the absence of runtime validation.
- Write the report and task-log entry, then archive this plan under
  `research/completed`.

## Risks and rollback / safe-failure behavior

- This is diagnostic-only state; production camera behavior must remain
  unchanged.
- Removing duplicate fields may expose missed references at compile time; fix
  only those direct snapshot references within the approved scope.
- If any diagnostic build fails, stop before runtime and retain the source
  changes for review; do not broaden the patch.
- If source semantics cannot be represented without duplication, stop and
  report the ambiguity instead of inventing a derived owner state.

## Stop conditions and phase gates

- Stop if implementation requires Candidate behavior or production ownership
  changes.
- Stop if exact source identity cannot be asserted from the current snapshot
  inputs.
- Stop after build, harness, diff check, and read-only Git review. Runtime is
  explicitly deferred.

## Expected final Git review

- Only the approved snapshot, runtime diagnostic logging, harness, report,
  task-log entry, and archived plan paths may be changed by this batch.
- Confirm production logic and non-diagnostic camera behavior are untouched.
- Record completed, remaining, deferred, blocked, and not-runtime-validated
  items in the report and task log.
