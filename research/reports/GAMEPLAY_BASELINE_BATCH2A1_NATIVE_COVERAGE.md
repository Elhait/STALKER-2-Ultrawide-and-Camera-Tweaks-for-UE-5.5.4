# GameplayBaseline Native Coverage Extension — Batch 2A.1

## Result

Extended the shadow `GameplayBaseline` projection so native Gameplay target
validity is independent from optional HorPlus result validity. The projection
now covers the established HorPlus writer path and the established
`AspectRecalculation` writer path without moving any production consumer.

## Revised schema

```cpp
struct GameplayBaseline {
    FovSample nativeFov;       // required native Gameplay target
    FovSample horPlusFov;      // optional observed transform evidence
    float aspect;
    FovWriterSourceToken source;
    uint64_t observationSequence;
    bool valid;                // native baseline validity
};
```

`nativeFov` is required for `valid=true`. `horPlusFov` is valid only when the
same factual observation actually contained a transformed HorPlus result. A
pass-through or AspectRecalculation event leaves it explicitly unavailable;
the implementation never copies native FOV into the optional transformed
field and never recomputes it.

## Projection points

### HorPlus path

`ApplyHorPlusGameplay()` publishes the factual `CameraWriter` observation and
then projects it with `gameplayEligible = (coordinator == Gameplay)`.

### AspectRecalculation path

`ReplayManualTransitionOriginal()` now publishes a truthful native/pass-through
`CameraWriter` observation for valid Gameplay callbacks while the mode is
`AspectRecalculation` and coordinator is `Gameplay`. It projects that committed
observation without adding semantic ownership fields to the factual schema.

Fix-owned Auto restore observations excluded by the existing aspect-cache
contract are not projected as new Gameplay baseline events.

## Eligibility predicate

The projector requires:

- explicit semantic `gameplayEligible` from the established projection point;
- `CameraWriter` boundary;
- valid native input with `Native` space and `NativeRegisterInput` provenance;
- valid aspect and writer source;
- non-zero committed observation sequence;
- result either:
  - `Transformed + ModTransformResult`, or
  - `Native + PassThroughResult`.

Ownership is never inferred from source identity, numeric FOV values, flags or
ZOOM evidence.

## UPDATE / RETAIN / INVALIDATE

```yaml
eligible transformed HorPlus Gameplay:
  UPDATE nativeFov=valid horPlusFov=valid

valid HorPlus pass-through Gameplay:
  UPDATE nativeFov=valid horPlusFov=unavailable

valid AspectRecalculation Gameplay:
  UPDATE nativeFov=valid horPlusFov=unavailable

invalid/unavailable observation:
  RETAIN previous whole snapshot

CinematicActive writer:
  RETAIN

cached transformed ENTER:
  RETAIN; never a native target

HorPlus -> AspectRecalculation:
  INVALIDATE explicitly, then allow the next valid AspectRecalculation event
  to UPDATE

startup:
  INVALID
```

Every UPDATE replaces the whole snapshot, so a newer native pass-through sample
cannot retain an older transformed `horPlusFov` from a previous sample.

## Mapping

```yaml
nativeFov:          observation.inputFov
horPlusFov:         observation.resultFov only when transformed
aspect:             observation.aspect
source:             observation.writerSource
sequence:           observation.publicationSequence
```

The stored Gameplay aspect remains factual provenance for that event. It is not
used as the future cinematic presentation aspect; `ResolveCinematicAspect()`
remains the separate cinematic policy source.

## ADS-like transient samples

No stable-endpoint classifier, timer, exact-value rule, ZOOM ownership or new
ADS policy was introduced. An eligible transient transformed Gameplay sample
updates the native and optional transformed fields according to the existing
legacy retention semantics. A valid pass-through sample updates only the native
field, matching the legacy target update that occurs before HorPlus evaluation.

## Legacy state and production consumers

The following remain unchanged and present:

```text
g_lastGameplayCameraSource
g_lastGameplayCameraFov
g_lastObservedAspect
g_lastObservedAspectValid
```

`ReplayManualTransitionOriginal()` retains its source/FOV exchange and
Dialogue invalidation. `g_lastObservedAspect` remains the input for gameplay
mode aspect restoration. `TraceCinematicEnter()` still reads legacy retained
fields; it does not read `GameplayBaseline`. No FOV math, cinematic policy,
Dialogue, ZOOM, resolver or hook behavior was migrated.

## Deterministic equivalence matrix

The expanded `gameplay_baseline_harness` covers:

- startup invalid state;
- transformed native Gameplay update with optional HorPlus pair;
- consecutive transformed replacement and sequence preservation;
- ADS-like transformed transient update;
- transformed sample followed by valid native pass-through, proving the old
  transformed result is not retained with the new native target;
- pass-through-only startup;
- pass-through followed by transformed update;
- invalid factual sample retaining a valid baseline;
- explicit semantic ineligibility retaining the baseline;
- CinematicActive retaining the baseline;
- cached transformed ENTER retaining the baseline;
- malformed source retaining the baseline;
- explicit invalidation and update after invalidation;
- modeled legacy native/aspect/source subset equivalence.

Static review additionally confirms that the production ENTER consumer remains
on legacy state and that independent Dialogue invalidation and aspect
restoration responsibilities remain untouched.

## Validation

- Relevant GameplayBaseline harness: PASS.
- Full `test.cmd`: PASS, including all existing harnesses.
- `build.cmd`: PASS.
- `git diff --check`: PASS apart from normal Git line-ending warnings.
- Build emitted only the known external Zydis C4201 warnings.
- Game launch/runtime validation: NOT PERFORMED by design.

## Verdict

```yaml
native_gameplay_coverage:          PASS
transformed_optional_evidence:     PASS
pass_through_coverage:             PASS
aspect_recalculation_coverage:     PASS_STATIC_HARNESS
legacy_native_target_equivalence:  PASS_FOR_SUPPORTED_EVENTS
production_consumer_migration:     NOT_PERFORMED
runtime_validation:                NOT_PERFORMED
```

The equivalence claim is limited to supported valid Gameplay writer events.
Legacy Dialogue invalidation and gameplay-mode aspect restoration are separate
responsibilities and are not claimed to be replaced.

## Remaining gaps before Batch 2B

- No production consumer has been switched to the new snapshot.
- Batch 2B must read one coherent snapshot and use only `nativeFov` as the
  native Gameplay target; `horPlusFov` must remain diagnostic evidence.
- Batch 2B must preserve `ResolveCinematicAspect()` and the existing
  NativeHorPlus fallback.
- A later runtime regression may validate wiring across HorPlus,
  AspectRecalculation and forced cinematic aspects, but it is not required to
  establish this shadow projection's static contract.

Batch 2B is not started by this task.
