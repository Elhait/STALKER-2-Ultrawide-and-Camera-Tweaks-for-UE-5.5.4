# Diagnostic Hot-Path Logging Cleanup — Task Plan

## Objective

Remove per-callback Dialogue `EARLY_CB` log spam from diagnostic builds while preserving meaningful lifecycle/FOV telemetry and aggregate callback statistics.

## Established evidence and current state

- The Dialogue boundary is a high-frequency native callback.
- Current logs contain repeated `EARLY_CB DIALOGUE ENTER` and `EARLY_CB DIALOGUE RETURN 4` lines for unchanged state.
- Existing lifecycle and change-driven FOV records already provide the useful timeline.
- This is an instrumentation-only cleanup; Dialogue behavior and return paths are not being redesigned.

## Approved scope

- Remove per-hit `EARLY_CB DIALOGUE ENTER/RETURN N` log writes.
- Add diagnostic-only counters for total callback count and relevant return reasons.
- Emit one summary at runtime resource shutdown when diagnostic logging is enabled.
- Preserve existing meaningful `DIALOGUE_FOV_CHANGE`, candidate/lifecycle and recovery logs.

## Explicit non-goals

- No Dialogue classifier or state-machine changes.
- No change to FOV transformation or return behavior.
- No changes to Cinematics, Gameplay, HorPlus or ZOOM callbacks.
- No stable release behavior changes.
- No runtime game launch in this batch.

## Expected files or areas

- `src/plugin/runtime.cpp`
- relevant local validation/harnesses if required
- this plan and task log entry

## Batches and validation

### Batch 1 — Diagnostic-only source cleanup

- Replace per-hit Dialogue logging with counters under the existing diagnostic guard.
- Emit a single summary during runtime resource cleanup.
- Confirm all functional branches retain their original return behavior.

Validation: source diff review and `git diff --check`.

### Batch 2 — Build and harness validation

- Run the existing complete harness suite.
- Build the merged Zoom/HorPlus diagnostic candidate without overwriting the stable ASI.

Validation: all harnesses pass and build completes; no runtime claim.

### Batch 3 — Post-change review

- Inspect Git status and relevant diff.
- Confirm only approved instrumentation paths changed.
- Record that live log compactness remains not runtime-validated until the next user run.

## Risks and safe-failure behavior

- Counter instrumentation must not acquire locks or alter callback decisions.
- If summary logging fails, native and Dialogue behavior remain unchanged.
- If the diagnostic build fails, retain the previous candidate and do not touch the stable ASI.

## Stop conditions

- Stop if any Dialogue branch condition, state transition or output assignment changes.
- Stop if Dialogue/Cinematic/Gameplay files outside the approved logging area are modified.

## Expected final Git review

- `git status --short`
- relevant diff and `git diff --check`
- report completed, remaining, deferred and not-runtime-validated items separately.
