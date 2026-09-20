# ZOOM Transition 2.0.6 Hash Repair Task Plan

## Objective

Update the existing neutral ZOOM diagnostic executable identity gate from the
validated 2.0.5 image hash to the current Steam 2.0.6 image hash observed in
the user's runtime log.

## Established evidence

- Current 2.0.6 runtime identity:
  `61BC1E030740CEBC30CF1DAD0C86CF65E39E12FF0500225821D684181E08D56B`.
- The diagnostic ASI reported a hash mismatch before installing either ZOOM
  hook.
- Gameplay, Cinematics and Dialogue hooks installed successfully in the same
  run, so this is isolated to the diagnostic ZOOM identity gate.

## Approved scope

- Change only the `ZOOM_TRANSITION_DIAGNOSTIC` hash constant.
- Rebuild the separate `STALKER2CameraTweaks_ZoomTransitionDiagnostic.asi`.
- Run harness/build/diff validation.

## Non-goals

- No signature changes or resolver weakening.
- No ZOOM behavior, Dialogue, HorPlus, Cinematics or gameplay changes.
- No game launch in this repair batch.

## Validation and stop condition

- Full harness suite passed.
- Diagnostic build passed.
- `git diff --check` passed.
- Stop before runtime; the rebuilt artifact is the only handoff for the next
  2.0.6 game test.
