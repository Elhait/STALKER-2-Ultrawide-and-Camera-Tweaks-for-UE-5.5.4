# Dialogue Candidate Context Repair

Date: 2026-09-18

## Scope

This bounded repair closes the static `Candidate → Active` observation-context
gap. It does not alter Active → Exiting → Recovery semantics or any camera,
ZOOM, HorPlus, Cinematics or CameraStateSnapshot behavior.

## Implemented contract

- Candidate creation captures the DialogueBoundary source (`context.rsi`) and
  a guarded native target from `[source+0x2C]`.
- Candidate creation fails closed when source, target or selected policy is
  invalid for promotion.
- Candidate promotion requires unchanged source and a coherent native target.
- Promotion also re-checks non-Native selected policy, Gameplay coordinator and
  inactive post-cinematic exclusion.
- A contradiction cancels the Candidate, passes the current sample unchanged,
  and does not reseed the same sample.
- Non-Native policy changes remain allowed; the current selected policy is
  captured only when promotion succeeds.
- RecoveryRearm remains source-aware and unchanged by this repair.

## Harness coverage

The candidate harness covers:

- same source/target with valid descent → Activate;
- source change → Cancel;
- target contradiction → Cancel;
- zero source and invalid target → fail closed;
- Native policy → promotion blocked;
- non-Gameplay and active post-cinematic exclusion → blocked;
- Adaptive/Reduced policy change → remains allowed;
- existing candidate trajectory and invalid-sample behavior.

## Validation

- Full `cmd /c test.cmd`: PASS.
- `cmd /c build.cmd`: PASS.
- `cmd /c build-horplus-fov-state-diagnostic.cmd`: PASS.
- `git diff --check`: PASS with normal repository line-ending warnings.
- Known external Zydis C4201 warnings remain unchanged.
- No game launch, runtime claim or Ghidra analysis.

## Status

- Completed: bounded Candidate context capture and promotion guard.
- Remaining: runtime regression on a real Dialogue followed by generic FOV/ZOOM
  transitions.
- Deferred: source-scoped Candidate generation/epoch design beyond this local
  context contract.
- Blocked: none within this batch.
- Not runtime-validated: all post-repair in-game behavior.

## Patch summary

Dialogue Candidate hypotheses are now tied to the DialogueBoundary source and
native target through promotion, with fail-closed cancellation on context
contradiction.

## Changelog summary

Pre-existing Candidate hypotheses can no longer promote using a different
DialogueBoundary source or target, and Native-policy Candidates are rejected
at the promotion boundary.
