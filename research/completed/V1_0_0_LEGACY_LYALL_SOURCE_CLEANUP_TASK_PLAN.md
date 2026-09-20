# v1.0.0 Legacy Lyall Source Cleanup — Task Plan

## Objective

Remove two unreachable legacy files from the current source tree:

- `src/helper.hpp`
- `src/stdafx.h`

The goal is to remove obsolete, unbuilt scaffolding. This is not a rewrite or
an attempt to alter historical licensing provenance.

## Established evidence and current state

- The completed provenance audit compared the current tree against
  `Lyall/STALKER2Tweak` and identified `src/helper.hpp` as derived legacy
  scaffolding and `src/stdafx.h` as byte-identical legacy scaffolding.
- `build.cmd` does not compile either file.
- No current production translation unit includes either file.
- `test.cmd` does not compile either file.
- The current production build does not contain identified
  Lyall/STALKER2Tweak-derived implementation.

## Approved scope

1. Reconfirm the files are unreachable from production and harness build
   inputs.
2. Delete only the two explicit target files.
3. Search the repository for remaining source references.
4. Run `build.cmd`, `test.cmd`, and `git diff --check`.
5. Perform a read-only Git review and record the completed cleanup in
   `backlog/TASKLOG.md`.

## Explicit non-goals

- Do not modify production behavior, production source, signatures, hooks, or
  configuration.
- Do not copy, rewrite, or relocate either legacy file.
- Do not modify `LICENSE.md`, `THIRD_PARTY_NOTICES.md`, or licensing policy.
- Do not remove historical provenance from Git history or historical plans.
- Do not launch the game.

## Expected files and areas

- Removed: `src/helper.hpp`, `src/stdafx.h`.
- Added during execution: this task plan; one factual `backlog/TASKLOG.md`
  completion entry after validation.
- No other source or runtime files are expected to change.

## Batches

### Batch 1 — Reachability confirmation and deletion

Confirm no production or harness include/reference and no `build.cmd` or
`test.cmd` dependency, then remove exactly the two named files.

### Batch 2 — Validation and review

Run the approved build and harness suite, inspect repository-wide references,
run `git diff --check`, and compare actual changed paths against this plan.

## Risks and safe failure behavior

The only material risk is an overlooked build or test dependency. The target
files are deleted only after reference checks; a failed build or test stops the
task and is reported without further refactoring. Git history retains the
removed files and their provenance.

## Stop conditions and phase gates

Stop if either target is referenced by an active build/test input, if any
unexpected source path changes, or if validation fails. Do not expand into
licensing changes or further legacy cleanup.

## Final review requirements

Review `git status`, the relevant diff/stat, and changed paths. Confirm only
the two legacy files, this plan, and the factual task-log entry changed; record
that runtime/game validation was not run because behavior was not changed.
