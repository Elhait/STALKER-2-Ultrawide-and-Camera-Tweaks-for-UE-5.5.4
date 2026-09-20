# Task 2 — Post-Cinematic Raw Writer Trace Containment

## Objective

Remove permanent raw post-exit writer instrumentation from the normal
production gameplay-writer path while preserving functional cinematic recovery
and `PostCinematicRecoveryExclusion` behavior.

## Established evidence and current state

- `ReplayManualTransition()` called `ObservePostExitRawWriterEntry()` without
  a diagnostic compile guard.
- `TraceCinematicExit()` armed `g_postExitTraceArmed` on every cinematic exit.
- The raw trace had no normal terminal disarm path and performed multiple
  guarded reads, snapshot comparison and possible logging after the first exit.
- `ObservePostCinematicDialogueRecovery()` is a separate functional exclusion
  path and remains unchanged.

## Approved scope and non-goals

- Compile-gate the raw post-exit trace and its state.
- Preserve explicit research trace consumers behind diagnostic macros.
- No Dialogue, HorPlus, ZOOM/ADS, Cinematic behavior, AspectRecalculation,
  ReplayState or mode-switching changes.
- No runtime launch, release replacement, Git commit or performance claim.

## Actual implementation

- Added `POST_EXIT_TRACE_STATE` as the shared compile-time state gate for raw
  post-exit research instrumentation.
- Raw snapshot/observation and its writer call are now available only under
  explicit raw-trace diagnostic macros.
- Normal builds no longer arm raw post-exit state and log EXIT with
  `postExitTrace=disabled`.
- Functional `ArmPostCinematicDialogueExclusion()` and
  `ObservePostCinematicDialogueRecovery()` remain on the normal writer path.

## Validation

- Full `test.cmd` suite passed.
- Diagnostic build `STALKER2CameraTweaks_ZoomHorPlusDiagnostic.asi` passed;
  existing Zydis C4201 warnings remain.
- `git diff --check` passed.
- No game launch or runtime validation was performed.

## Final status

- D4: `FIXED STATICALLY / HARNESS VALIDATED`
- Hot-path work: raw post-exit SafeRead/snapshot/logging path removed from the
  normal production writer path.
- Functional recovery/exclusion: intended unchanged.
- Performance impact: not measured.
- Runtime: deferred to the combined regression session.

## Stop condition

Task 2 is complete. Do not begin Task 3 automatically.
