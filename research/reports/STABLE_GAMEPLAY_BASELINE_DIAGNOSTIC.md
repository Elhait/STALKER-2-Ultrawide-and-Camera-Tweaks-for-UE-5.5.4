# Stable Gameplay Baseline Runtime Discovery — Diagnostic Design

## Scope

This batch prepares one runtime experiment for the future MatchGameplay
research. It does not implement MatchGameplay and does not create a production
baseline classifier or cache.

All new records are behind `[Diagnostics] Enabled=true` and are read-only.

## Existing signals reused

The canonical diagnostic build already exposes:

- native writer FOV changes and previous/current values;
- gameplay writer source token;
- derived presentation/owner labels;
- Dialogue phase through CameraStateSnapshot;
- post-cinematic recovery exclusion;
- last ZOOM direction and sequence, explicitly as observation evidence;
- change-only event ordering and snapshot sequence;
- HorPlus input/output values.

The implementation does not interpret `lastZoomDirection` as a current zoom
state. It also does not treat missing ZOOM callbacks as proof of idle because
binocular/internal magnification may use another path.

## Added diagnostic telemetry

### Stable endpoint observation

The canonical telemetry now emits at most one record when the same native FOV
is observed for three consecutive writer samples in a strictly limited
context:

```text
STABLE_GAMEPLAY_ENDPOINT_OBSERVATION
```

The context requires:

- derived owner = `Gameplay`;
- coordinator = `Gameplay`;
- Dialogue phase = `Inactive`;
- post-cinematic exclusion inactive;
- readable gameplay source;
- finite native writer FOV.

The record includes the FOV, source token, sample count, Dialogue phase,
coordinator and last ZOOM sequence. It explicitly labels the result:

```text
hypothesis=stable-endpoint-only
lastZoomDirection=observation-only
```

This is not `ConfiguredGameplayFov`, does not establish ownership, and does
not affect HorPlus or any other production decision. It is intentionally a
small observation aid for manual comparison of idle, ADS and post-ADS values.

The observation resets on a context change, source change or material native
FOV change. No timer, hardcoded FOV or no-ZOOM assumption is used.

### Task 9 CinematicActive telemetry

The existing change-only cinematic writer trace is now compiled into the
canonical diagnostics path and runtime-gated. It adds:

```text
cachedEnterFov
enterAspect
cacheValid
numericGuardMatched
```

The existing record already reports writer input/output, current aspect,
eligibility, applied status and output FOV. This lets one cinematic run show
whether a writer sample matched the cached ENTER result or followed the normal
HorPlus path. No guard behavior was changed.

### Native-to-HorPlus observation pair

The earlier change-driven FOV record already contained the mathematical pair as
`nativeNew` and `horPlusNew`. The explicit fields below clarify provenance and
make it unambiguous that both values belong to the same writer observation:

```text
nativeBeforeHorPlus = HorPlusGameplayApplyResult.inputFov
horPlusResult       = HorPlusGameplayApplyResult.outputFov
```

Both values come from the same `HorPlusGameplayApplyResult` and the result is
recorded after the existing calculation. This is a telemetry truthfulness
improvement, not new mathematical evidence. The diagnostic does not infer a
configured or menu FOV, and it does not substitute the native value when a
HorPlus result is unavailable. Existing `nativeNew`/`horPlusNew` fields remain
for continuity with earlier logs.

The future baseline record must retain the complete observation tuple rather
than only one FOV value:

```cpp
StableGameplayBaseline {
    float nativeFov;
    float horPlusFov;
    float aspect;
    // provenance and validity fields
};
```

The HorPlus value must not later be reconstructed from a different aspect or
runtime state.

## Runtime experiment

In one game session, set Gameplay FOV to each value and collect the diagnostic
log:

```text
80  -> idle -> ADS -> release -> idle
100 -> idle -> ADS -> release -> idle
110 -> idle -> ADS -> release -> idle
```

If practical, change the setting in the same session. For every setting,
compare:

```text
stable endpoint before ADS
ADS native trajectory
stable endpoint after ADS
```

The hypothesis is supported only if the stable endpoints track the selected
settings and return to the same value after transient zoom. A binocular pass
should be treated separately because its internal magnification may not emit
the observed ZOOM callbacks.

The same session may include a cinematic with a dynamic FOV/aspect change to
exercise the new Task 9 fields; this is evidence collection only.

## Validation

- Full `test.cmd`: PASS.
- Canonical `build.cmd`: PASS.
- `git diff --check`: PASS, with existing normal line-ending warnings.
- No game launch.
- No Ghidra/EXE analysis.
- Native-to-HorPlus pair fields clarify existing evidence and do not alter
  production behavior.

## Status

```yaml
production_baseline_classifier: NOT ADDED
ConfiguredGameplayFov: UNKNOWN
stable_endpoint_observation: DIAGNOSTIC ONLY
Task9 telemetry: DIAGNOSTIC ONLY
runtime experiment: READY
runtime validation: PENDING
```
