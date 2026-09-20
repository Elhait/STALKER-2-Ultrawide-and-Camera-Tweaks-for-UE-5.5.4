# HorPlus FOV and ZOOM Combined 2.0.6 Diagnostic Task Plan

## Objective

Build the combined diagnostic ASI that includes both the neutral ZOOM
transition telemetry and the three-layer HorPlus FOV-state telemetry.

## Established evidence

- The previous runtime artifact enabled only `ZOOM_TRANSITION_DIAGNOSTIC`.
- Therefore it correctly logged ZOOM edges but did not emit the configured,
  native-input and HorPlus-output FOV records.
- `build-horplus-fov-state-diagnostic.cmd` already enables both required
  diagnostic macros and reuses the current source.

## Approved scope

- Use the existing combined diagnostic builder.
- Include current source fixes and the 2.0.6 ZOOM identity gate.
- Preserve all production behavior; this is diagnostic-only.

## Non-goals

- No source behavior changes.
- No Dialogue, HorPlus, Cinematic, AspectRecalculation or ZOOM logic changes.
- No game launch in this batch.

## Validation

- Build the combined diagnostic artifact.
- Run `git diff --check` and path review.
- Runtime validation remains the next user-run step.
