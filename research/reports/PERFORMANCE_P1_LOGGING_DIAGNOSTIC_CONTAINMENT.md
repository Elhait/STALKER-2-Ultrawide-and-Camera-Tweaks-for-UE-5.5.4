# Performance P1 — Logging and Diagnostic Containment

Date: 2026-09-19

## Scope

This batch contained confirmed logging and diagnostic overhead hazards. It did
not change camera behavior, FOV math, GameplayBaseline semantics, hook
locations or state machines.

## Changes

### Buffered normal logging

Per-info `flush_on(info)` was removed. The logger now flushes automatically
only for error-level records. Explicit flushes remain at initialization
completion, initialization failure handling and shutdown.

Normal informational records therefore no longer force synchronous file flushes
from camera callbacks.

### Camera-mode diagnostics

`LogCameraModeChange()` now checks `diagnostics::Enabled()` before any
diagnostic memory reads. With diagnostics disabled it returns immediately, so
the seven detailed `SafeRead`/`VirtualQuery` probes and associated
formatting are skipped.

Its result is diagnostic-only: the caller uses it only to decide whether to
emit transition snapshots and combined diagnostic records. Production aspect,
FOV and replay decisions remain outside this function.

### Detailed gameplay FOV trace

`LogGameplayFovSourceChange()` now returns before comparing values,
locking the Dialogue mutex or formatting a record when diagnostics are
disabled. High-frequency FOV trajectory logging is therefore diagnostic-only.

### Existing high-rate gates

The static audit confirmed that the existing high-rate paths already return
early when diagnostics are disabled:

- ZOOM transition tracing;
- HorPlus FOV-state telemetry;
- cinematic writer tracing;
- CameraState snapshot telemetry.

No production behavior was added to those paths in this batch.

## Non-goals preserved

- GameplayBaseline publication and projection remain unchanged.
- HorPlus and cinematic math remain unchanged.
- Dialogue, recovery, coordinator and ownership semantics remain unchanged.
- SafetyHook mechanism and hook locations remain unchanged.
- The production/diagnostic build split is deferred to P2.
- Mutex/store, SafetyHook context, compiler optimization and HorPlus math were
  not optimized speculatively.

## Validation

- Full `test.cmd`: PASS.
- Existing diagnostics gate and all camera/gameplay/dialogue harnesses: PASS.
- `build.cmd`: PASS.
- Existing external Zydis C4201 warnings remain.
- `git diff --check`: PASS; only normal line-ending warnings reported.
- Game launch and FPS/frametime measurement: NOT PERFORMED.

## Status

`P1 logging/diagnostic containment: FIXED STATICALLY / HARNESS VALIDATED`.

The batch removes confirmed synchronous flush and ungated diagnostic probe
paths. It does not yet prove a measured FPS or frametime improvement.

## Deferred

P2 may separately introduce a production/diagnostic build policy. The
measurement-driven items remain deferred until profiling evidence exists:

- GameplayBaseline and observation-store mutex cost;
- SafetyHook context-save overhead;
- /O1 versus /O2;
- duplicate Auto viewport queries;
- any broader diagnostic sampling/buffering architecture.
