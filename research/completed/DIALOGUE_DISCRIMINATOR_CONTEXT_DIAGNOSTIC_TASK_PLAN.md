# Dialogue Discriminator Context Diagnostic — Task Plan

## Objective

Collect bounded, read-only context telemetry at the validated DialogueBoundary
mid-hook to compare generic FOV transitions, cinematic EXIT descent and real
dialogue without treating the current stack as caller evidence.

## Established evidence and current state

- The DialogueBoundary classifier is a broad FOV-descent heuristic.
- Runtime logs confirmed false-positive Candidate/Active transitions.
- The current mid-hook exposes register/XMM context, but `context.rsp` does
  not provide a validated caller address.
- `[RSI+0x2C]` is a source-derived candidate field, not yet semantically
  identified.

## Approved scope

- Diagnostic-only changes guarded by `DIALOGUE_BOUNDARY_DIAGNOSTIC`.
- Log XMM6/XMM1, RCX/RDX/RSI, receiver/vtable reads, `[RSI+0x2C]`, raw FOV,
  coordinator, recovery state, Dialogue phase and policy.
- Build a separate diagnostic ASI for one user-run correlation test.

## Explicit non-goals

- Do not change Dialogue classification, thresholds, lifecycle or FOV math.
- Do not change HorPlus, Cinematics, AspectRecalculation, hotkeys or hooks.
- Do not infer or log a caller from `[RSP]`.
- Do not add a new hook, resolver, observer or production setting.
- Do not launch the game.

## Validation and stop condition

- Diagnostic build completed successfully.
- `git diff --check` passed.
- Stop after preparing the ASI; runtime validation remains user-run.

## Expected final Git review

Only diagnostic telemetry and this task record are in scope; the result is
build-validated but not runtime-validated.
