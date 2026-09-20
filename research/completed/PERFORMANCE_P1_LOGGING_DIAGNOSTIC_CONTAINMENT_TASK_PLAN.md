# Performance P1 — Logging and Diagnostic Containment Task Plan

## Objective

Remove confirmed synchronous and ungated diagnostic overhead from normal
runtime execution while preserving production camera behavior and existing
diagnostic semantics when Diagnostics.Enabled=true.

## Established evidence and current state

- The logger flushes every info record synchronously.
- Detailed camera-mode probes perform SafeRead/VirtualQuery work before the
  diagnostics gate.
- Detailed gameplay FOV trajectory logging is not fully gated.
- High-rate FOV, ZOOM and cinematic-writer telemetry must not prepare or emit
  records when diagnostics are disabled.

## Approved scope

- Remove per-info synchronous flush; retain explicit flush at initialization,
  errors/fatal events and shutdown where the current code has such boundaries.
- Gate detailed camera-mode probes before all SafeRead/VirtualQuery work.
- Gate detailed gameplay FOV trajectory records behind Diagnostics.Enabled.
- Ensure high-rate FOV, ZOOM and cinematic-writer diagnostic work returns before
  diagnostic-only mutexes, probes, formatting and logging when disabled.
- Add or adjust deterministic harness coverage for the gates where practical.

## Explicit non-goals

- No GameplayBaseline, HorPlus, cinematic or Dialogue behavior changes.
- No coordinator, hook location/mechanism or resolver changes.
- No production/diagnostic build split; that is a later P2 batch.
- No mutex, SafetyHook, compiler optimization or HorPlus-math optimization.
- No runtime game launch or performance claim.

## Expected files or areas

- `src/plugin/runtime.cpp`
- `src/diagnostics/diagnostic_runtime.*` if required by existing gate APIs
- relevant diagnostics/test harnesses
- `test.cmd`, if harness wiring changes
- implementation report and `backlog/TASKLOG.md`

## Validation

- Relevant deterministic harnesses.
- Full `test.cmd`.
- `build.cmd`.
- `git diff --check`.
- Read-only Git review against this plan.
- No game launch.

## Risks and safe failure

- Diagnostics enabled must retain current diagnostic records and provenance.
- Diagnostics disabled must preserve production output and return before
  diagnostic-only probes, locks and formatting.
- If a proposed gate changes production lifecycle or camera output, stop and
  revert that part of the bounded batch.

## Stop conditions

- Stop after P1 validation and report.
- Do not begin P2 build separation or P3 measurement work.

## Final review

Compare all changed paths with this plan, separate pre-existing worktree
changes, and record runtime validation as not performed.
