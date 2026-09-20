# Stable Gameplay Baseline Diagnostic Task Plan

## Objective

Prepare reusable, diagnostic-only telemetry for one runtime experiment that
tests whether stable Gameplay writer endpoints track user-selected FOV values
across transient zoom modifiers. Add the already planned CinematicActive guard
telemetry to the same canonical diagnostics path.

## Established evidence and current state

- Writer `XMM0` is a transient camera FOV observation, not an established
  configured-FOV source.
- Existing diagnostics already log native FOV changes, source, owner,
  presentation-related state, ZOOM last-observation evidence and event order.
- `lastZoomDirection` is not a current zoom-state signal.
- Binocular/internal magnification may bypass the observed ZOOM hooks.
- `ConfiguredGameplayFov` remains `UNKNOWN`; hardcoded `90` is forbidden.
- Task 9 has a static provenance gap but no confirmed runtime defect.

## Approved scope

1. Inspect existing diagnostic output and add only the minimum reusable
   diagnostic fields needed to reconstruct ordered Gameplay baseline samples.
2. Keep any stable-baseline notion diagnostic-only and explicitly distinct from
   `ConfiguredGameplayFov` and production ownership.
3. Add change-only CinematicActive writer telemetry for the Task 9 matrix:
   writer input, cached ENTER value, current/ENTER aspect, cache validity,
   numeric guard result, HorPlus decision and output.
4. Gate all new telemetry through `[Diagnostics] Enabled=true`.
5. Add focused harness coverage for diagnostic-only state transitions and
   telemetry contracts where practical.

## Explicit non-goals

- No MatchGameplay implementation or formula.
- No production baseline cache, classifier or camera decision.
- No use of `lastZoomDirection` as `zoomActive`.
- No assumption that absence of ZOOM means idle or baseline.
- No hardcoded configured FOV.
- No current-image Ghidra/EXE analysis.
- No Dialogue, Candidate, ZOOM or cinematic behavior changes.
- No game launch in this batch.

## Expected files or areas

- `src/plugin/runtime.cpp` diagnostic-only telemetry.
- `build.cmd` supported diagnostics define only if required by existing
  Task 9 trace code.
- focused diagnostics harnesses and `test.cmd` if a pure helper is added.
- `research/reports/STABLE_GAMEPLAY_BASELINE_DIAGNOSTIC.md`.
- `backlog/TASKLOG.md`.

## Implementation batches

### Batch 1 — Existing telemetry audit

Confirm which current fields already expose ordered native Gameplay samples,
source identity, owner/context and ZOOM observation limitations. Do not add
fields that duplicate existing truth.

### Batch 2 — Diagnostic-only baseline observation

Add only change-driven observation metadata needed to identify repeated stable
Gameplay samples. It must never update HorPlus, Dialogue, Cinematics or mode
state. Any candidate value remains a hypothesis and is labeled as such.

### Batch 3 — Task 9 telemetry integration

Compile and runtime-gate the existing CinematicActive writer trace or an
equivalent minimal change-only record so the next cinematic run can compare
writer input against cached ENTER output and current aspect.

### Batch 4 — Harness/build/review

Run focused/full harnesses, canonical build, `git diff --check`, and read-only
Git review. Stop before runtime.

## Validation

- Focused diagnostics/config harnesses.
- Full `test.cmd`.
- Canonical `build.cmd`.
- `git diff --check`.
- No game launch or Ghidra analysis.

## Risks and safe-failure behavior

- New telemetry must be read-only and default-off.
- Invalid or unavailable context logs as unavailable; it must not become a
  baseline candidate or alter production behavior.
- Last-observation ZOOM data must remain labeled as observation, not state.
- If Task 9 trace cannot be integrated without broadening scope, leave it out
  and report the missing telemetry rather than changing the guard.

## Stop conditions and phase gates

- Stop if a proposed baseline field would influence production behavior.
- Stop if the design requires treating no-ZOOM as idle.
- Stop if current source does not support safe change-only telemetry.
- Stop after static/harness/build validation.

## Expected final Git review

Confirm that only diagnostics, harness/build wiring, report, task log and the
archived plan changed. Record the runtime matrix: Gameplay FOV 80, 100 and
110, each with idle → ADS → return → idle, plus a cinematic scenario for Task
9 telemetry.
