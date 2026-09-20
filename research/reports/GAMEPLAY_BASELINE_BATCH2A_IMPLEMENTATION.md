# GameplayBaseline Projection Architecture — Batch 2A

## Result

Implemented a coherent retained `GameplayBaseline` as a semantic projection of
eligible factual `CameraWriter` observations. It exists in parallel with the
legacy retained state. No production consumer reads it in this batch.

> `GameplayBaseline` is a retained semantic projection of eligible factual
> `CameraWriter` observations. It is not the `CameraWriter` observation itself,
> and `CameraFovObservation` does not determine semantic ownership.

## Final schema and storage

```cpp
struct GameplayBaseline {
    float nativeFov;
    float horPlusFov;
    float aspect;
    FovWriterSourceToken source;
    uint64_t observationSequence;
    bool valid;
};
```

`GameplayBaselineStore` retains the whole snapshot behind a short mutex. Reads,
updates and invalidation operate on the complete object, so native FOV,
HorPlus FOV, aspect, source and sequence cannot become a torn semantic pair.
The sequence is copied from the committed `CameraWriter` observation; it is not
an engine-causality claim.

## Exact projection point

Projection occurs in `ApplyHorPlusGameplay`, immediately after the factual
`CameraWriter` observation has been committed to `CameraFovObservationStore`.
The projection receives a separate semantic eligibility value:

```cpp
coordinator == CoordinatorState::Gameplay
```

The projector does not infer ownership from source address, FOV value, flags,
provenance or numeric anchors.

## Eligibility predicate

An update requires all of the following:

- `boundary == CameraWriter`;
- `gameplayEligible == true` from the established projection point;
- input is valid `Native` `NativeRegisterInput`;
- result is valid `Transformed` `ModTransformResult`;
- aspect is valid and finite;
- writer source is valid;
- publication sequence is non-zero.

Any failure returns `RETAIN` and cannot create partial baseline state.

## UPDATE / RETAIN / INVALIDATE

```yaml
eligible native -> transformed Gameplay HorPlus observation: UPDATE
invalid/unavailable observation: RETAIN
CinematicActive writer: RETAIN
cached transformed ENTER: RETAIN
native/pass-through/non-HorPlus result: RETAIN
HorPlus -> AspectRecalculation transition: INVALIDATE
startup: INVALID
```

The explicit mode-transition invalidation is wired beside the existing legacy
reset. It does not remove or reorder any legacy writes or reads.

## Mapping from factual observation

```yaml
GameplayBaseline.nativeFov:          observation.inputFov.value
GameplayBaseline.horPlusFov:         observation.resultFov.value
GameplayBaseline.aspect:             observation.aspect.value
GameplayBaseline.source:             observation.writerSource
GameplayBaseline.observationSequence: observation.publicationSequence
```

The already committed HorPlus result is used directly. No later recomputation
from a current aspect is performed.

## ADS-like and transient samples

No stable-endpoint classifier, timer, ZOOM ownership or exact-value rule was
introduced. An eligible transient native Gameplay sample follows the existing
HorPlus retention semantics and updates the shadow baseline in the same way as
the legacy subset. This deliberately does not claim that the value is an
authoritative user setting.

## Cinematic and cached ENTER handling

`CinematicActive` samples are passed to the projector with semantic eligibility
false and therefore retain the prior Gameplay baseline. A cached transformed
ENTER sample is rejected by both the explicit eligibility and the factual
space/provenance predicate:

```yaml
input.space=Transformed
input.provenance=CachedCinematicEnter
=> RETAIN
```

No fake native baseline is created.

## Legacy responsibilities intentionally retained

- `g_lastGameplayCameraSource`, `g_lastGameplayCameraFov`,
  `g_lastObservedAspect` and `g_lastObservedAspectValid` remain present and
  unchanged.
- `ReplayManualTransitionOriginal` keeps its source/FOV exchange and Dialogue
  invalidation behavior.
- `g_lastObservedAspect` remains the input for gameplay mode aspect restoration.
- Cinematic ENTER continues to read the legacy retained fields.
- No Dialogue, ZOOM, CameraState, recovery, resolver or FOV math migration was
  performed.

## Equivalence harness

`tests/camera/gameplay_baseline_harness.cpp` covers:

- startup invalid state;
- valid arbitrary native/result/aspect/source projection;
- coherent consecutive updates and sequence preservation;
- ADS-like transient native FOV changes without numeric classification;
- invalid/pass-through observation retaining the prior baseline;
- CinematicActive semantic ineligibility retaining the baseline;
- cached transformed ENTER retaining the baseline;
- explicit semantic eligibility separate from factual source data;
- malformed source retaining the baseline;
- explicit invalidation and update after invalidation;
- comparison with a modeled legacy retained subset.

Static source review confirms the production Cinematic ENTER consumer still
reads the legacy fields and that the old Dialogue invalidation and aspect
restoration paths remain in place.

## Validation

- Relevant gameplay baseline harness: PASS.
- Full `test.cmd`: PASS, including all existing harnesses.
- `build.cmd`: PASS.
- `git diff --check`: PASS apart from normal Git line-ending warnings.
- Build emitted only the known external Zydis C4201 warnings.
- Game launch/runtime validation: NOT PERFORMED by design.

## Verdict

```yaml
gameplay_baseline_projection:     PASS
legacy_semantic_equivalence:      PASS_FOR_GAMEPLAY_BASELINE_SUBSET
production_consumer_migration:    NOT_PERFORMED
legacy_globals:                   RETAINED
runtime_required_for_batch_2a:    NO_NEW_QUESTION_ESTABLISHED
```

The equivalence claim is intentionally limited to the Gameplay baseline subset.
It does not claim equivalence for Dialogue invalidation or gameplay-mode aspect
restoration, which remain legacy responsibilities.

## Batch 2B gate

The likely future candidate is:

```text
Cinematic ENTER -> GameplayHorPlus baseline consumption
```

It was not migrated here. Batch 2B requires a separate bounded plan and review
before changing that production consumer.
