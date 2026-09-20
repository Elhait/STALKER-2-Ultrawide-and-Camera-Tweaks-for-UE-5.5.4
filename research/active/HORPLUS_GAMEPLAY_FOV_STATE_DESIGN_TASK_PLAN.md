# HorPlus Gameplay FOV State Design Task Plan

## Objective

Define a production-safe state/cache and change-only telemetry model for `Gameplay.Mode=HorPlus` before changing runtime behavior. The design must separate gameplay FOV from cinematic FOV and make cache validity depend on the inputs that produced the cached result.

## Established evidence and current state

- `TryTransformHorPlus()` is runtime-validated for tested ultrawide aspects, native ADS inputs, and 16:9 bypass.
- ADS ownership aggregation and completion are runtime validated for the tested Steam 2.0.5 matrix.
- Cinematic ENTER/EXIT and Dialogue exclusion paths already exist.
- Current `ApplyHorPlusGameplay()` reads the current writer input and applies HorPlus on every eligible writer callback; no gameplay HorPlus cache or owner/validity model currently exists.
- `ConfiguredGameplayFov` has not yet been located as a validated standalone runtime source in the current source.
- Cinematic authored/native FOV is a separate domain; observed `90` is not an invariant.

## Approved scope

- Static design only for:
  - `CameraOwner` classification inputs;
  - `GameplayHorPlusState` validity and invalidation;
  - three gameplay FOV values;
  - separate cinematic FOV values;
  - change-only telemetry for handled and unclassified gameplay FOV changes;
  - the first bounded runtime acceptance matrix.
- Identify the smallest source seam needed to obtain configured gameplay FOV, without implementing it in this batch.

## Explicit non-goals

- Do not change `ApplyHorPlusGameplay()`.
- Do not change `AspectRecalculation`.
- Do not change cinematic or Dialogue behavior.
- Do not add sprint, damage, binocular, or other unsupported owner handling.
- Do not claim that unknown gameplay FOV changes are safe until runtime evidence exists.
- Do not build an ASI or launch the game in this design batch.

## Proposed contract

### Domains

Gameplay domain:

```text
ConfiguredGameplayFov
NativeGameplayCameraFov
HorPlusGameplayCameraFov
```

Cinematic domain:

```text
NativeCinematicFov
CinematicHorPlusFov
```

`NativeCinematicFov` must never populate `NativeGameplayCameraFov` or manufacture a valid gameplay cache.

### Gameplay cache

```text
GameplayHorPlusState {
    valid
    configuredFov           // only when a validated source exists
    nativeGameplayFov
    runtimeAspect
    gameplayMode
    horPlusGameplayFov
}
```

`valid=true` only when the cached result corresponds to the current native gameplay FOV, runtime aspect and mode, plus configured FOV when that value is part of the validated input contract.

### Ownership

```text
Gameplay / ADS     → eligible for gameplay HorPlus policy
Cinematic          → cinematic policy only
Dialogue           → Dialogue policy only
Recovery           → no gameplay HorPlus transform
Unknown            → native pass-through and change-only diagnostic
```

The cinematic bypass rule is not unconditional: cinematic samples must not recalculate gameplay state. A previously valid gameplay cache may be preserved; if it is invalid, it remains invalid until a trustworthy gameplay-camera value is observed after gameplay ownership returns.

### Change-only telemetry

For every actual native gameplay FOV change, record:

```text
GAMEPLAY_FOV_CHANGE
configured=...
nativeOld=...
nativeNew=...
modifier=...
horPlusOld=...
horPlusNew=...
aspect=...
owner=...
cacheValid=...
reason=...
handled=...
```

`modifier = nativeNew - configured` is diagnostic evidence only; it is not a production classification rule.

## Batches and validation

### Batch 1 — static design

- Trace current HorPlus, coordinator, ADS, Dialogue and cinematic state ownership.
- Locate or explicitly mark the missing configured-FOV source.
- Record invariants and invalidation triggers: aspect, mode, configured FOV, load/ownership return.
- Produce the design report.

Validation: source cross-reference and design review; no build or runtime.

### Batch 2 — telemetry-only implementation design

- After approval of Batch 1, add change-only telemetry behind a diagnostic macro.
- Do not alter HorPlus output or owner behavior.

Validation: harness/build only; no stable ASI replacement.

### Batch 3 — one runtime acceptance pass

- Use a non-90 configured FOV if available.
- Exercise gameplay idle, ADS, sprint/run, weapon changes, damage/DoT, death/respawn, Dialogue, Cinematic, and binocular 2.0.
- Classify only observed unknown FOV changes; do not infer unsupported behavior in advance.

Validation: supplied runtime log and visual result; separate from build evidence.

## Risks and safe failure

- Treating cinematic FOV as gameplay FOV can corrupt the gameplay cache; domain separation is mandatory.
- A missing configured-FOV source must leave that field unknown rather than inventing a read.
- Unknown ownership must pass through natively and be logged, not transformed by assumption.
- Existing `AspectRecalculation` remains the fallback if HorPlus behavior is unsuitable.

## Stop conditions

- Stop before implementation if configured FOV cannot be sourced without a new unvalidated hook.
- Stop if any proposed design requires continuous heavy FOV processing during stable idle gameplay.
- Stop and reopen ownership design if runtime shows a foreign owner being transformed by HorPlus.

## Expected final review

- Confirm this batch changed only research/design documentation.
- Distinguish established source facts, hypotheses and missing seams.
- Keep implementation and runtime validation explicitly pending.
