# v1.0 Aspect Policy Testability — Task Plan

## Objective

Expose the shared pure aspect predicates used by production so automated tests validate the actual `AspectRecalculation` constrained-state branch and HorPlus eligibility without duplicating their conditions in a harness.

## Established evidence and current state

- Production now uses a generic ultrawide threshold rather than exact 21:9/32:9 values for runtime eligibility.
- `AspectRecalculation` constrained normalization requires ultrawide aspect plus `flags == 0x5`.
- HorPlus accepts ultrawide aspect with either `flags == 0x4` or `0x5`; flags do not change the multiplier.
- The predicates are currently private/embedded, preventing direct production-predicate sweep coverage.

## Approved scope

- Add a small shared pure aspect-policy helper with one canonical ultrawide threshold and a constrained-state predicate.
- Use the helper from HorPlus and the `AspectRecalculation` runtime/diagnostic gates.
- Add test harness coverage for exact threshold boundaries, arbitrary aspect sweep, named regression aspects and separate `0x4`/`0x5` semantics.
- Preserve all existing operations and state transitions.

## Explicit non-goals

- No changes to normalization writes, replay choreography, hook ownership or FOV formulas.
- No changes to forced cinematic target policy.
- No runtime/game launch or ASI runtime validation.
- No modernization of rejected historical diagnostics.

## Files or areas expected to be touched

- `src/gameplay/aspect_policy.hpp/.cpp`
- `src/gameplay/horplus_gameplay.cpp`
- `src/plugin/runtime.cpp`
- `tests/gameplay/aspect_policy_harness.cpp`
- `test.cmd`
- `build.cmd`

## Implementation batches

### Batch 1 — Shared predicates

- Add `IsUltrawideAspect(aspect, nativeAspect)` using the existing `+0.001f` threshold semantics.
- Add `IsConstrainedUltrawideAspect(aspect, flags, nativeAspect)` composing the shared predicate with `flags == 0x5`.
- Replace duplicate HorPlus/runtime predicate expressions with the shared helpers.

Validation: source review and targeted compile.

### Batch 2 — Automated coverage

- Add boundary tests below, at and above `nativeAspect + 0.001f`.
- Sweep arbitrary aspects from `1.00` through `4.00` using integer ticks.
- Verify HorPlus flags `0x4/0x5` separately from AspectRecalculation constrained `0x5` semantics.
- Add named regression values `2.37037`, `2.38889`, `2.4`, `3.2`, `3.55556`.

Validation: relevant harness, full `test.cmd`, `build.cmd`, and `git diff --check`.

## Risks and rollback / safe-failure behavior

- Risk: changing the threshold accidentally. Mitigation: preserve literal `aspect > nativeAspect + 0.001f` semantics and test equality explicitly.
- Risk: conflating HorPlus and AspectRecalculation flag contracts. Mitigation: keep separate helpers/tests.
- Invalid/non-finite values remain false; no production write occurs from predicates alone.
- Rollback is limited to reverting this bounded refactor after review; no destructive Git operation is permitted.

## Stop conditions and phase gates

- Stop if predicate extraction requires changing state-machine behavior.
- Stop if forced cinematic targets or rejected probes would need modification.
- Stop after static/build/harness review; runtime remains a separate user-run gate.

## Expected final Git review

- Confirm only the planned helper, integration, harness and build/test registration paths changed, plus archived plan/task log.
- Confirm the actual normalization and HorPlus operations are unchanged.
- Report automated validation separately from runtime validation.
