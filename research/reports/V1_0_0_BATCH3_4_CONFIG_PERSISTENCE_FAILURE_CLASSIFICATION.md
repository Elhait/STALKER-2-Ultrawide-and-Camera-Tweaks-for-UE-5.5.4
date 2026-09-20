# v1.0.0 Batch 3.4 — Config Persistence Failure Classification

## Scope

Audit the current `config::PersistConfigValue` control flow after the Batch 2
config extraction. This batch classifies the persistence failure path only.
It does not modify production code, configuration format, defaults or
user-facing behavior.

## Current control flow

1. The existing INI is opened with `std::ifstream` and read fully into memory.
2. The requested value is replaced or appended in the in-memory line list.
3. A sibling temporary file (`<config>.tmp`) is opened with
   `std::ios::trunc` and receives the complete rewritten content.
4. If the temporary write stream reports failure, the function returns
   `false`. The original file has not been opened for writing; the temporary
   file is not explicitly removed on this early return.
5. The temporary stream is closed by scope exit.
6. `MoveFileExW` is attempted with `MOVEFILE_REPLACE_EXISTING |
   MOVEFILE_WRITE_THROUGH`.
7. If replacement succeeds, the function returns `true` and the original path
   is replaced by the completed temporary file.
8. If replacement fails, the function logs the error and enters the direct
   fallback.
9. The fallback opens the original path with `std::ios::out |
   std::ios::trunc`. Successful opening truncates the last-known-good INI
   before the replacement content has been written successfully.
10. If the fallback write finishes with a good stream state, the temporary
    file is deleted and the function returns `true`.
11. If the fallback cannot open or the stream reports failure, the function
    logs the error, deletes the temporary file and returns `false`.

## Failure-path findings

| Failure point | Original INI before failure | Result |
| --- | --- | --- |
| Input open/read failure | Unmodified | `false`; no persistence change is attempted. |
| Temporary open/write failure | Unmodified | `false`; temporary cleanup is incomplete on this early return. |
| `MoveFileExW` failure before fallback | Unmodified | Fallback is entered; temporary file still exists. |
| Fallback open failure | Normally unmodified | `false`; temporary file is deleted. |
| Fallback write/flush failure after `trunc` | Already truncated | `false`; original may be empty or partially rewritten, then temporary file is deleted. |
| Fallback write succeeds | Truncated then rewritten | `true`; new value is persisted, but without atomic replacement. |

The critical transition is the successful open of the original path in the
fallback. `std::ios::trunc` destroys the previous contents before the fallback
has established that the complete replacement was written. A later write,
flush or close-related failure can therefore leave a partial or empty file.

The caller keeps the new policy active in memory and logs
`persistence failed` when `PersistConfigValue` returns `false`. That is useful
session behavior, but it does not protect the on-disk last-known-good config.

## Classification

```text
Config persistence data-loss path       CONFIRMED DEFECT
Temporary-file early-cleanup gap        ARCHITECTURAL DEBT
Config format/defaults                  UNCHANGED
Hotkey in-memory policy behavior        INTENTIONAL / OUT OF SCOPE
```

This is a confirmed defect because it violates the approved safety invariant:

> Persistence failure must not destroy the last known-good config merely
> because saving the new value failed.

The classification does not depend on whether the failure is common in normal
operation. The reachable control flow is sufficient: after a failed atomic
replace, a successful fallback open can truncate the original, and a later
fallback write failure returns `false` after the damage has occurred.

## Bounded failure contract for the next implementation batch

- On every persistence failure, the last-known-good original INI remains
  readable and unchanged.
- A replacement is attempted only after the temporary file has been written
  completely and closed successfully.
- If replacement fails, the function reports failure and cleans up the
  temporary file without opening the original with truncation.
- The config format, key names, defaults and in-memory hotkey behavior remain
  unchanged.
- A failed persistence operation may leave the newly selected policy active
  for the current session, but must report that it was not persisted.
- The next implementation must preserve the existing success path where the
  complete new configuration is installed.

## Non-goals

- No production code changes in Batch 3.4 classification.
- No change to configuration format or defaults.
- No redesign of hotkey policy selection.
- No broad config subsystem cleanup.
- No runtime game test is claimed by this audit.

## Validation and limits

- Current source control flow was inspected in `src/config/config_repository.cpp`
  after the Batch 2 extraction.
- Caller behavior was inspected in `src/plugin/runtime.cpp`.
- No failure injection or filesystem fault harness was added during the initial
  classification phase; the implementation phase added one below.
- Build was deferred during the initial classification phase because production
  code was not changed.

## Bounded implementation result

The direct-write fallback was removed. After a failed `MoveFileExW`, the
function now logs the replacement failure, deletes the staging artifact where
possible and returns `false`; it never opens the live INI with `trunc`.
Temporary-file open/write failures also attempt cleanup before returning.
The source read handle is explicitly closed before staging and replacement so
the normal atomic replacement path can complete successfully on Windows.

Therefore there is no fallback after a failed replacement. Failure is
preferable to destructive recovery, and the caller retains its existing
in-memory behavior while reporting persistence failure.

## Validation

The behavioral harness exercises the production `PersistConfigValue` function
and checks file contents rather than helper names or internal structure:

```text
normal persistence                         PASS
staging/write failure preserves original   PASS
replacement failure preserves original     PASS
```

The main production build, lifecycle harness and feature-status harness were
also run after the implementation and passed. `git diff --check` passed. Full
in-game validation remains part of Batch 4; this batch does not claim
filesystem power-loss guarantees.
