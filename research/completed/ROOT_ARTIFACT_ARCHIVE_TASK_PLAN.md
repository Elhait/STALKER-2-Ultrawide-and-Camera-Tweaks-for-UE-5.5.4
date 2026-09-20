# Root Artifact Archive — Task Plan

## Objective

Move generated ASI artifacts and obsolete specialized build scripts out of
the repository root without deleting them or changing source behavior.

## Approved scope

- Root `*.asi` files to `build-artifacts/asi-history/`.
- Historical specialized diagnostic build scripts to
  `research/completed/build-profiles/`.
- Keep current `build.cmd`, `build-diagnostic.cmd`, and `test.cmd` in root.

## Non-goals

- No deletion, source changes, build changes, documentation changes, or
  runtime validation.
- No task-log or report entry for this housekeeping operation.

## Validation

- Confirm exact source paths before moving.
- Confirm no root ASI or archived script remains at the original path.
- Confirm every moved file exists at its archive path.
- Read-only Git status/path review.
