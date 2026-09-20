# Task 3 — Neutral ZOOM Transition Cleanup

## Objective

Remove the stale ADS-specific ownership integration from current code and
retain the validated Wideboy-derived hooks as read-only neutral
`ZOOM_IN`/`ZOOM_OUT` gameplay transition signals.

## Established evidence and implementation

- The two hooks are generic native gameplay zoom-transition signals; callback
  `XMM0` is transition weight, not camera FOV.
- Removed current `AdsLifecycle`, `AdsDirection`, ADS owner state and the
  Dialogue suppression gate derived from them.
- Renamed current source/build identifiers to neutral
  `ZoomTransition`/`ZOOM_*` vocabulary.
- Kept read-only ZOOM telemetry and attribution evidence in historical reports;
  no direct ZOOM HorPlus transform was added.
- HorPlus remains owned solely by the validated gameplay writer.

## Explicit non-goals

- No Dialogue D1 repair or positive discriminator.
- No HorPlus math or Task 1 telemetry contract changes.
- No Cinematic, AspectRecalculation, mode-switching or writer optimization.
- No runtime launch, Git commit, release or stable ASI replacement.

## Actual paths changed

- `src/plugin/runtime.cpp`
- `build.cmd`
- `build-zoom-transition-diagnostic.cmd`
- `build-zoom-horplus-diagnostic.cmd`
- `build-horplus-fov-state-diagnostic.cmd`
- `test.cmd`
- removed current-only `src/dialogue/ads_lifecycle.*` and
  `tests/dialogue/ads_lifecycle_harness.cpp`
- this completed plan and `backlog/TASKLOG.md`

## Validation

- Full `test.cmd` suite passed.
- Neutral Zoom/HorPlus diagnostic build passed with the existing Zydis C4201
  warnings.
- Normal production configuration compile passed to a separate task artifact;
  the stable production ASI was not replaced.
- `git diff --check` passed.
- Current source/tests/build-script search found no stale ADS-owner references.
- No game launch or runtime validation was performed.

## Final status

- D6: `FIXED STATICALLY / HARNESS VALIDATED`
- ZOOM semantics: `NEUTRAL GAMEPLAY TRANSITION SIGNAL`
- ADS-specific ownership: `REMOVED FROM CURRENT ARCHITECTURE`
- Direct HorPlus from ZOOM: `NONE`
- Dialogue coupling: `NONE`
- Runtime: deferred to the combined regression session.

## Stop condition

Task 3 is complete. Do not begin Task 4 automatically.
