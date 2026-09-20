# v1.0.0 Template Persistence Safety Fix Task Plan

## Objective

Make `config::SynchronizeManagedConfigTemplate` preserve the last-known-good
INI whenever staging, writing, closing or replacing the managed template fails.

## Established evidence and current state

- `PersistConfigValue` already uses staging plus non-destructive replacement.
- `SynchronizeManagedConfigTemplate` still opens the live INI with
  `std::ios::trunc` after constructing replacement content.
- A failure after truncation can leave the last-known-good config empty or
  partial.
- Batch 4 release closure is stopped; no observer changes are in scope.

## Approved scope

- Change only the managed-template persistence path.
- Preserve template content, keys, defaults, parsing and synchronization policy.
- Add bounded failure-injection validation for normal sync, staging failure and
  replacement failure.

## Explicit non-goals

- No observer or camera-writer changes.
- No `helper.hpp` cleanup.
- No `PersistConfigValue` rewrite unless required for compilation.
- No config format/default changes.
- No game launch or full Batch 4 regression.
- No broad ConfigRepository abstraction rewrite.

## Expected files

- `src/config/config_template.hpp/.cpp`
- `src/plugin/runtime.cpp` (failure logging callback wiring only)
- `tests/config/config_persistence_harness.cpp`
- `research/reports/V1_0_0_FINAL_SAFETY_FINDINGS_VERIFICATION.md`
- `backlog/TASKLOG.md`

## Batches

### Batch 1 — non-destructive template commit

Write complete synchronized output to `path + ".tmp"`, close and validate the
staging stream, then call `MoveFileExW` with replacement/write-through flags.
On any failure, remove the staging artifact where possible and leave the live
INI untouched.

### Batch 2 — behavioral validation

Extend the existing config persistence harness with template synchronization
cases and verify actual live-file contents after each injected failure.

## Validation

- Normal template synchronization installs complete output.
- Staging failure preserves the original live file.
- Replacement failure preserves the original live file.
- Existing config persistence harness passes.
- Worker lifecycle and feature status harnesses pass.
- `build.cmd` passes.
- `git diff --check` passes.
- Perform read-only Git review against this plan.

## Risks and safe failure

- The staging filename remains the existing `.tmp` convention.
- If replacement cannot be completed, synchronization returns failure and the
  current live config remains authoritative.
- No direct live-file fallback is permitted.

## Stop conditions

- Stop before observer work after this batch.
- Stop if preserving exact template semantics requires changing parsing or
  configuration policy.
- Stop if failure injection cannot distinguish staging failure from replacement
  failure without changing production behavior.

## Final review

Compare changed paths and behavior against this plan, record completed and
remaining work in `TASKLOG.md`, and leave Batch 4 closed only after the open
regression and this safety fix receive their own validation gates.
