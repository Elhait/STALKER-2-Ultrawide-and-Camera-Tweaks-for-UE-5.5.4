# v1.0 HorPlus Dynamic Aspect Eligibility — Task Plan

## Objective

Remove HorPlus production and diagnostic eligibility dependence on exact canonical aspect values such as `21:9` and `32:9`. HorPlus must accept any valid effective aspect wider than native 16:9, including the actual aspect produced by common 21:9 resolutions such as `3440x1440` (`2.38889`), while preserving native 16:9 identity/bypass and the existing HorPlus transformation semantics.

## Established evidence and current state

- HorPlus runtime behavior passed for tested 32:9 and 21:9 states, ADS transitions, and flags `0x4`/`0x5`.
- The latest 21:9 resolution produced runtime aspect `2.38889`, not exactly `21.0 / 9.0` (`2.33333`).
- Current HorPlus helper and diagnostic eligibility still contain exact `21:9`/`32:9` comparisons.
- `AspectRecalculation` is validated/frozen and is outside this repair.

## Approved scope

- Make HorPlus eligibility generic for finite effective aspects greater than native 16:9.
- Keep flags limited to the existing accepted eligibility set (`0x4`/`0x5`); flags must not select a different multiplier.
- Update bounded HorPlus harness coverage for actual and non-canonical ultrawide aspects.
- Update diagnostic-only HorPlus eligibility to use the same generic rule where it is part of the current HorPlus POC path.
- Preserve all existing transformation, hook, coordinator, cinematic, dialogue, scanner and AspectRecalculation behavior.

## Explicit non-goals

- No change to `AspectRecalculation` implementation or its exact historical state handling.
- No change to HorPlus mathematics.
- No new mode, setting, hook, resolver or observer.
- No cinematic EXIT redesign or Dialogue repair.
- No runtime test or game launch by the agent.

## Expected files or areas

- `src/gameplay/horplus_gameplay.cpp`
- HorPlus diagnostic eligibility in `src/plugin/runtime.cpp`, only if required to remove the same fixed-aspect POC gate.
- `tests/gameplay/horplus_gameplay_harness.cpp`

## Implementation batches

### Batch 1 — Generic HorPlus eligibility

- Replace exact `21:9`/`32:9` eligibility checks in the HorPlus helper with the established generic ultrawide predicate.
- Ensure invalid/non-finite/native-or-narrow aspects remain fail-closed and flags remain validation-only.
- Remove fixed-aspect gating from the HorPlus diagnostic path without changing diagnostic-only behavior beyond eligibility.

Validation: targeted HorPlus harness and source diff review.

### Batch 2 — Regression validation and review

- Add coverage for `2.38889` and another non-canonical ultrawide aspect with both `0x4` and `0x5`.
- Confirm 16:9 identity/bypass and invalid flags remain rejected.
- Run the approved build, relevant harnesses, `test.cmd`, and `git diff --check`.
- Perform read-only Git review against this plan.

## Risks and rollback / safe-failure behavior

- Risk: accepting an invalid or narrow aspect could apply HorPlus where native behavior is required. Mitigation: retain finite-value and `aspect > nativeAspect` checks.
- Risk: accidentally changing frozen AspectRecalculation behavior. Mitigation: do not modify its state/replay branches or constants.
- On invalid input or unsupported flags, HorPlus remains identity/pass-through and does not write the register.
- Rollback is limited to reverting the bounded source/test changes after review; no destructive Git operation is permitted.

## Stop conditions and phase gates

- Stop if generic eligibility would require changing HorPlus mathematics or frozen AspectRecalculation semantics.
- Stop if unrelated files or behavior change.
- Stop after Batch 2 report; runtime validation remains a user-run step.

## Expected final Git review

- Confirm only approved source and harness paths changed, plus this plan and its permitted archive/task-log follow-up.
- Confirm `AspectRecalculation`, Cinematics, Dialogue and configuration defaults are untouched.
- Report build/test results separately from runtime validation, which is not performed by the agent.
