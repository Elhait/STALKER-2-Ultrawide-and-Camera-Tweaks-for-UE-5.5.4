# v1.0 AspectRecalculation Dynamic Aspect — Task Plan

## Objective

Remove the exact `3440x1440` aspect gate from the validated AspectRecalculation runtime-state normalization branch so custom and alternate ultrawide resolutions use the same established normalization mechanism.

## Established evidence and current state

- HorPlus already accepts any finite aspect wider than native 16:9.
- Runtime evidence established the actual `2.38889` 21:9-class aspect and
  `3.55556` 32:9 aspect in the tested sessions. Harness coverage additionally
  validates representative custom aspects `2.37037`, `2.4` and `3.2`.
- `runtime.cpp:1264` currently recognizes only `kCinemaAspect` plus constrained flag `0x5` before performing the existing native-aspect normalization.
- `kCinemaAspect` and `kWideAspect` remain valid explicit forced cinematic targets.

## Approved scope

- Replace only the `1264` physical-aspect detection gate with `IsUltrawideAspect(aspect) && flags == 0x5`.
- Generalize the two compile-time diagnostic gates that test the same constrained ultrawide runtime state.
- Preserve the existing normalization write, two-pass replay state machine, FOV ownership and forced cinematic target values.
- Extend bounded harness coverage to canonical and custom ultrawide aspects with flags `0x4`/`0x5` where applicable.

## Explicit non-goals

- No change to `ResolveAspect()` forced `16:9/21:9/32:9` policy targets.
- No HorPlus mathematical or hook changes.
- No scanner, Cinematics, Dialogue or coordinator redesign.
- No modernization of the rejected delayed-aspect diagnostic.
- No game launch or runtime validation by the agent.

## Files or areas expected to be touched

- `src/plugin/runtime.cpp`
- `tests/gameplay/horplus_gameplay_harness.cpp`

## Implementation batches

### Batch 1 — Generalize runtime-state gates

- Update the production constrained normalization predicate.
- Update only matching diagnostic readiness/trigger predicates.
- Leave explicit forced target constants and native restore checks unchanged.

Validation: static inventory and diff review.

### Batch 2 — Regression coverage

- Add representative custom aspect values `2.37037`, `2.38889`, `2.4`, `3.2` and `3.55556`.
- Confirm both accepted constrained/unconstrained flags where the shared HorPlus eligibility contract applies.
- Confirm native 16:9 bypass and invalid flags remain fail-closed.

Validation: relevant harness, full `test.cmd`, `build.cmd`, and `git diff --check`.

## Risks and rollback / safe-failure behavior

- Risk: applying normalization to a constrained state that is not equivalent to the prior special case. Mitigation: retain the required `flags == 0x5` state predicate and preserve the exact existing write/choreography.
- Risk: altering forced cinematic framing. Mitigation: do not modify `ResolveAspect()` or target constants.
- Invalid/narrow/non-finite aspects remain outside `IsUltrawideAspect` and fail closed.
- Rollback is limited to reverting this bounded patch after review; no destructive Git operation is permitted.

## Stop conditions and phase gates

- Stop if generalized normalization requires changing the operation or replay state machine.
- Stop if unrelated files or behavior change.
- Stop after static/build/harness validation; runtime validation remains a separate user-run gate.

## Expected final Git review

- Confirm only the planned runtime and harness paths changed, plus the archived plan and task-log entry.
- Confirm forced cinematic targets and all other features remain untouched.
- Report static/build/test evidence separately from runtime validation.
