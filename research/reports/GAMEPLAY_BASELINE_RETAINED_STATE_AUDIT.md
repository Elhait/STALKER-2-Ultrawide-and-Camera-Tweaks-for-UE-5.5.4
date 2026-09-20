# Gameplay Baseline Retained State — Static Audit

## Scope

Read-only audit for a future Batch 2 migration. No source, harness, build or
runtime behavior was changed by this audit.

## Current retained state

The current Gameplay baseline is distributed across these runtime fields:

```cpp
g_lastGameplayCameraSource  // atomic uintptr_t
g_lastGameplayCameraFov     // atomic float
g_lastObservedAspect        // atomic float
g_lastObservedAspectValid   // atomic bool
```

The FOV/source pair and the aspect/validity pair are separate atomic values.
There is no shared publication sequence or coherent multi-field commit for the
baseline consumed at Cinematic ENTER.

## Writers

### 1. `ReplayManualTransitionOriginal`

This is the legacy/original writer path used outside the HorPlus early-return
path.

- Exchanges `g_lastGameplayCameraSource` and `g_lastGameplayCameraFov` for
  every callback before validating the source fields.
- Uses the previous source/FOV pair for Dialogue runtime invalidation:
  source identity changes or a material same-source FOV jump can reset a
  non-inactive Dialogue phase.
- After safe-reading aspect and flags, updates `g_lastObservedAspect` when the
  observation is valid and is not a fix-owned Auto restore.
- The Auto restore exclusion applies to the aspect cache only; the source/FOV
  exchange has already occurred before that decision.

### 2. `ApplyHorPlusGameplay`

This is the current `Gameplay.Mode=HorPlus` writer path.

- For `coordinator == Gameplay` and valid input FOV/aspect, stores source, FOV,
  aspect and aspect-validity in the legacy retained fields.
- Does not perform the legacy source/FOV exchange or Dialogue invalidation.
- Does not update the retained baseline while `coordinator == CinematicActive`.
- Invalid source/FOV/aspect leaves the previous retained baseline untouched.
- The same callback also publishes the new Batch 1 `CameraWriter`
  observation; the observation is factual, but no production consumer reads it
  yet.

### 3. Other paths

- When gameplay is disabled, the writer path can update observed aspect only;
  it does not establish a gameplay FOV baseline.
- `HorPlus -> AspectRecalculation` explicitly invalidates the retained source,
  FOV and aspect-validity fields.
- Startup initializes the FOV to NaN, source to zero and aspect-validity false.

## Readers

### Cinematic ENTER

`TraceCinematicEnter` reads `g_lastGameplayCameraFov` and
`g_lastObservedAspect` independently, then derives:

```text
gameplayPairValid = valid FOV && valid aspect
pre-enter native baseline = lastGameplayCameraFov
pre-enter HorPlus baseline = HorPlus(lastGameplayCameraFov, lastObservedAspect)
```

`GameplayHorPlus` uses the retained native FOV when the pair is valid;
`NativeHorPlus` fallback uses the cinematic ENTER value when it is not.

The source token is not used by ENTER to validate the baseline. It is retained
for Dialogue invalidation and diagnostics only.

### Other readers

- `ApplyPendingGameplayModeTransition` reads only `g_lastObservedAspect` and
  its validity to restore a previously observed ultrawide aspect.
- No current production consumer reads the new observation store.

## Invalidation and preservation semantics

```yaml
startup:
  source=0
  fov=NaN
  aspectValid=false

valid HorPlus Gameplay writer:
  update source + nativeFov + aspect + aspectValid

invalid HorPlus sample:
  retain previous baseline

CinematicActive writer:
  retain Gameplay baseline

HorPlus -> AspectRecalculation:
  clear source + fov
  invalidate aspect

legacy/original writer:
  source/FOV exchange occurs before source-field validation
  aspect update is guarded by SafeRead and Auto-restore exclusion
```

## Migration risks

1. **Pair coherence:** a direct replacement that reads separate latest slots
   could combine a new FOV with an older aspect. Batch 2 must consume one
   committed `CameraWriter` observation, not independently read store fields.
2. **Eligibility mismatch:** not every `CameraWriter` observation is a
   Gameplay baseline. Cinematic-active writer samples and cached transformed
   ENTER samples must not project into GameplayBaseline.
3. **Legacy path behavior:** replacing the exchange in
   `ReplayManualTransitionOriginal` could accidentally remove Dialogue
   invalidation semantics. Baseline projection and Dialogue context tracking
   must remain separate until equivalence is proven.
4. **Mode transition reset:** the existing explicit
   `HorPlus -> AspectRecalculation` invalidation must remain represented in the
   new retained semantic snapshot.
5. **Aspect ownership:** `g_lastObservedAspect` also serves gameplay mode
   transition restoration, not only MatchGameplay. It cannot be deleted merely
   because GameplayBaseline moves to the observation store.

## Proposed Batch 2 projection contract

This is a design target, not an implementation decision:

```yaml
input:
  committed CameraWriter observation

eligible only when:
  boundary == CameraWriter
  input.valid == true
  input.space == Native
  input.provenance == NativeRegisterInput
  result/aspect components are coherent and valid
  gameplay coordinator/owner is confirmed
  observation is not CinematicActive or cached transformed input

projection:
  GameplayBaseline.nativeFov = observation.inputFov
  GameplayBaseline.horPlusFov = observation.resultFov when transformed
  GameplayBaseline.aspect = observation.aspect
  GameplayBaseline.source = observation.writerSource
  GameplayBaseline.sequence = observation.publicationSequence
  GameplayBaseline.valid = true

invalidation:
  preserve current explicit HorPlus -> AspectRecalculation reset semantics
  preserve fail-closed invalid/unavailable behavior
```

The `Gameplay coordinator/owner` condition is not encoded in the Batch 1
observation itself. Batch 2 therefore needs either a separately validated
projection point at `ApplyHorPlusGameplay` or an explicit owner field added by
its own bounded contract; it must not infer owner from source identity alone.

## Equivalence proof required before migration

Before removing or redirecting the old cache, deterministic tests must prove:

- valid Gameplay HorPlus writer sample produces the same native/aspect/source
  values consumed by Cinematic ENTER;
- CinematicActive and cached transformed samples cannot replace the baseline;
- invalid samples preserve the old valid baseline and fail closed when none
  exists;
- `HorPlus -> AspectRecalculation` clears the new semantic baseline exactly
  where the old consumer became invalid;
- the original writer path still performs Dialogue source/FOV invalidation;
- gameplay mode transition aspect restoration still sees the required aspect
  state;
- NativeHorPlus fallback remains unchanged when GameplayBaseline is invalid.

## Verdict

```yaml
old_baseline_dataflow:                STATICALLY MAPPED
single_authoritative_old_cache:       NO
CameraWriter_observation_input:       AVAILABLE_FROM_BATCH1
direct_store_replacement_safe:        NOT_YET_ESTABLISHED
production_migration:                 NOT_PERFORMED
runtime_validation:                   NOT_REQUIRED_FOR_THIS_AUDIT
next_step:                            bounded Batch 2 projection design
```

The safe next step is to define and harness a `GameplayBaseline` projection in
parallel with the existing fields. Deleting or redirecting the old fields is a
separate phase gate after equivalence is demonstrated.
