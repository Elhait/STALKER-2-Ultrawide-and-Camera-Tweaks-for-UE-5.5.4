# v1.0.0 Gameplay Modes — Implementation Task Plan

## Objective

Implement two mutually exclusive gameplay correction modes:

- `AspectRecalculation` — existing validated behavior and default;
- `HorPlus` — the runtime-validated native-input FOV transformation path.

## Established evidence and current state

- `AspectRecalculation` is the current validated production gameplay fix.
- HorPlus was runtime-validated for 21:9 and 32:9, 16:9 native bypass,
  dynamic native FOV modifiers and aspect/flag transitions.
- The current config has `Gameplay.Enabled` but no gameplay mode.
- The current production writer hook is validated and must remain the single
  gameplay writer hook.
- Cinematics are a shared existing subsystem and are outside this batch's
  redesign scope.

## Approved scope

- Add `Gameplay.Mode` with default `AspectRecalculation`.
- Parse and persist `AspectRecalculation` and `HorPlus` mode values.
- Dispatch the existing validated gameplay writer to exactly one mode path.
- Preserve the current AspectRecalculation implementation as the default path.
- Add the production HorPlus gameplay path using current native writer input,
  current observed aspect and the validated `cinematics::HorPlus` semantics.
- Keep 16:9 as native identity/bypass and support validated 21:9/32:9 states.
- Keep flags out of the HorPlus multiplier; use only validated eligibility.
- Ensure one native input receives at most one HorPlus transformation.

## Explicit non-goals

- Do not redesign or audit Cinematics in this batch.
- Do not solve or reopen cinematic EXIT ownership/handoff research here.
- Do not modify cinematic aspect/FOV hooks or Dialogue behavior.
- Do not change the AspectRecalculation algorithm, aspect/flags writes or
  existing default behavior.
- Do not add a second gameplay writer hook.
- Do not add caching, timers, polling or a new reverse-engineering anchor.

## Expected files and areas

- `src/config/feature_config.hpp/.cpp`
- `src/config/config_repository.cpp`
- `src/config/config_template.cpp`
- `src/plugin/runtime.cpp`
- `tests/config/` and/or `tests/platform/` for bounded mode/transform tests.
- `test.cmd` only if a new harness is added.

## Implementation batches

1. Add config enum/parser/template support with fail-closed default.
2. Extract or preserve the current AspectRecalculation writer body and add a
   single mode dispatcher.
3. Add production HorPlus writer handling with native-input and single-
   transformation invariants.
4. Add bounded unit/harness coverage and validate the source/config diff.

## Validation

- Config parser/template harness: both values, unknown fallback and default.
- HorPlus pure behavior harness: 16:9 identity, 21:9/32:9 transform, flags do
  not change the multiplier, native input is the only transform input.
- Static search for duplicate writer-hook installation and AspectRecalculation
  calls reachable from the HorPlus dispatch path.
- `test.cmd`, `build.cmd` and `git diff --check`.
- Runtime/cinematic integration remains a later user-run validation gate.

## Risks and rollback / safe failure

- Risk: changing the default path while introducing dispatch.
- Safe failure: unknown mode selects `AspectRecalculation`; failed HorPlus
  initialization leaves gameplay correction unavailable/native rather than
  silently selecting a different correction.
- Rollback: revert only the bounded mode/config/dispatch changes; preserve all
  existing dirty worktree changes.

## Stop conditions and phase gates

- Stop if preserving the existing AspectRecalculation body requires changing
  its algorithm or validated writes.
- Stop if HorPlus requires a second writer hook or new runtime anchor.
- Stop after static/build/harness validation; do not infer cinematic behavior
  from this implementation batch.

## Expected final Git review

- Confirm only mode/config/dispatch/HorPlus implementation and tests changed.
- Confirm Cinematics, Dialogue and AspectRecalculation internals are untouched.
- Report runtime integration as not yet validated rather than blocked or
  excluded from v1.0.
