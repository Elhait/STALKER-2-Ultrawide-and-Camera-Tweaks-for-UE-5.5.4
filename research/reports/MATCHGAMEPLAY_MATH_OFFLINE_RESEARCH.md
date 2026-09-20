# MatchGameplay Mathematics — Offline Research

Status: research only. No production formula or runtime behavior changed.

## 1. Runtime evidence used

For the tested aspect `3.55556` with native aspect `1.77778`, the current
HorPlus transform uses the tangent-space relation:

```text
H(f, a) = 2 * atan(tan(f / 2) * a / nativeAspect)
```

Observed endpoints:

```text
Gameplay native baseline       G  = 112.623
HorPlus gameplay baseline      H(G) = 143.132

Authored cinematic reference   R  = 90
HorPlus authored cinematic     H(R) = 126.870
```

The visual EXIT discontinuity is therefore explained by two different
endpoints:

```text
cinematic: H(R) = 126.870
gameplay:  H(G) = 143.132
```

At a gameplay FOV of approximately 90, `G` and `R` coincide, which explains
the earlier seamless result without requiring a different engine path.

## 2. Invariants for MatchGameplay

The future mode must satisfy all of these:

1. If the gameplay native baseline equals the cinematic reference, output must
   remain the current `H(R)` result.
2. If the gameplay baseline differs, the cinematic endpoint must meet the
   gameplay HorPlus baseline at the corresponding reference cinematic sample.
3. Authored cinematic variation must remain visible; every sample must not be
   replaced with one constant gameplay FOV.
4. Native and HorPlus values must remain in their respective spaces; degree
   addition is not automatically projection-correct.
5. The current runtime aspect must be applied exactly once.

## 3. Candidate contracts

### A. Direct replacement — rejected

```text
every cinematic sample = H(G, aspect)
```

This fixes the endpoint but destroys authored cinematic FOV variation and
therefore does not satisfy the contract.

### B. Additive degree offset — reference candidate only

```text
output = H(C, aspect) + (H(G, aspect) - H(R, aspect))
```

This matches the endpoint by construction, but the correction is additive in
degrees while the existing transform is tangent-space. It is not the preferred
mathematical contract and requires visual/runtime validation before promotion.

### C. Native tangent-space baseline transfer — preferred candidate

First preserve the authored sample relative to the cinematic reference in
native projection space:

```text
nativeMatched(C) =
    2 * atan(
        tan(C / 2) * tan(G / 2) / tan(R / 2)
    )
```

Then apply the existing aspect transform once:

```text
MatchGameplay(C, G, R, aspect) = H(nativeMatched(C), aspect)
```

Equivalent HorPlus-space form, when the same aspect is used throughout:

```text
tan(output / 2) =
    tan(H(C, aspect) / 2) *
    tan(H(G, aspect) / 2) /
    tan(H(R, aspect) / 2)
```

This candidate has the desired endpoint property:

```text
C = R
→ nativeMatched(C) = G
→ MatchGameplay = H(G, aspect)
```

It also preserves authored variation as a multiplicative projection-space
relationship instead of replacing it with a constant.

For the observed `G=112.623`, `R=90`, aspect ratio 2.0:

```text
authored C=90  → 143.132
authored C=80  → 136.671
authored C=100 → 148.749
```

These numbers are mathematical projections only, not runtime validation.

## 4. Required provenance inputs

The preferred candidate requires an episode-valid tuple:

```text
NativeGameplayBaseline
HorPlusGameplayBaseline
GameplayBaselineAspect
AuthoredCinematicReferenceFov
EffectiveCinematicAspect
```

The baseline must be stored as the complete native/result/aspect observation,
not reconstructed later from a different aspect or camera state:

```cpp
GameplayBaseline {
    float nativeFov;
    float horPlusFov;
    float aspect;
    provenance;
    validity;
};
```

The `HorPlusGameplayBaseline` value is evidence and a consistency check. The
formula should still derive its output from the native baseline in native
projection space, then apply the effective cinematic aspect once, rather than
feeding an already transformed value back through HorPlus.

## 5. What is established and what is not

```yaml
FOV 90 seamless control case: CONFIRMED RUNTIME
FOV 112 endpoint mismatch: CONFIRMED RUNTIME + VISUAL
native recovery trajectory: CONFIRMED RUNTIME
direct replacement is unsafe: ESTABLISHED BY CONTRACT
tangent-space transfer is mathematically coherent: ESTABLISHED OFFLINE
preservation of authored cinematic semantics: PLAUSIBLE, RUNTIME REQUIRED
reference FOV R is universal for all cinematic episodes: NOT ESTABLISHED
production MatchGameplay formula: NOT IMPLEMENTED
```

The preferred formula is therefore a bounded design candidate, not a production
decision yet. The remaining uncertainty is semantic: whether cinematic
authored samples should be interpreted as native-space FOV samples relative to
the observed reference `R`, and whether the same transfer is appropriate for
all cinematic episodes and policies.

## 6. Minimal future runtime validation

One diagnostic-only cinematic run should compare, for the same episode:

```text
native gameplay baseline pair
authored cinematic ENTER sample
dynamic authored cinematic samples
current effective cinematic aspect
native recovery endpoint
final gameplay HorPlus endpoint
```

Acceptance criteria:

- when gameplay baseline matches the cinematic reference, output remains the
  existing seamless result;
- when gameplay baseline differs, the proposed transfer predicts the desired
  final gameplay framing without collapsing authored variation;
- current aspect changes do not cause a second HorPlus transform;
- native recovery remains engine-owned and continuous;
- no Dialogue, ZOOM or CameraState ownership semantics are changed.

No implementation should begin until this candidate is reviewed against that
runtime evidence.
