# CameraStateSnapshot Batch 2 — Telemetry Truthfulness Task Plan

## Objective

Correct only the diagnostic snapshot wiring exposed by the 2.0.6 combined run.
Make the derived telemetry semantic, provenance-aware and source-accurate
without changing any production camera/FOV behavior or lifecycle decisions.

## Approved scope

- Semantic change-only diagnostic snapshot logging.
- Correct ZOOM weights from `RAX+0x4C/+0x50`.
- Event-local gameplay-writer source.
- Dialogue source/native-target evidence and recovery exclusion state.
- Named enum/provenance output and Candidate non-owner classification.
- Harness, production build, combined diagnostic build and Git review.

## Explicit non-goals

- No Dialogue Candidate invalidation or classifier repair.
- No ZOOM owner/Common lifecycle, ADS owner or completion inference.
- No FOV math, HorPlus, Cinematics, AspectRecalculation or mode changes.
- No generation-based production invalidation or pointer equivalence.
- No game launch, Ghidra analysis, commit or release.

## Validation

- Full `test.cmd` suite.
- Production and combined diagnostic builds.
- `git diff --check` and read-only Git review.
- Runtime validation remains deferred to the next combined run.
