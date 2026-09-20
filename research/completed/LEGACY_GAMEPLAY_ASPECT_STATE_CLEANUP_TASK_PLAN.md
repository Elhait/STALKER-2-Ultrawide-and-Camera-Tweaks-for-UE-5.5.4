# Legacy Gameplay Aspect State Cleanup — Task Plan

## Objective

Remove the obsolete `g_lastObservedAspect` / `g_lastObservedAspectValid`
legacy pair and its shadow-comparison telemetry after the production restoration
authority has moved to `GameplayAspectRestorationStore`.

## Established evidence and current state

- The deletion audit found no production reads of the legacy pair.
- `GameplayAspectRestorationStore` is the production authority for restoration.
- Remaining pair reads/writes serve diagnostics/shadow comparison only.
- `g_lastGameplayCameraSource` and `g_lastGameplayCameraFov` remain production
  state for Dialogue invalidation and are out of scope.
- User-supplied runtime evidence reports live F11
  `HorPlus -> AspectRecalculation -> HorPlus` restoration PASS.

## Approved scope

- Remove the two legacy `RuntimeState` fields and aliases.
- Remove their writes and diagnostic comparison reads/log fields.
- Preserve restoration, GameplayBaseline, Dialogue, ZOOM/ADS, Cinematic and
  AspectRecalculation behavior.
- Create the cleanup report after validation.

## Explicit non-goals

- No cleanup of `g_lastGameplayCameraSource/Fov`.
- No architecture changes or production decision changes.
- No HorPlus, Cinematic, Dialogue, ZOOM, ADS, binocular or recovery changes.
- No game launch, release packaging, commit or unrelated cleanup.

## Expected files/areas

- `src/plugin/runtime.cpp`
- Relevant restoration/baseline harnesses only if stale comparison expectations
  exist.
- `research/reports/LEGACY_GAMEPLAY_ASPECT_STATE_CLEANUP.md`

## Batches and validation

1. Remove legacy pair fields, aliases, writes and comparison telemetry.
2. Repository-wide stale-reference search.
3. Run relevant harnesses, `test.cmd`, `build.cmd`, and `git diff --check`.
4. Perform read-only Git diff review against this plan.
5. Write the final cleanup report and stop before runtime.

## Risks and rollback/safe failure

- Risk: accidentally remove a Dialogue or restoration responsibility. Mitigate by
  preserving `g_lastGameplayCameraSource/Fov` and the complete restoration store
  decision tree.
- Risk: stale diagnostic references break compilation or schema assumptions.
  Mitigate with repository-wide search and full test/build validation.
- Rollback: source changes are limited to the approved pair and can be reverted
  as one bounded diff; no destructive filesystem operation is permitted.

## Stop conditions and phase gates

- Stop if a production consumer of the legacy pair is found.
- Stop if tests/build fail for an unrelated pre-existing reason and do not claim
  cleanup PASS without separating that evidence.
- Stop after static/build validation; runtime is explicitly not performed.

## Expected final Git review

Confirm only the approved source, test/build expectation, plan archival and report
paths changed. Preserve all pre-existing user changes and do not commit.
