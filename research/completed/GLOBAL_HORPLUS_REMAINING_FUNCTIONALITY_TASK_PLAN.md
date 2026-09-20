# Global Hor+ — Remaining Functionality Task Plan

## Objective

Close the deterministic, repository-testable remainder of the Global Hor+ consolidation without changing production hook ownership or launching the game.

## Established evidence and current state

- The unified cinematic transform and GameplayBaseline projection already exist.
- Existing runtime evidence validates the core gameplay, cinematic, restoration, dialogue, and zoom contracts, but live resolver/order and visual behavior remain runtime-only.
- The Runtime Evidence Replay foundation is integrated into `test.cmd`.
- Cached ENTER numeric-guard behavior remains an accepted production contract and must not be replaced in this batch.

## Approved scope

- Inventory remaining Global Hor+ gaps.
- Extend deterministic replay/harness coverage for arbitrary aspects, FOV sweeps, mode transitions, cinematic mode/policy selection, stale-state prevention, and fail-closed inputs.
- Extend the runtime matrix and write the batch report/task record.
- Make no production behavior changes unless strictly required for deterministic testability.

## Explicit non-goals

- No new hooks or speculative UE ownership model.
- No replacement of the cached ENTER numeric guard.
- No Dialogue redesign, legacy-state deletion, release work, commit, or game launch.
- No deletion of diagnostic or transitional state based only on static appearance.

## Expected files/areas

- `tests/regression/runtime_evidence_replay_harness.cpp`
- `tests/config/config_persistence_harness.cpp`
- `test.cmd`
- `research/reports/GLOBAL_HORPLUS_RUNTIME_MATRIX.md`
- `research/reports/GLOBAL_HORPLUS_REMAINING_FUNCTIONALITY_BATCH.md`
- `backlog/TASKLOG.md`

## Batches and validation

1. Inventory and deterministic coverage expansion.
2. Add/verify cinematic policy-cycle coverage and stale-state/invalid-input coverage.
3. Run `test.cmd`, `build.cmd`, and `git diff --check`.
4. Perform read-only Git review against this plan, write the report/task record, then archive this plan.

## Risks and safe failure

- Tests must exercise pure/stateful contracts only; they must not simulate UE or alter hook behavior.
- Invalid or unavailable evidence must fail closed and must not overwrite a valid retained baseline.
- If any validation fails, stop with the plan at the repository root and do not broaden scope.

## Stop conditions and phase gates

- Stop after deterministic tests/build/report/matrix validation.
- Any need for new executable research, a new hook, or runtime interpretation is deferred to the combined runtime session.

## Final review

- Confirm changed paths are limited to approved test/documentation scope.
- Separate completed deterministic work from runtime-pending checks.
- Archive this plan under `research/completed/` only after final review.
