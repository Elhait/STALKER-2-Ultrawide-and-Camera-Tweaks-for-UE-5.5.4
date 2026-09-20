# MatchGameplay Cinematic Reference Provenance Audit

Status: offline research only. No production code, telemetry or runtime was
changed.

## 1. Question

The MatchGameplay candidate needs a reference FOV `R` for each cinematic
episode. The formula is mathematically valid when `R` is known, but the project
must not promote the observed `90` value to a universal engine contract without
evidence.

## 2. Existing evidence inventory

| Evidence | Aspect | ENTER/reference evidence | HorPlus ENTER result | EXIT/recovery evidence | Provenance status |
|---|---:|---:|---:|---:|---|
| Current combined runtime, Steam 2.0.6 | 3.55556 | authored ENTER `90` | `126.87` | native target `112.623` | direct runtime observation |
| Earlier Auto runtime, Steam 2.0.4 | 2.38889 | not directly logged in the retained report | `106.688` | recovery passed | `R≈90` only by inverse mathematical inference |
| Earlier Auto runtime, Steam 2.0.4 | 3.55556 | not directly logged in the retained report | `126.87` | recovery passed | `R≈90` only by inverse mathematical inference |
| F7 timing probe, Steam 2.0.4 | 3.55556 | ENTER sample `90.65574` | probe-specific, not the final production transform | native convergence `90.65574→90.0` | direct native ENTER observation |

The 21:9 and 32:9 transformed values are exactly consistent with applying the
existing HorPlus function to an authored native reference near `90`, but those
retained reports do not directly record the native ENTER input. They are useful
consistency evidence, not direct provenance evidence.

The F7 probe is the important counterexample to a universal literal `90`: it
captured `90.65574` at ENTER and then observed native convergence to `90.0`.
This may represent an initialization/interpolation tail rather than a different
authored cinematic setting, but it proves that the first native ENTER sample is
not guaranteed to be exactly `90.0`.

## 3. Classification of possible `R` semantics

```yaml
fixed_literal_90:
  not_established

authored/native_ENTER_sample_per_episode:
  strongest_available provenance model

gameplay_FOV_at_cinematic_ENTER:
  not established; must not be substituted for the cinematic reference

other_native_camera_reference:
  possible for initialization tails; requires episode-level evidence
```

The current source already receives the actual ENTER `XMM0` value at the
validated cinematic boundary before applying the existing HorPlus transform.
Therefore the least-assumptive future model is to capture the exact native
ENTER reference for the episode, together with its aspect and provenance,
rather than hardcoding `90`.

## 4. Mathematical consequences

For the candidate transfer:

```text
nativeMatched(C) =
    2 * atan(tan(C / 2) * tan(G / 2) / tan(R / 2))

MatchGameplay(C) = HorPlus(nativeMatched(C), effectiveCinematicAspect)
```

The following properties are mathematically proven for finite valid inputs:

```yaml
endpoint_alignment_when_C_equals_R: PROVEN
identity_when_G_equals_R: PROVEN
tangent_space_authored_variation_preservation: PROVEN
```

They do not depend on `R=90`. They depend only on using the same captured
reference `R` consistently for the episode.

## 5. Recommended provenance contract

The future episode state should conceptually retain:

```cpp
CinematicReference {
    float nativeEnterFov;       // R, exact ENTER observation
    float effectiveAspect;
    float gameplayNativeBaseline;
    float gameplayHorPlusBaseline;
    generation;
    provenance;
    validity;
};
```

The `nativeEnterFov` value must be captured before transformation. It must not
be reconstructed from the transformed ENTER value, the gameplay baseline, or a
later converged writer sample. If the ENTER reference is invalid or unavailable,
MatchGameplay must remain unselected/fail closed until a separate contract is
established.

## 6. Current verdict

```yaml
R_is_universal_90: NOT ESTABLISHED
R_as_exact_episode_ENTER_native_sample: BEST_SUPPORTED MODEL
R_as_gameplay_baseline: NOT ESTABLISHED
21_9_and_32_9_90_consistency: SUPPORTED, partly inferred
90.65574_initialization_counterexample: OBSERVED in historical 2.0.4 evidence
MatchGameplay_formula_endpoint_property: MATHEMATICALLY PROVEN
STALKER2_episode_semantics: NOT ESTABLISHED
production_suitability: RUNTIME REQUIRED
```

## 7. Minimal remaining runtime evidence

The next combined diagnostic session should record, per cinematic episode:

```text
pre-ENTER stable gameplay native/HorPlus pair
exact native ENTER input before transformation
transformed ENTER output
any post-ENTER native convergence
dynamic authored/native cinematic range
effective cinematic aspect
native EXIT target
post-EXIT gameplay native/HorPlus pair
```

The key comparison is whether the exact ENTER sample remains the correct
episode reference when native convergence occurs, and whether an episode with
an ENTER value different from `90` preserves the proposed tangent-space
relationship. No additional runtime is needed merely to re-test the already
proven `G=R` identity case.
