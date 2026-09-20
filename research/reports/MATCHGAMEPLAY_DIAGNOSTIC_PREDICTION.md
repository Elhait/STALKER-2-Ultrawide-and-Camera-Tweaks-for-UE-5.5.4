# MatchGameplay Diagnostic Prediction

Status: diagnostic-only implementation; production output unchanged.

## Purpose

This batch keeps the exact cinematic ENTER native observation `E` separate from
the unresolved semantic cinematic reference `R`. It logs a prediction of the
candidate MatchGameplay transfer without applying that prediction to the game.

## Captured values

At cinematic ENTER, diagnostics snapshot the latest valid pre-ENTER gameplay
observation pair:

```text
gameplayNativeBaseline = G (diagnostic observation, not production authority)
gameplayHorPlusBaseline = H(G)
gameplayBaselineAspect
```

The ENTER hook separately records:

```text
matchGameplayEnterObservation = E
```

The field name intentionally does not call `E` the semantic reference `R`.

## Prediction

For each changed cinematic writer sample `C`, diagnostics compute:

```text
nativeMatched(C) =
    2 * atan(tan(C / 2) * tan(G / 2) / tan(E / 2))

candidateHorPlus = HorPlus(nativeMatched(C), currentCinematicAspect)
```

The record contains:

```text
matchGameplayCandidateAvailable
matchGameplayReferenceSource=ENTER_OBSERVATION
matchGameplayEnterObservation
matchGameplayNativeBaseline
matchGameplayHorPlusBaseline
matchGameplayBaselineAspect
matchGameplaySampleNative
matchGameplayNativeMatched
matchGameplayHorPlus
```

The candidate is explicitly observational. It does not write `XMM0`, alter
the cinematic cache, affect recovery, or change any owner/state decision.

## Interpretation limits

- `E` is an exact native ENTER observation, not proven `R`.
- The pre-ENTER gameplay pair is a diagnostic observation, not a confirmed
  configured baseline.
- Missing or invalid values produce `candidateAvailable=false`.
- The current production cinematic result and existing Task 9 telemetry remain
  unchanged.

## Runtime question this enables

The next combined run can compare an episode with an ENTER/convergence pattern
such as:

```text
E = 90.65574
C → 90.0
```

against the predicted transfer relative to `E`, without risking a production
camera write. This separates the mathematical candidate from the unresolved
question whether the ENTER sample or a later native value is the correct
semantic cinematic reference.

## Validation status

The implementation is intended to remain behind `[Diagnostics] Enabled=true`.
Production behavior is unchanged. Runtime prediction validation is pending the
next combined cinematic session.

## Telemetry correction before the next runtime

The first run with this diagnostic exposed a wiring issue in the `AFTER` record:
the writer register already contained the production HorPlus output when the
record was assembled. The initial `AFTER` candidate therefore used transformed
`XMM0` as `C`, while the `BEFORE` record used the native sample correctly.

This was diagnostic-only and did not affect production output. The logger now
uses the saved pre-transform `beforeFov` for `matchGameplaySampleNative` in
both `BEFORE` and `AFTER` records. The next runtime is required before any
candidate trajectory is interpreted.
