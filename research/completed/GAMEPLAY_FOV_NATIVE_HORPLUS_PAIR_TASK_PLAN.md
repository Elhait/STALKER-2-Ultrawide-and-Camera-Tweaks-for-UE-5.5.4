# Gameplay FOV Native/HorPlus Pair Telemetry — Task Plan

## Objective

Make diagnostic gameplay FOV records explicitly preserve the native writer
sample before HorPlus and the exact HorPlus result derived from that same
sample.

## Established evidence and current state

- `HorPlusGameplayApplyResult` already contains the native input and derived
  output in the gameplay writer path.
- Existing diagnostics expose `nativeNew` and `horPlusNew`, but the names do not
  make the atomic before/after transformation contract explicit.
- Stable native gameplay endpoints exist, while stable samples alone do not
  establish baseline ownership.

## Approved scope

- Diagnostic log schema only for the existing change-driven FOV record.
- Use the existing `applyResult.inputFov` and `applyResult.outputFov` values.
- Update the related research report to describe the pair contract.

## Explicit non-goals

- No `ExpectedGameplayFOV` INI parameter.
- No baseline classifier or production baseline cache.
- No MatchGameplay implementation.
- No changes to HorPlus, Dialogue, Cinematics, ZOOM, CameraState ownership or
  recovery behavior.
- No new hooks, resolver changes, runtime launch or Ghidra analysis.

## Expected files

- `src/plugin/runtime.cpp`
- `research/reports/STABLE_GAMEPLAY_BASELINE_DIAGNOSTIC.md`
- this plan and, after completion, the bounded task record if required.

## Batches and validation

1. Rename/add explicit diagnostic fields at the existing FOV event:
   `nativeBeforeHorPlus` and `horPlusResult`, preserving existing fields for
   compatibility if practical. Validate source diff and compile.
2. Update the diagnostic report with the native/result pair invariant.
3. Run relevant harnesses, `full test.cmd`, production build and
   `git diff --check`.
4. Perform read-only Git review against this plan.

## Risks and safe failure

- Diagnostic field changes must not alter the values passed to the game.
- If the HorPlus result is unavailable for a bypassed sample, record it as
  unavailable rather than substituting the native input.
- Existing log fields should remain where possible to avoid making prior
  evidence unreadable.

## Stop conditions

- Stop if the pair cannot be sourced from the same `HorPlusGameplayApplyResult`.
- Stop if implementation requires production-state or hook changes.
- Stop after static/harness/build validation; runtime is deferred to the next
  combined session.

## Final review

Confirm only the approved diagnostic source/log/report paths changed, the pair
uses one native sample and one derived result, and no baseline inference was
introduced.
