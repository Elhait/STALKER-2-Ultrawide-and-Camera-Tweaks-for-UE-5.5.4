# Task 1 — HorPlus Telemetry Truthfulness

## Objective

Make diagnostic HorPlus telemetry report the actual production decision and
result for the validated gameplay-writer callback, without changing camera/FOV
behavior.

## Established evidence and current state

- `ReplayManualTransition()` called the production HorPlus path and separately
  called `TraceHorPlusFovState()` before that path ran.
- The diagnostic owner classifier could therefore disagree with the actual
  production transform decision.
- Runtime validation was intentionally deferred until the complete repair
  queue is finished.

## Approved scope

- Refactor the existing HorPlus application boundary only as needed to expose
  its actual eligibility/applied/result to diagnostic telemetry.
- Keep classifier/lifecycle metadata separate from production transform data.
- Preserve change-only logging and existing harness coverage, adding focused
  assertions for decision/result agreement.

## Explicit non-goals

- No Dialogue classifier, recovery, candidate, or policy changes.
- No Cinematic, AspectRecalculation, ZOOM, ADS, or mode-switching changes.
- No post-exit trace repair, hot-path optimization, or configured-FOV search.
- No stable release replacement, Git commit, or runtime game launch.

## Actual files changed

- `src/gameplay/horplus_gameplay.hpp`
- `src/gameplay/horplus_gameplay.cpp`
- `src/plugin/runtime.cpp`
- `tests/gameplay/horplus_gameplay_harness.cpp`
- this completed plan
- `backlog/TASKLOG.md`

## Implementation and validation

- Added a shared `EvaluateHorPlus()` result boundary used by both the
  production writer path and the existing `TryTransformHorPlus()` wrapper.
- `ApplyHorPlusGameplay()` now returns the actual production decision/result;
  diagnostic telemetry consumes that result after application.
- Classifier/lifecycle owner metadata remains separate from the production
  HorPlus decision.
- Cinematic-applied samples do not establish gameplay cache validity.
- Added harness assertions for eligible ultrawide, 16:9 identity bypass,
  invalid flags, invalid FOV and invalid aspect.
- Full `test.cmd` suite passed.
- Diagnostic build `STALKER2CameraTweaks_ZoomHorPlusDiagnostic.asi` passed;
  existing Zydis C4201 warnings remain.
- `git diff --check` passed.

## Final status

- D3: `FIXED STATICALLY / HARNESS VALIDATED`
- Production behavior: unchanged by source intent and harness coverage.
- Runtime: not requested and not performed.
- Stable production ASI: not replaced.
- Dialogue, Cinematics, AspectRecalculation, ZOOM and mode switching:
  unchanged and out of scope.

## Stop condition

Task 1 is complete. Do not begin Task 2 automatically.
