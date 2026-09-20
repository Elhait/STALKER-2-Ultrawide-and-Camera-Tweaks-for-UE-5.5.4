# v1.0.0 Release Preparation Task Plan

## Objective

Prepare the validated v1.0.0 product for a release candidate review without publishing or committing it.

## Established evidence and current state

- The working tree contains the v1 camera/FOV implementation and accumulated documentation/test changes.
- The release INI header is now `v1.0.0`.
- Prior deterministic tests and builds were reported passing, but this release-preparation batch must re-check the current tree.
- Runtime validation is historical evidence and will not be replaced by this batch.

## Approved scope

- Inspect current Git state and release inputs.
- Validate production and diagnostic build separation.
- Run the relevant deterministic tests and builds.
- Verify release asset contents, version strings, configuration defaults, documentation consistency and known limitations.
- Produce a factual release-readiness report/checklist if needed.

## Explicit non-goals

- No new gameplay, cinematic, dialogue or resolver behavior.
- No architecture refactor or speculative cleanup.
- No game launch or new runtime session.
- No Git add, commit, tag, push or release upload.
- No deletion or relocation of files without a separately approved cleanup scope.

## Expected files or areas

- `src/`, `tests/`, `test.cmd`, `build.cmd`, `build-diagnostic.cmd`.
- `release-assets/` and release documentation.
- Current version/default configuration and user-facing README/Nexus text.
- `research/reports/` only for a factual release-preparation record if required.

## Batches

1. Read-only inventory of release state, version/configuration consistency and repository status.
2. Run deterministic tests and production/diagnostic builds serially.
3. Inspect generated artifacts and release package contents without publishing.
4. Perform a final read-only Git/path review against this plan and report remaining gaps.

## Validation

- `test.cmd`.
- `build.cmd`.
- `build-diagnostic.cmd`.
- `git diff --check` for touched release-preparation files.
- Explicit release-asset content and version checks.

## Risks and rollback/safe-failure behavior

- Existing user changes are preserved; no reset/checkout/clean operation is allowed.
- If a check exposes a behavioral or packaging defect, stop and report it rather than changing production behavior implicitly.
- Build outputs are treated as validation artifacts, not as proof of runtime compatibility.

## Stop conditions and phase gates

- Stop before publication, commit/tag, or game launch.
- Stop if the requested scope would require behavior changes or destructive cleanup.
- Stop after the final report and Git/path review.

## Expected final Git review

- Confirm changed paths are limited to approved release-preparation work.
- Separate completed, remaining, deferred and not-runtime-validated items.
- Do not create a task-log entry unless this batch changes implementation or completes a bounded release-preparation task requiring durable tracking.
