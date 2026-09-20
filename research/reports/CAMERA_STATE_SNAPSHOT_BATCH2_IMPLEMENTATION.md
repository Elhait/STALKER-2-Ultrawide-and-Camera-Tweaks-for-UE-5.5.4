# CameraStateSnapshot Batch 2 — Telemetry Truthfulness Report

## Result

Corrected the diagnostic snapshot wiring identified by the 2.0.6 combined
runtime run. No production camera/FOV behavior or lifecycle decision was
changed.

## Corrections

- `Dialogue Candidate` is no longer reported as authoritative
  `CAMERA_OWNER=Dialogue`; only Active/Exiting/RearmPending use that owner
  category.
- Snapshot logging now uses semantic change detection instead of comparing
  every animated numeric FOV sample.
- ZOOM snapshot weights now use the validated `RAX+0x4C/+0x50` pair.
- HorPlus snapshot source uses the event-local gameplay writer `context.rsi`.
- Dialogue source and native target evidence are retained through diagnostic
  atomics and exposed with their own typed source domain.
- Post-cinematic recovery exclusion is represented separately from Dialogue
  lifecycle state.
- Snapshot logs use readable presentation, Dialogue, provenance and ZOOM names.
- Diagnostic snapshot events receive a local sequence; unavailable values stay
  unavailable rather than being guessed.

## Explicitly unchanged

- Stale Candidate invalidation behavior.
- Dialogue classifier, policy snapshot and recovery semantics.
- ZOOM ownership/lifecycle semantics.
- HorPlus, Cinematics, AspectRecalculation and gameplay mode behavior.
- Pointer equivalence and generation-based production invalidation.

## Validation

- Full `test.cmd`: PASS, including `camera_state_snapshot=PASS`.
- Production `build.cmd`: PASS.
- Combined `build-horplus-fov-state-diagnostic.cmd`: PASS.
- `git diff --check`: PASS with normal line-ending warnings and known external
  Zydis C4201 warnings.
- Game launch/runtime validation after this correction: NOT PERFORMED.

## Status

```yaml
Snapshot semantic logging:              FIXED STATICALLY / HARNESS VALIDATED
ZOOM weight source:                     FIXED STATICALLY / BUILD VALIDATED
Gameplay writer source provenance:      FIXED STATICALLY / BUILD VALIDATED
Dialogue source/target telemetry:       FIXED STATICALLY / BUILD VALIDATED
Recovery exclusion representation:      FIXED STATICALLY / BUILD VALIDATED
Candidate invalidation behavior:        NOT CHANGED / SEPARATE FINDING
Runtime validation of corrections:      PENDING
Behavior migration:                     NOT PERFORMED
```

The stale Candidate observed in the previous run remains a separate behavior
design finding. This batch deliberately makes it visible and correctly
classified without attempting to invalidate it.
