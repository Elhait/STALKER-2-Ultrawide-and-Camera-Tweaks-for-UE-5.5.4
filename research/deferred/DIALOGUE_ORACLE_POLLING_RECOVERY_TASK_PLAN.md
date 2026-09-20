# Dialogue Oracle Polling Recovery Task Plan

## Objective

Make the UE4SS `DialogueGroundTruthCorrelation` research poller survive temporary `PlayerController`/map-transition failures so it can resume polling and capture `IsInStaticDialog()` edges.

## Established evidence and current state

- UE4SS discovers `ProcessEvent` and starts the oracle successfully.
- `UEHelpers.GetPlayer()` throws during map/controller lifecycle transitions.
- The exception currently terminates the `LoopAsync` callback, so no oracle edges are recorded for that run.
- The ASI diagnostic trace is already built and must not be changed for this task.

## Approved scope

- Change only the UE4SS research Lua script `DialogueGroundTruthCorrelation/Scripts/main.lua`.
- Catch transient player/controller lookup failures inside the polling callback.
- Skip the current poll and allow the next scheduled poll to retry.
- Preserve 50 ms cadence, read-only behavior, edge-only logging, and `IsInStaticDialog()` query semantics.

## Explicit non-goals

- No ASI or production C++ changes.
- No Dialogue classifier repair.
- No changes to HorPlus, Cinematics, AspectRecalculation, hooks, or config.
- No new UE4SS reflection path or alternate ownership signal.
- No game launch by the agent.

## Files or areas expected to be touched

- External research script: `E:/Steam/steamapps/common/S.T.A.L.K.E.R. 2 Heart of Chornobyl/Stalker2/Binaries/Win64/ue4ss/Mods/DialogueGroundTruthCorrelation/Scripts/main.lua`
- This plan file only in the repository.

## Implementation batches

### Batch 1 — polling recovery guard

- Wrap player lookup/validation in a bounded protected call.
- On lookup failure or unavailable player, return from the current poll without terminating the loop.
- Keep existing protected call around `IsInStaticDialog()` and existing edge output.

### Batch 2 — static/script validation

- Re-read the edited Lua script.
- Verify only the approved script changed.
- Check syntax using an available Lua parser/interpreter if present; otherwise perform a bounded structural review.

## Validation

- Static script inspection.
- `git diff --check` and read-only Git path review for repository changes.
- No ASI build and no game launch in this task.
- Runtime validation remains user-owned: verify recovery across `InitMap → MainMenuMap → WorldMap`, then capture dialogue edges.

## Risks and rollback / safe-failure behavior

- The guard is diagnostic-only and does not write game memory or alter production behavior.
- If the script API behaves differently than expected, the poll should skip rather than call an unavailable object.
- Rollback is limited to restoring the exact previous Lua script if the static validation fails.

## Stop conditions and phase gates

- Stop if editing requires changing UE4SS shared helpers or production code.
- Stop after static validation; do not infer runtime oracle success until the user supplies a new UE4SS log with edges.

## Expected final Git review

- Confirm the plan and any repository documentation are the only repository changes.
- Confirm the external Lua script is the only behavior-bearing file touched.
- Report completed, remaining, deferred, blocked, and not-runtime-validated items separately.
