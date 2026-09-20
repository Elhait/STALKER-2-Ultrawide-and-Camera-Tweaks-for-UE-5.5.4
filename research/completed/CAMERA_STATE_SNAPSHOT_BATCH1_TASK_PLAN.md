# CameraStateSnapshot Batch 1 Task Plan

## Objective

Implement a pure, read-only derived `CameraStateSnapshot` model and builder
that composes existing camera/FOV evidence for diagnostics and deterministic
harness validation. It must not become a new runtime owner or change production
behavior.

## Established evidence and current state

- Coordinator, Dialogue, gameplay mode, native writer, native target and ZOOM
  evidence currently live in separate owners/callbacks.
- Dialogue `Candidate` is a classifier hypothesis; `Active` is a mod-confirmed
  lifecycle state, not proven game-owned Dialogue.
- ZOOM hooks are diagnostic-only native observations; their complete lifecycle
  is not established.
- Source pointers from different hook contracts must not be treated as one
  universal identity.
- The latest Dialogue recovery repair remains unchanged in this batch.

## Approved scope

- Add pure snapshot/provenance/validity types.
- Add a pure builder that receives event-local values explicitly.
- Add typed source-token domains and diagnostic generation/epoch provenance.
- Add deterministic harness coverage for independent substates and unknown data.
- Add compile-gated, change-only diagnostic snapshot telemetry only if it can
  be integrated without a gameplay-writer hot-path log or behavior branch.
- Update the research report and task log with the implementation result.

## Explicit non-goals

- No production behavior migration or new camera coordinator.
- No super-enum, ADS owner, ZOOM lifecycle inference or timeout.
- No Dialogue classifier, policy, endpoint or recovery changes.
- No HorPlus, Cinematics, AspectRecalculation or mode-transition changes.
- No pointer equivalence assumptions and no generation-based invalidation of
  production state.
- No game launch, Ghidra work, commit or release.

## Validation

- Full `test.cmd` suite.
- Production build.
- Combined HorPlus FOV/ZOOM diagnostic build.
- `git diff --check` and read-only Git review.
- Runtime validation remains deferred.
