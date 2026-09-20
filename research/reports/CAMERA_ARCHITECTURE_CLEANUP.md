# Camera Architecture Cleanup

Date: 2026-09-20

## Audit scope

The audit followed FOV/camera state from its producers to consumers in
`src/plugin/runtime.cpp`, the camera/gameplay/cinematic stores and the
diagnostic/test harnesses. Classification is based on dataflow, not on the
presence of a `g_` prefix or a write site.

## State classification

| State or surface | Classification | Evidence / responsibility |
|---|---|---|
| `CameraFovObservationStore` | `PRODUCTION_REQUIRED` | Factual native/transformed observation publication used by the baseline projection and diagnostics. |
| `GameplayBaselineStore` | `PRODUCTION_REQUIRED` | Retained coherent native Gameplay baseline consumed by Cinematic ENTER `GameplayHorPlus`. Rejects transformed input and ineligible boundaries. |
| `GameplayAspectRestorationStore` | `PRODUCTION_REQUIRED` | Sole production authority for AspectRecalculation restoration after the legacy aspect cleanup. |
| `cinematicTransformedFov` / `cinematicFovApplied` | `PRODUCTION_REQUIRED` | Cached ENTER transformed-value guard; preserves the accepted exactly-once/bypass contract. |
| `lastGameplayCameraSource` / `lastGameplayCameraFov` | `PRODUCTION_REQUIRED` | Dialogue invalidation and camera-context continuity; not part of the obsolete aspect pair. |
| `lastAutoRestoreSource` | `PRODUCTION_REQUIRED` | Restoration write ownership/duplicate suppression. |
| `matchGameplay*` atomics | `DIAGNOSTIC_REQUIRED` | MatchGameplay prediction context and ENTER telemetry under `MATCHGAMEPLAY_DIAGNOSTIC`; no production decision. |
| `horPlusFovTelemetry*`, stable endpoint fields | `DIAGNOSTIC_REQUIRED` | Change-driven HorPlus/FOV-state telemetry and CameraState snapshot publication. |
| `lastLogged*` FOV fields | `DIAGNOSTIC_REQUIRED` | Log deduplication only; they do not feed FOV decisions. |
| `g_lastCombinedOutput`, resolution snapshots and combined diagnostic markers | `DIAGNOSTIC_REQUIRED` | Combined runtime diagnostic path only. |
| `g_lastCameraSource` and related combined flags | `DIAGNOSTIC_REQUIRED` | Combined observation logging; no production owner decision. |
| Runtime Evidence Replay fixtures/harnesses | `TEST_REQUIRED` | Deterministic contracts for transforms, baseline isolation, restoration, policy and lifecycle behavior. |
| `EARLY_CB ASPECT ENTER/RETURN` lines | `OBSOLETE` | Per-callback duplicate logging; the same hook already logs resolved aspect, policy, source and write refusal/success. Removed. |
| `UpdateGameplayAspectRestorationShadow` / `InvalidateGameplayAspectRestorationShadow` names | `OBSOLETE` naming | The underlying store is production authority, not a shadow. Renamed to `...RestorationState`; behavior unchanged. |
| Cached ENTER provenance replacement | `UNKNOWN` / deferred | Numeric guard passed the combined runtime session; no observed defect justifies replacing it. |

No production FOV cache, GameplayBaseline state, Dialogue source/FOV state or
restoration state was deleted. No diagnostic state was deleted because each
remaining surface has a current reader or a documented runtime-evidence role.

## Cleanup performed

- Removed only the redundant `EARLY_CB ASPECT ENTER/RETURN` log lines.
- Renamed the two restoration helper functions to match their production
  authority semantics.
- Preserved all stores, atomics, hooks, transform math and lifecycle behavior.

## Explicitly not cleaned up

- MatchGameplay diagnostic prediction/context.
- cached ENTER numeric guard and cinematic transformed cache.
- `lastGameplayCameraSource/Fov`.
- CameraState/FOV telemetry and combined diagnostics.
- Transitional state whose ownership is still required by production or runtime
  evidence.

## Cleanup verdict

```yaml
obsolete_behavior_removed: ONLY_DUPLICATE_ASPECT_CALLBACK_LOGGING
production_state_deleted: NO
diagnostic_state_deleted: NO
unknown_state_preserved: YES
architecture_rewrite: NO
```
