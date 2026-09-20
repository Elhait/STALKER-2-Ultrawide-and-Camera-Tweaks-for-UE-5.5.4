# ADS Owner Completion Repair Task Plan

## Objective

Prevent a native ADS OUT blend that ends at the observed endpoint `0.0143868 / 0.985613` from leaving the ADS owner in `Exiting` and suppressing a subsequent real Dialogue lifecycle.

## Established evidence and current state

- Steam executable identity in the supplied runtime log matches the validated 2.0.5 image.
- The candidate diagnostic integration recorded 310 Wideboy samples in 26 alternating directional runs.
- The final pre-dialogue OUT sample was `rax4c=0.0143868`, `rax50=0.985613`.
- The integration passes `epsilon=0.01`, so that sample does not satisfy the current completion predicate.
- The subsequent Dialogue boundary produced `RETURN 6` and the log contains no positive Dialogue lifecycle record.
- This is sufficient evidence of a stale/over-conservative ADS completion gate, but not evidence to change Dialogue ownership heuristics.

## Approved scope

- Adjust the ADS lifecycle completion tolerance to cover the validated native endpoint behavior.
- Add harness regression coverage for the observed `0.0143868 / 0.985613` OUT endpoint and nearby non-endpoint values.
- Keep the integration diagnostic-only behind its existing build macros.
- Rebuild the diagnostic candidate and run the existing harnesses.

## Explicit non-goals

- Do not change HorPlus or AspectRecalculation.
- Do not change cinematic lifecycle or post-cinematic exclusion.
- Do not add a positive Dialogue classifier.
- Do not add timers, frame-based release, or FOV heuristics.
- Do not modify the stable ASI or launch the game.

## Expected files or areas

- `src/dialogue/ads_lifecycle.cpp`
- `src/plugin/runtime.cpp` only if the shared completion tolerance needs a named constant at the call site
- `tests/dialogue/ads_lifecycle_harness.cpp`
- this plan and the bounded implementation record

## Batches and validation

### Batch 1 — completion tolerance and harness

- Introduce one named completion tolerance for the validated native blend endpoint.
- Use it for the existing stateless endpoint predicates without changing state transitions.
- Add tests proving the observed endpoint completes OUT, while a clearly incomplete sample remains `Exiting`.
- Run `test.cmd` and inspect the source diff.

### Batch 2 — diagnostic candidate build

- Rebuild only `STALKER2CameraTweaks_WideboyAdsDiagnostic.asi`.
- Confirm the stable `STALKER2CameraTweaks.asi` remains untouched.
- Run `git diff --check` and a read-only Git review.

## Risks and rollback / safe failure

- A tolerance that is too broad could release ADS ownership before the native blend is visually complete. The harness must include a lower-weight non-completion case.
- If the harness or build fails, keep the existing candidate artifact and do not replace the stable artifact.
- Revert only the bounded source/test changes if review shows scope drift.

## Stop conditions and phase gates

- Stop if the observed endpoint cannot be represented without a broad heuristic.
- Stop before runtime validation; user must decide whether to run the rebuilt candidate.
- Stop if any HorPlus, AspectRecalculation, cinematic, or stable-release file changes outside the approved scope.

## Expected final Git review

- Confirm only the approved source/test/plan paths changed.
- Record build and harness results separately from runtime evidence.
- Leave runtime validation as pending until a user-run log confirms real Dialogue is no longer blocked after ADS.
