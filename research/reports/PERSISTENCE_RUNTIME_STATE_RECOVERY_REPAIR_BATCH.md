# Persistence / Runtime State / Recovery Repair Batch

## Scope

Implemented the bounded P1–P4 repair batch from the persistence/runtime-state audit. No game launch, executable reverse engineering, unrelated cleanup, or production release work was performed.

## Results

### P1 — Cinematic selection authority

- Cinematic ENTER now accepts a selection before publishing it.
- A duplicate ENTER is rejected while an active selection is valid.
- The active selection is the source for cinematic aspect resolution; later F9/F12 changes remain next-lifecycle configuration.
- Selection capture is represented by a deterministic helper and covered by harness tests.
- EXIT still invalidates the active selection through the existing lifecycle path.

### P2 — Closed/stopping cinematic aspect fallback

- The closed/stopping path now performs an aspect-only write to the native `+0x254` field.
- It does not write the native flags byte at `+0x259` and does not require that flags field to be writable.
- The normal committed path remains on the existing validated aspect-store helper.

### P3 — Dialogue context invalidation

- Existing source-change/material-FOV-jump semantics were extracted into a shared deterministic evaluator.
- Both `ReplayManualTransitionOriginal` and `ApplyHorPlusGameplay` use the same evaluator before mode-specific FOV handling.
- The helper observes native/current writer input, not transformed HorPlus output.
- Candidate/recovery/ZOOM policy was not redesigned.

### P4 — Configuration duplicate semantics

Canonical rule: the loader remains last-valid-occurrence-wins; persistence updates every matching managed key occurrence across matching managed sections. Therefore a persisted value is the effective value after reload even when a user file contains duplicate managed sections or keys. Unmanaged content and parser behavior remain unchanged.

Initial INI creation now explicitly flushes and closes before reporting success.

## Ghost/recovery decisions

- `ReplayState::AppliedConstrainPass`: retained. It remains part of the existing replay lifecycle and is not an unreachable-only deletion candidate in this batch.
- Recovery liveness questions remain runtime-validation items: CinematicExiting terminal behavior, post-cinematic exclusion completion, and unreadable/invalid recovery targets.

## Validation

- Focused and full deterministic harnesses: PASS.
- `test.cmd`: PASS.
- `build.cmd`: PASS.
- `build-diagnostic.cmd`: PASS.
- `git diff --check`: PASS; only line-ending normalization warnings were reported by Git.
- Runtime: NOT PERFORMED.

## Manifest

```yaml
cinematic_snapshot_authority: FIXED
cinematic_native_fallback: FIXED
dialogue_context_invalidation: FIXED
config_duplicate_semantics: FIXED
config_canonical_rule: last-valid-loader semantics; persistence normalizes all matching managed occurrences
replay_ghost_state: RETAINED_WITH_REASON
initial_ini_creation: FIXED
recovery_liveness: RUNTIME_VALIDATION_REQUIRED
tests: PASS
builds: PASS
diff: PASS
runtime: NOT_PERFORMED
```

## Remaining runtime checks

Accumulate for the next runtime session:

- duplicate cinematic ENTER while F9/F12 selection changes are pending;
- closed/stopping cinematic aspect store with writable aspect and unavailable flags;
- Dialogue source/FOV context changes through both Gameplay modes;
- recovery liveness and terminal handoff behavior listed above.
