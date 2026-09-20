# Task 9 — CinematicActive HorPlus Guard Audit

## Scope

Bounded read-only audit of the current CinematicActive gameplay-writer path.
No source behavior, harness, build or runtime session was changed or run.

The question is whether the current guard can prevent a second HorPlus
transform when the gameplay writer receives cinematic FOV values other than
the cached ENTER result.

## Current dataflow

### Cinematic ENTER

`TraceCinematicEnter()` in `src/plugin/runtime.cpp:1755`:

1. Clears the previous `g_cinematicTransformedFov` cache.
2. Reads the ENTER `XMM0` value as `before`.
3. Resolves the cinematic aspect.
4. Applies `cinematics::TryTransformEnterFov()` when the cinematic policy is
   not `Native`.
5. Writes the transformed value back to `XMM0`.
6. Stores the transformed value in `g_cinematicTransformedFov`.
7. Sets the coordinator to `CinematicActive`.

The cache stores only a float value. It does not store input provenance,
source identity, aspect, episode generation or whether the value was already
transformed before the ENTER hook.

### Gameplay writer during CinematicActive

`ReplayManualTransition()` enters the HorPlus path before the generic
`CinematicActive` return. `ApplyHorPlusGameplay()` accepts both `Gameplay` and
`CinematicActive` coordinators.

For `CinematicActive`, the current guard is:

```cpp
finite(cachedCinematicFov) &&
fabs(writerInput - cachedCinematicFov) <= kDialogueTransformEpsilon
```

If true, the writer input is passed through with reason
`CINEMATIC_CACHED_VALUE`. Otherwise the current writer input is sent to
`gameplay::EvaluateHorPlus()` using the writer's current aspect and flags.

### EXIT

`TraceCinematicExit()` clears both `g_cinematicFovApplied` and
`g_cinematicTransformedFov`, so the ENTER cache does not intentionally survive
into the next cinematic episode.

## Input matrix

| Writer input during `CinematicActive` | Current result | Classification |
| --- | --- | --- |
| Native ENTER FOV, unchanged aspect | HorPlus transform is applied again unless it equals the cached transformed value | Safe only when the input contract is native and expected to be transformed here |
| Same cached transformed ENTER value | Numeric guard bypasses | SAFE BY CURRENT CONTRACT |
| Already-transformed ENTER value equal to cache | Numeric guard bypasses | SAFE BY CURRENT CONTRACT, though provenance is not proven |
| New native authored cinematic FOV | HorPlus transforms the new value once | SAFE BY CURRENT CONTRACT if the writer input is native |
| New already-transformed cinematic FOV not equal to cache | HorPlus transforms it again | CONCRETE STATIC GAP in provenance detection; occurrence is not established |
| Aspect changes while the old cached value is supplied | Old cached output bypasses recalculation | CONCRETE STATIC GAP if runtime aspect changes this way; occurrence is not established |
| Aspect changes and writer supplies a new native value | New value is transformed using current aspect | SAFE BY CURRENT CONTRACT |
| Cache invalid/Native cinematic policy with HorPlus gameplay mode | No cinematic cache guard; writer path can evaluate HorPlus | Separate policy-contract question, not proven as a Task 9 double-transform defect |

## Findings

### SAFE BY CURRENT CONTRACT

- ENTER cache reset and EXIT cache reset are episode-bounded.
- The same transformed ENTER value is not transformed twice by the gameplay
  writer because numeric equality triggers `CINEMATIC_CACHED_VALUE`.
- A changed native authored FOV is not incorrectly treated as the old cached
  value; it follows the normal HorPlus evaluation path.
- Existing production behavior does not depend on diagnostics.

### CONCRETE STATIC GAP

The current guard is a numeric equality guard, not a provenance guard. It can
prove only “this input is approximately the cached ENTER output.” It cannot
prove whether a different input is native or already HorPlus-transformed.

Therefore a hypothetical already-transformed dynamic cinematic sample can be
transformed a second time. Likewise, an old cached output can be accepted
after an aspect change even though it may no longer represent the current
aspect.

This is a limitation of the observable contract, not proof that the engine
actually sends either problematic sequence.

### RUNTIME EVIDENCE REQUIRED

The following engine behaviors are not established by current source or
existing runtime evidence:

- whether the gameplay writer receives a new already-transformed cinematic
  FOV during `CinematicActive`;
- whether the same logical cinematic FOV can reach the writer through a second
  transform path;
- whether the runtime aspect can change while `CinematicActive` and the writer
  continues supplying the old cached value;
- whether `CinematicAspectPolicy=Native` with `Gameplay.Mode=HorPlus` is an
  intended cross-mode interaction or should remain fully native.

### NOT ESTABLISHED

- A confirmed double HorPlus runtime defect.
- A safe universal production repair based only on numeric FOV values.
- A need to change the cinematic ENTER hook, gameplay writer ownership or
  EXIT choreography.

## Harness coverage

Existing harnesses cover the HorPlus mathematical transform, native/ultrawide
eligibility, cinematic policy resolution, initialization and mode transitions.
They do not model the runtime writer/cache provenance matrix above. No harness
was changed in this read-only audit.

## Minimal next telemetry if this risk becomes actionable

Use the existing `[Diagnostics] Enabled=true` gate and add one bounded,
change-only CinematicActive writer record containing:

```text
episode/generation
writerInputFov
cachedEnterFov
currentAspect
enterAspect
cacheValid
numericGuardMatched
horPlusEligible
horPlusApplied
outputFov
```

Do not infer provenance from the numeric values in the logger. The telemetry
should expose the decision that was made and the values that made it possible.

One runtime scenario is sufficient: a cinematic with a deliberate camera/FOV
change during `CinematicActive`, followed by an aspect change if the game can
perform one without leaving the cinematic. Compare the writer input/output
against the ENTER cache and the current aspect.

## Verdict

```yaml
current_same_cached_enter_value: SAFE_BY_CURRENT_CONTRACT
new_native_authored_value: SAFE_BY_CURRENT_CONTRACT
new_already_transformed_value: CONCRETE_STATIC_GAP
aspect_change_with_stale_cached_value: CONCRETE_STATIC_GAP
actual_runtime_occurrence: RUNTIME_EVIDENCE_REQUIRED
confirmed_double_transform_defect: NOT_ESTABLISHED
production_repair: NOT_PERFORMED
```

No production change is justified by this audit alone. The next action, if
needed, is diagnostic telemetry under the already reusable config gate, not a
new guard or cache architecture.
