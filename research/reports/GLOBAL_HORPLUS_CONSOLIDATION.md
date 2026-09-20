# Global HorPlus — Consolidation

Date: 2026-09-20  
Scope: offline production-path audit and deterministic hardening. Runtime was
not performed.

## Architecture map

| Production path | Native input | Aspect/policy | Transform owner | Output/lifetime | Status |
| --- | --- | --- | --- | --- | --- |
| Gameplay writer, `ApplyHorPlusGameplay()` | `context.xmm0` native writer input | camera context aspect/flags; Gameplay coordinator | `gameplay::EvaluateHorPlus()` -> shared `cinematics::HorPlus()` | `context.xmm0`; per writer sample | PASS |
| Cinematic ENTER, `TraceCinematicEnter()` | ENTER `context.xmm0` authored/native FOV | `ResolveCinematicAspect()` | `cinematics::TryTransformCinematicFov()` -> shared `HorPlus()` | ENTER output plus cached transformed ENTER value | PASS |
| Cinematic writer cached ENTER branch | already-transformed value identified by current cache/tolerance guard | no second policy transform | pass-through in `ApplyHorPlusGameplay()` | per writer sample | PASS, numeric guard retained |
| GameplayBaseline projection | `CameraFovObservation.inputFov` | observation aspect; eligibility is Gameplay writer + native provenance | no new transform | retained native baseline plus optional transformed evidence | PASS |
| Dialogue FOV subsystem | Dialogue-specific native samples/targets | Dialogue lifecycle policy | `src/dialogue/dialogue_fov.cpp` projection helpers | lifecycle/transition values | Intentionally independent; untouched |
| MatchGameplay prediction | diagnostic input snapshot | diagnostic cinematic aspect | `src/diagnostics/matchgameplay_prediction.hpp` | diagnostic candidate only | Diagnostic-only, not production owner |

The production Global HorPlus invariant is satisfied for the covered paths:

```text
native FOV evidence
    -> selected effective aspect/policy
    -> one shared HorPlus projection
    -> transformed output or explicit pass-through
```

## Consolidation findings

### Gameplay

`EvaluateHorPlus()` is the single production Gameplay eligibility/transform
decision. It requires finite in-range FOV, a strictly wider-than-native aspect,
and flags `0x4` or `0x5`; unsupported/native-aspect inputs pass through. It
delegates the actual projection to the shared `cinematics::HorPlus()` helper.

The arbitrary-aspect replay now sweeps noncanonical aspects up to `4.0`, FOVs
from `30` to `150`, both supported flags, native-aspect identity and invalid
inputs. No ratio whitelist is used.

### Cinematics

`TryTransformCinematicFov()` is the common cinematic transform for both
`NativeHorPlus` and `GameplayHorPlus`. The mode selects only the target baseline:

- NativeHorPlus: target baseline equals authored ENTER/reference FOV;
- GameplayHorPlus: target baseline is `GameplayBaseline.nativeFov` when valid;
- invalid GameplayBaseline: authored ENTER fallback, which is the NativeHorPlus
  contract.

The optional `GameplayBaseline.horPlusFov` is not used as the cinematic native
baseline. The stored gameplay aspect is not substituted for the effective
cinematic aspect.

### Cached ENTER guard

The current production writer guard uses cached transformed ENTER value plus a
tolerance. The writer then publishes explicit observation metadata:
`Transformed/CachedCinematicEnter` input and pass-through result. That metadata is
truthful after classification, but it is not available as an authoritative
pre-classification ownership signal at the exact writer seam. Numeric equality can
still be ambiguous if a genuinely native cinematic sample coincides with the
cached value.

Therefore no provenance-based production repair is justified in this batch:

```yaml
cached_enter_guard: NUMERIC_GUARD_RETAINED
remaining_ambiguity: native value can numerically equal cached transformed value
runtime_needed: yes, for any future replacement contract
```

The existing cached-enter diagnostic predictor explicitly excludes matched cached
samples from native-space prediction.

## Negative and cross-state coverage

The replay harness now verifies:

- transformed sample cannot become a native GameplayBaseline;
- CinematicEnter observation cannot contaminate GameplayBaseline;
- pass-through B replaces stale transformed H(A) evidence;
- invalid/NaN/Inf Gameplay and cinematic inputs fail closed;
- unsupported Gameplay flags pass through;
- retained restoration target survives invalidate/republish/restore;
- NativeHorPlus and GameplayHorPlus use distinct reference inputs;
- coherent Dialogue Candidate promotes;
- changed source or contradictory target cancels without Active promotion.

No production state or diagnostic state was found to be safely obsolete solely
from this audit. Dialogue/ZOOM state and diagnostic MatchGameplay context remain
owned by their existing paths.

## Evidence corpus and validation

The existing Runtime Evidence Regression Corpus was extended through its replay
harness and fixture set. It now covers four recorded scenarios with expanded FOV,
aspect, flags and negative sweeps.

```yaml
gameplay_horplus: PASS
cinematic_native_horplus: PASS
cinematic_gameplay_horplus: PASS
arbitrary_aspect_contract: PASS
exactly_once_transform: PASS
cached_enter_guard: NUMERIC_GUARD_RETAINED
gameplay_baseline_isolation: PASS
negative_regressions: PASS
evidence_corpus_extended: YES
tests: PASS
build: PASS
runtime: NOT_PERFORMED
```

Validation used full `test.cmd`, production `build.cmd` and `git diff --check`.
Only the known external Zydis C4201 warnings were emitted by the build.

## Deferred items

- Replace the cached ENTER numeric guard only after runtime evidence establishes
  a reliable input-space/provenance contract at the writer seam.
- Remove any diagnostic-only MatchGameplay state only after a separate
  repository-wide responsibility audit.
- Do not infer resolver installation, UE callback ordering or visual framing from
  these deterministic tests.
