# Dialogue Candidate Context Task Plan

## Objective

Close the bounded `Candidate → Active` observation-context gap by binding a
Candidate hypothesis to its DialogueBoundary source and native target, while
leaving the established Active/Exiting/Recovery semantics unchanged.

## Established evidence and current state

- `CandidateTracker` currently stores only FOV samples.
- Candidate promotion currently calls `Observe(incoming, ...)` without source
  or target context.
- Gameplay invalidation observes a different callback domain and cannot prove
  DialogueBoundary continuity.
- Runtime showed stable source/target during legitimate promotion, while the
  cross-domain mismatch remains a concrete static gap rather than an observed
  runtime failure.

## Approved scope

- Capture DialogueBoundary `context.rsi` and guarded `[rsi+0x2C]` at
  `Inactive → Candidate`.
- Require valid unchanged source and coherent target at Candidate promotion.
- Re-check Gameplay coordinator, post-cinematic exclusion and non-Native
  selected policy at promotion.
- Cancel invalid Candidates without same-sample reseeding.
- Add deterministic harness coverage for source, target, policy and trajectory
  cases.

## Explicit non-goals

- No CameraStateSnapshot rewrite.
- No changes to Active → Exiting → Recovery or RecoveryRearm.
- No ZOOM ownership, `0.25` interpretation, gameplay-writer substitution,
  target-70 special case, timeout or global generation logic.
- No unrelated cleanup, Ghidra work or runtime/game launch.

## Expected files or areas

- `src/dialogue/dialogue_state.hpp/.cpp`
- `src/plugin/runtime.cpp`
- `tests/dialogue/candidate_hardening_harness.cpp`
- `research/reports/DIALOGUE_CANDIDATE_CONTEXT_REPAIR.md`
- `backlog/TASKLOG.md`

## Implementation batches

### Batch 1 — Candidate context contract

Extend `CandidateTracker` with source/target context capture and promotion
validation. Preserve existing trajectory logic and fail closed on unavailable
or incoherent context.

### Batch 2 — Runtime integration

Read DialogueBoundary context only at Candidate creation/promotion, guard the
native target read, cancel on context contradiction or Native policy, and do
not reseed the sample that caused cancellation.

### Batch 3 — Harness and review

Cover same-context activation, source/target contradiction, invalid context,
policy changes, legitimate lifecycle continuity, and existing trajectory
rules. Run relevant harness, full tests, builds, diff check and Git review.

## Validation

- Candidate hardening harness: PASS required.
- Full `test.cmd`: PASS required.
- Production `build.cmd`: PASS required.
- Combined diagnostic build if shared runtime code is compiled by it: PASS
  required.
- `git diff --check`: PASS required.
- No runtime validation in this batch.

## Risks and safe failure

- A missing or unreadable target must cancel the Candidate and pass the sample
  unchanged; it must not create a new Candidate in the same callback.
- Source/target tolerance must use existing finite/epsilon conventions and not
  introduce a hardcoded Dialogue target.
- Existing real-dialogue promotion must remain possible with a changed
  selected policy as long as it remains non-Native.

## Stop conditions and phase gates

- Stop if the patch touches Active/Exiting/Recovery behavior.
- Stop if context cannot be read safely at the existing DialogueBoundary.
- Stop after static/harness/build validation; runtime is deferred to the next
  planned combined session.

## Expected final Git review

Confirm only Candidate context, its direct runtime integration, harness,
report, task log and archived plan changed. Record runtime validation as
deferred and preserve all unrelated worktree changes.
