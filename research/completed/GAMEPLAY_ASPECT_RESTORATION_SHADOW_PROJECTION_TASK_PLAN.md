# Gameplay Aspect Restoration Shadow Projection — Task Plan

## Objective

Implement a coherent shadow projection for the established Gameplay aspect restoration semantics, without changing the production consumer or deleting legacy state.

## Established evidence/current state

- `g_lastObservedAspect` and `g_lastObservedAspectValid` are the only production inputs to `ApplyPendingGameplayModeTransition`.
- The legacy aspect may update from a valid aspect even when FOV or flags are not publishable as a `CameraFovObservation`.
- Fix-owned Auto restore and pending own restore retain the previous historical target.
- `GameplayBaseline.aspect` has narrower coherent Gameplay/FOV eligibility and is not a direct replacement.

## Approved scope

- Add `GameplayAspectRestorationState` and whole-snapshot store/projector.
- Project from the established writer facts at the same semantic ordering as the legacy aspect update.
- Preserve UPDATE, RETAIN and INVALIDATE semantics, including Auto restore exclusion and Gameplay-disabled path.
- Add deterministic harness coverage and diagnostics-gated comparison telemetry.
- Keep legacy state as the sole production authority.

## Explicit non-goals

- No production consumer cutover.
- No legacy deletion.
- No GameplayBaseline redesign.
- No HorPlus, Cinematic ENTER, AspectRecalculation, Dialogue, ZOOM, ADS or cached-ENTER behavior changes.
- No hooks, resolver, performance, release or runtime game launch.

## Expected files/areas

- New `src/camera/gameplay_aspect_restoration.hpp/.cpp`.
- `src/plugin/runtime.cpp` shadow publication, invalidation and diagnostics only.
- New restoration harness and `test.cmd` entry.
- Completed implementation report under `research/reports/`.

## Batches and validation

1. Add coherent state/store and modeled semantic operations; validate with focused harness.
2. Wire shadow projection at legacy writer/invalidation points without changing outputs; run focused harness and source diff review.
3. Add diagnostics-gated legacy-vs-shadow comparison; run full `test.cmd`, production `build.cmd`, `git diff --check` and static scope review.

## Risks and safe failure

- The shadow state must not become a production input. If projection evidence is unavailable, retain or invalidate shadow only; legacy behavior remains authoritative.
- Do not require FOV or flags validity for aspect-only shadow UPDATE when the legacy path does not require them.
- Do not run logging or callbacks while the state mutex is held.

## Stop conditions/phase gates

- Stop if source ordering cannot preserve the legacy Auto-restore exclusion.
- Stop before any consumer cutover or deletion.
- Stop after build/harness and report; runtime validation is deferred to one combined session.

## Final Git review

Compare changed paths against this plan, preserve all pre-existing user changes, inspect the diff and status, and report completed, remaining, deferred and not-runtime-validated items. This plan was archived after validation; legacy deletion remains a separate future batch.
