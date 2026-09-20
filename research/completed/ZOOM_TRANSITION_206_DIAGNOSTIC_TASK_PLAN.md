# ZOOM Transition 2.0.6 Diagnostic Task Plan

## Objective

Build a separate neutral `ZOOM_IN`/`ZOOM_OUT` diagnostic ASI using the
existing read-only Wideboy-derived transition hooks, and expose startup
resolver validation for both boundaries.

## Established evidence and current state

- ZOOM hooks are neutral gameplay transition signals, not Dialogue, ADS-owner,
  HorPlus or cinematic ownership signals.
- Existing `ZOOM_TRANSITION_DIAGNOSTIC` already records change-driven
  `ZOOM_IN`/`ZOOM_OUT` edges and a shutdown summary.
- The current 2.0.5 diagnostic used unique paired signatures and a validated
  post-load instruction contract.
- The current 2.0.6 Dialogue run did not enable this telemetry, so ZOOM
  compatibility on 2.0.6 is not yet established.

## Approved scope

- Use the existing `build-zoom-transition-diagnostic.cmd` path.
- Preserve neutral edge telemetry and startup resolver/install status.
- Confirm the diagnostic artifact is separate from the stable production ASI.
- Run existing harnesses, production-independent build checks and diff review.

## Explicit non-goals

- No ZOOM behavior changes or direct HorPlus transformation.
- No Dialogue, ADS-owner, Cinematic, AspectRecalculation or Gameplay mode
  changes.
- No new resolver, signature or hook design in this batch.
- No Ghidra analysis of 2.0.6.
- No game launch in this batch.

## Expected files or areas

- Existing `src/plugin/runtime.cpp` diagnostic path only if a static review
  finds a necessary telemetry correction.
- `build-zoom-transition-diagnostic.cmd`.
- Diagnostic output `STALKER2CameraTweaks_ZoomTransitionDiagnostic.asi`.
- This plan and `backlog/TASKLOG.md`.

## Batches

1. Read-only audit of the existing neutral ZOOM diagnostic and startup install
   logging.
2. Build the separate diagnostic ASI; do not replace the stable ASI.
3. Run the full harness suite and `git diff --check`.
4. Perform read-only Git/path review and archive this plan.

## Validation

- `test.cmd` must pass.
- `build-zoom-transition-diagnostic.cmd` must complete successfully.
- Artifact must exist under the diagnostic name.
- No production source behavior changes are permitted.
- Runtime compatibility remains pending until a separate 2.0.6 game run with
  ADS in/out and controller pull/release.

## Risks and safe failure

- Hash or signature mismatch must fail closed and be logged by the diagnostic;
  do not weaken validation.
- Hook installation failure must roll back the paired diagnostic hook.
- Any unexpected source change stops this batch for review.

## Stop conditions and final review

- Stop after build/harness/diff review; do not launch the game.
- Confirm only the approved diagnostic artifact/plan/task-log paths changed.
- Record runtime-not-validated status explicitly.
