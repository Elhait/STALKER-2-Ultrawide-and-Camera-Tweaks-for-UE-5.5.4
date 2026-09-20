# GameplayBaseline to Cinematic ENTER Cutover — Pre-Batch 2B Review

Date: 2026-09-19

## Scope and authority

This is a read-only architecture review of the proposed first production
consumer of `camera::GameplayBaseline`. Current source is authoritative;
existing reports are context only. No production implementation, build, test
or game launch was performed by this review.

## 1. Current ENTER production dataflow

The current path in `TraceCinematicEnter()` is:

```text
reset exit/recovery adjunct state
clear cached transformed ENTER FOV
resolve cinematic aspect/FOV enablement
deduplicate an enabled cinematic ENTER episode
read authored/native ENTER FOV from XMM0
resolve effective cinematic aspect
read legacy retained Gameplay FOV and aspect independently
validate the legacy pair
select the target native baseline
run the unified cinematic transform
write transformed XMM0 and cache it when successful
publish the observational CinematicEnter record
set coordinator=CinematicActive
```

The exact legacy reads are:

```cpp
const float gameplayNative =
    g_lastGameplayCameraFov.load(std::memory_order_acquire);
const float gameplayAspect =
    g_lastObservedAspect.load(std::memory_order_acquire);
```

`g_lastGameplayCameraSource` and `g_lastObservedAspectValid` are not read by
the current ENTER policy. Pair validity is recalculated from the numeric
values with `IsValidFovValue(gameplayNative)` and
`IsValidAspect(gameplayAspect)`.

The current target selection is:

```cpp
targetBaseline =
    fovMode == GameplayHorPlus && gameplayPairValid
        ? gameplayNative
        : authoredEnterFov;
```

The transform is then:

```cpp
TryTransformCinematicFov(
    authoredEnterFov,
    authoredEnterFov,
    targetBaseline,
    effectiveCinematicAspect,
    nativeAspect,
    output);
```

When `Cinematics.AspectRatio=Native`, `CinematicAspectOverrideEnabled()` is
false and no cinematic FOV transform is attempted. A forced `16:9` policy is
different: the override remains enabled and the effective transform aspect is
`16:9`.

## 2. Exact role of gameplay and cinematic aspect

The retained gameplay aspect currently has two ENTER-local roles:

1. it participates in legacy pair validity;
2. it produces the diagnostic retained Gameplay HorPlus value.

It is not passed to the production cinematic transform. The transform receives
the independently resolved effective cinematic aspect from
`ResolveCinematicAspect()`:

```yaml
Auto:       client/display viewport, native fallback
Native:     native aspect, but cinematic FOV override disabled
Forced16x9: native 16:9 aspect
Forced21x9: configured 3440/1440 aspect
Forced32x9: configured 32/9 aspect
```

Therefore the future consumer must not substitute
`GameplayBaseline.aspect` for the effective cinematic aspect.

## 3. Unified transform contract

The source still implements the documented common pipeline. In projection
space it computes:

```text
matchedProjection = tan(cinematicFov / 2)
                  * tan(targetBaselineFov / 2)
                  / tan(cinematicReferenceFov / 2)

matchedNative = inverseProjection(matchedProjection)
output        = HorPlus(matchedNative, effectiveCinematicAspect, nativeAspect)
```

At ENTER, `cinematicFov == cinematicReferenceFov`, so the native matched
endpoint reduces to `targetBaselineFov`. The final output is consequently:

```text
NativeHorPlus:
    HorPlus(authoredEnterFov, effectiveCinematicAspect)

GameplayHorPlus:
    HorPlus(gameplayNativeFov, effectiveCinematicAspect)
```

This establishes option A:

```text
GameplayBaseline.nativeFov is the correct target input.
```

`GameplayBaseline.horPlusFov` was calculated for the gameplay observation
aspect. Passing it as `targetBaselineFov` would treat an already transformed
horizontal FOV as a native angle and transform it again. It would be wrong even
when both aspects happen to match, and would additionally bind cinematic
output to the gameplay aspect when the policies differ.

For ENTER, `horPlusFov` is evidence useful for diagnostics and equivalence
checks only. It must not affect production cinematic math.

## 4. Coherence improvement and field use

A single `GameplayBaselineStore::Read()` returns a mutex-protected copy of:

```text
nativeFov
horPlusFov
aspect
source
observationSequence
valid
```

That is a real coherence improvement over the two independent legacy atomic
loads. The fields have different consumer relevance:

| Field | ENTER production role |
| --- | --- |
| `nativeFov` | Target native baseline for `GameplayHorPlus` |
| `valid` | Semantic availability gate |
| `aspect` | Baseline provenance/validation; never the cinematic transform aspect |
| `horPlusFov` | Diagnostics/equivalence only |
| `source` | Diagnostics/provenance only |
| `observationSequence` | Commit provenance only |

Neither source identity nor sequence proves presentation ownership or engine
causality.

## 5. Validity and fallback contract

Within the currently proven eligible-HorPlus subset, the minimal usable check
is:

```text
baseline.valid
&& IsValidFovValue(baseline.nativeFov)
&& IsValidAspect(baseline.aspect)
```

The numeric checks preserve the current ENTER guard and protect the production
consumer from treating a malformed but marked-valid snapshot as usable.
`source.valid`, non-zero `observationSequence` and finite `horPlusFov` are
already projection invariants and are not additional inputs to the transform.

The intended fallback remains:

```text
GameplayHorPlus + usable applicable baseline
    -> targetBaseline = baseline.nativeFov

otherwise
    -> targetBaseline = authoredEnterFov
    -> same unified NativeHorPlus identity path
```

No zero/default FOV, reverse reconstruction from `horPlusFov`, or fabricated
native FOV is justified.

However, `newBaseline.valid == false` is not currently equivalent to
`legacyPairValid == false`. That distinction blocks the proposed unconditional
replacement; see the next section.

## 6. Concrete equivalence gaps before production cutover

### 6.1 AspectRecalculation gameplay

The two runtime modes and `GameplayHorPlus` cinematic FOV mode are independently
configurable. No source guard prevents this combination:

```ini
[Gameplay]
Mode=AspectRecalculation

[Cinematics]
FovMode=GameplayHorPlus
```

In `AspectRecalculation`, `ReplayManualTransitionOriginal()` exchanges
`g_lastGameplayCameraFov` and updates the retained aspect. Current ENTER can
therefore receive a valid legacy gameplay target in this mode.

The Batch 2A baseline is populated only by eligible transformed observations
inside `ApplyHorPlusGameplay()` and is explicitly invalidated on
`HorPlus -> AspectRecalculation`. An unconditional cutover would therefore
change this reachable behavior:

```text
current legacy ENTER:
    valid AspectRecalculation Gameplay pair
    -> GameplayHorPlus target

proposed unconditional new ENTER:
    GameplayBaseline invalid
    -> NativeHorPlus fallback
```

This is a concrete current-source contradiction to the claim that the proposed
cutover leaves AspectRecalculation behavior unchanged.

### 6.2 Valid HorPlus pass-through samples

`ApplyHorPlusGameplay()` updates the legacy source/FOV/aspect fields before
`EvaluateHorPlus()`. Any valid Gameplay sample updates the legacy target even
when HorPlus is bypassed because the aspect is not ultrawide or flags are not
eligible.

Batch 2A deliberately retains the previous baseline for a native/pass-through
observation because its projection requires a transformed
`ModTransformResult`. Therefore a reachable sequence is:

```text
eligible transformed Gameplay sample A
    -> legacy=A, GameplayBaseline=A

valid pass-through Gameplay sample B
    -> legacy=B, GameplayBaseline retains A

cinematic ENTER
    -> current output targets B
    -> unconditional cutover would target A
```

The ADS-like case listed in the task is safe only when the transient sample is
eligible and transformed: both caches update to that sample. It does not close
the valid pass-through mismatch.

### Consequence

`PASS_FOR_GAMEPLAY_BASELINE_SUBSET` from Batch 2A is accurate. It is not a
proof that the new snapshot is a drop-in replacement for every current ENTER
input.

## 7. Recommended resolution

The better long-term model is to stop making availability of a native
cinematic target depend on availability of a transformed Gameplay pair:

```cpp
struct GameplayBaseline {
    FovValue nativeFov;       // required target evidence
    FovValue horPlusFov;      // optional transformed evidence
    float aspect;
    FovWriterSourceToken source;
    uint64_t observationSequence;
    bool valid;
};
```

The native target portion should be projected from every valid Gameplay writer
sample that is part of the supported legacy ENTER contract. The HorPlus result
should be valid only when it was actually observed as a transformed result.
This avoids inventing a HorPlus result for pass-through input and reflects what
the future ENTER consumer actually needs.

To achieve a complete behavior-preserving cutover, the projection also needs a
defined source for AspectRecalculation Gameplay observations. That can be a
separate bounded prerequisite publication/projection extension; it must not be
silently added inside `TraceCinematicEnter()`.

If that prerequisite is intentionally deferred, the only safe bounded Batch
2B is a partial cutover:

- use the coherent baseline only for the exact eligible transformed HorPlus
  subset proven equivalent;
- retain the legacy ENTER path for AspectRecalculation and for newer valid
  Gameplay observations that the shadow projection cannot represent;
- log which input path was selected during validation.

The current `GameplayBaseline` snapshot alone cannot distinguish “still the
latest applicable Gameplay sample” from “an older transformed baseline retained
across a newer pass-through sample.” An unconditional `baseline.valid` check is
therefore insufficient even for a mode-gated partial cutover. Either the
projection must expose coherent applicability/freshness metadata for the latest
Gameplay observation, or the baseline projection must be broadened first.

The alternative is an explicit product-contract change:

> `GameplayHorPlus` uses the last eligible transformed HorPlus Gameplay sample,
> not the latest valid Gameplay FOV known to the existing implementation.

That would make the current Batch 2A baseline suitable, but it is a behavior
change and must not be presented as legacy-equivalent cutover work.

## 8. Staleness and sequence semantics

No timer, age threshold or invented generation is justified.
`observationSequence` is a publication commit sequence and should remain
provenance-only. It is not an engine timestamp or causal generation.

Retention through invalid/unavailable samples, CinematicActive samples and
cached transformed ENTER is safe for the Batch 2A shadow contract. The
production-cutover problem is narrower: retained state can cease to equal the
latest legacy Gameplay target after a valid unprojected sample. That is an
applicability/equivalence problem, not a wall-clock freshness problem.

## 9. AspectRecalculation interaction

The existing `HorPlus -> AspectRecalculation` transition atomically invalidates
the whole new baseline and separately clears the legacy HorPlus-retained
source/FOV/aspect-validity state. `ReplayManualTransitionOriginal()` can then
repopulate the legacy values on subsequent AspectRecalculation writer calls;
it cannot repopulate the new baseline.

Batch 2B must not migrate or remove `g_lastObservedAspect`. It remains the
independent input to `ApplyPendingGameplayModeTransition()` for restoring the
observed gameplay aspect during `AspectRecalculation -> HorPlus`.

## 10. Legacy responsibilities that must remain

The following fields cannot be removed in Batch 2B:

```text
g_lastGameplayCameraSource
g_lastGameplayCameraFov
g_lastObservedAspect
g_lastObservedAspectValid
```

Independent responsibilities remain:

- `ReplayManualTransitionOriginal()` uses the source/FOV exchange for Dialogue
  invalidation;
- `ApplyPendingGameplayModeTransition()` uses observed aspect state for aspect
  restoration;
- AspectRecalculation still populates the legacy ENTER context but not the new
  baseline;
- a partial cutover needs the legacy ENTER path for non-equivalent cases.

## 11. Concurrency and coherence

`GameplayBaselineStore` uses one mutex for whole-snapshot projection,
invalidation and reads. `Read()` copies the snapshot while holding the mutex
and releases it before the caller performs math or logging. That is sufficient
for a production ENTER read.

Current publication/projection ordering is also safe:

```text
CameraFovObservationStore::Publish()
    -> releases observation-store mutex
GameplayBaselineStore::Project()
    -> acquires baseline-store mutex
```

There is no nested observation/baseline lock. ENTER needs only a short baseline
read lock. No logging, game callback or transform should occur while that lock
is held. No lock-free redesign is justified.

If a partial cutover adds cross-store “latest observation” comparison, two
independent reads would not by themselves create a coherent applicability
snapshot. Applicability metadata should be owned by the baseline projection or
the projection should be broadened; ad hoc cross-store reads in ENTER are not
recommended.

## 12. Minimal implementation scope after gap resolution

Once the equivalence decision is made, the actual consumer migration should
remain narrow:

1. add or use a pure target-selection helper that receives the authored ENTER
   reference, requested cinematic FOV mode and one copied baseline snapshot;
2. in `TraceCinematicEnter()`, take exactly one
   `g_gameplayBaselineStore.Read()` copy;
3. select only `baseline.nativeFov` as the GameplayHorPlus native target;
4. continue resolving cinematic aspect through `ResolveCinematicAspect()`;
5. continue calling the existing `TryTransformCinematicFov()` unchanged;
6. retain the current authored-FOV NativeHorPlus fallback;
7. retain diagnostic publication, transformed ENTER cache and coordinator
   ordering;
8. keep all legacy writes and their non-ENTER readers.

If complete legacy equivalence is required, a prerequisite baseline projection
extension is needed before these steps. If partial cutover is selected, the
legacy fallback boundary and applicability metadata must be part of the task
plan and harness contract.

No Dialogue, ZOOM, CameraState, resolver, cinematic aspect, transform math,
stable-endpoint classifier or hook change is justified.

## 13. Required deterministic harness matrix

The cutover harness must cover target selection and final transform, not merely
`GameplayBaselineStore::Read()`:

| Case | Required result |
| --- | --- |
| GameplayHorPlus + usable applicable baseline | Target is `baseline.nativeFov` |
| GameplayHorPlus + unavailable baseline | Authored ENTER NativeHorPlus fallback |
| NativeHorPlus mode | Baseline ignored |
| Cinematic aspect equals gameplay aspect | Result matches the established Gameplay HorPlus endpoint at reference ENTER |
| Cinematic aspect differs from gameplay aspect | Result uses cinematic aspect and does not equal stored gameplay-aspect result merely by reuse |
| Custom gameplay aspect | Does not replace effective cinematic aspect |
| Forced 16:9 | Output at reference is the selected native target; not the stored Gameplay HorPlus result |
| Forced 21:9 | One HorPlus conversion using 21:9 |
| Forced 32:9 | One HorPlus conversion using 32:9 |
| Aspect policy Native | Existing no-transform behavior retained |
| HorPlus -> AspectRecalculation invalidation | No stale HorPlus snapshot consumed |
| AspectRecalculation + valid legacy Gameplay context | Behavior follows the explicitly chosen resolution, not accidental fallback |
| Eligible ADS-like transient | Both legacy and new selection use the transient native target |
| Valid HorPlus pass-through after an eligible sample | No silent use of older target unless explicitly accepted as new product semantics |
| Cached transformed ENTER | Cannot become a native target |
| CinematicActive writer | Cannot replace Gameplay target |
| Invalid numeric baseline | Authored-FOV fallback; no zero/default target |
| Legacy Dialogue invalidation | Source/FOV exchange path remains untouched |
| Gameplay aspect restoration | `g_lastObservedAspect` path remains untouched |

The critical aspect-independence assertion is:

```text
GameplayBaseline.aspect != effectiveCinematicAspect

expected = HorPlus(
    GameplayBaseline.nativeFov,
    effectiveCinematicAspect,
    nativeAspect)

expected must not be obtained by directly reusing
GameplayBaseline.horPlusFov.
```

## 14. Runtime gate

Runtime evidence is not needed to decide the native-versus-transformed target:
the current math establishes that statically. Nor is runtime required to prove
whole-snapshot mutex coherence.

After the equivalence gap is resolved and implementation passes deterministic
harness/build validation, one bounded runtime regression is useful for wiring,
not for architecture discovery:

```text
Scenario:
  establish HorPlus Gameplay baseline at 32:9
  run GameplayHorPlus ENTER with forced 21:9
  run GameplayHorPlus ENTER with forced 32:9
  exercise the chosen AspectRecalculation/pass-through contract

Telemetry:
  baseline valid/native/horPlus/aspect/source/sequence
  selected target source
  authored ENTER FOV
  effective cinematic aspect
  final transformed FOV

Distinguishing outcome:
  the same native Gameplay target is transformed once using 21:9 and 32:9,
  producing policy-specific outputs independent of baseline.aspect;
  AspectRecalculation/pass-through follows the explicitly selected contract.
```

This runtime session is a later production regression gate, not a prerequisite
for resolving the current source-level contract mismatch.

## Verdict

```text
SPECIFIC_GAP_REQUIRES_RESOLUTION
```

The aspect/math question is resolved: use `GameplayBaseline.nativeFov` and the
effective cinematic aspect; keep `horPlusFov` diagnostic-only.

The unresolved pre-implementation decision is concrete:

> Must `GameplayHorPlus` preserve the current latest-valid-Gameplay target
> across AspectRecalculation and valid HorPlus pass-through samples, or is it
> intentionally redefined to use only the last eligible transformed HorPlus
> observation?

If current behavior must be preserved, broaden the native baseline projection
or retain an explicitly bounded legacy path before production cutover. If the
narrower definition is chosen, document and test it as a deliberate behavior
change rather than an equivalence migration.
