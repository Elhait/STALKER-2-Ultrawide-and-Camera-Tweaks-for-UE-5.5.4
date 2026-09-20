# Stable Gameplay Baseline Capture Contract

Status: offline design only; no production behavior changed.

## 1. Established evidence

| Scenario | Stable native endpoint | Transient endpoint | Return |
|---|---:|---:|---:|
| Menu FOV 80 | 80.6721 | 72.597 | 80.6721 |
| Menu FOV 100 | 100.295 | 90.2558 | 100.295 |
| Menu FOV 110 | 110.623 | 99.5499 | 110.623 |

The stable native endpoint is confirmed for the tested gameplay trajectories. A
fixed number of stable writer samples is not sufficient evidence of baseline
ownership: the ADS endpoint can also remain stable for multiple samples.

The observed sequence is therefore evidence for a stable endpoint, not yet for
an authoritative configured/menu value.

Relevant constraints:

- `ZOOM_IN`/`ZOOM_OUT` are observation boundaries, not a persistent zoom-owner
  state. Binocular internal magnification can bypass those hooks.
- Dialogue, cinematic and recovery have independent lifecycle signals.
- Gameplay-writer source must not be conflated with DialogueBoundary source.
- Exact values such as 70, 80, 90, 100 or 110 are not production contracts.
- `lastZoomDirection` is historical observation only.
- No hardcoded FOV and no wall-clock timeout are valid baseline evidence.

## 2. Diagnostic model

The model remains diagnostic-only and must not be named or treated as
`ConfiguredGameplayFov`:

```cpp
StableGameplayBaseline {
    float value;
    bool valid;
    BaselineProvenance provenance;
};
```

Recommended provenance values:

- `Unknown` — no trustworthy baseline exists;
- `ObservedStableCandidate` — a stable endpoint was observed, but ownership is
  ambiguous;
- `ConfirmedOrRestoredBaseline` — an existing baseline was preserved or the
  native trajectory returned to it;
- `ReplacementCandidate` — a distinct endpoint followed evidence that could be
  a user setting change, but replacement is not yet confirmed.

The provenance is descriptive. It must not drive HorPlus, Dialogue, Cinematics,
CameraState ownership or MatchGameplay behavior.

## 3. Baseline establishment

With no previous baseline, the diagnostic observer may record an
`ObservedStableCandidate` only when all of the following hold:

- gameplay presentation/coordinator context is valid;
- Dialogue is inactive and post-cinematic exclusion is clear;
- native writer FOV and source are readable and finite;
- the source/FOV pair remains coherent across repeated observations;
- no known transition boundary is currently being observed.

This is not sufficient to promote the value to an authoritative baseline. The
observer must retain `Unknown` or `ObservedStableCandidate` when a zoom,
binocular, dialogue, cinematic or recovery relationship cannot be excluded.

Fail-closed behavior: do not invent a baseline and do not substitute a menu
value, 90, a Dialogue target, or a transient writer sample.

## 4. Baseline preservation

Once a baseline candidate is established, preserve it across a known modifier
trajectory:

```text
A → modifier transition → B → modifier exit → A
```

The intermediate endpoint `B` must not replace `A` merely because it is stable
for several writer samples. This applies to tested ADS trajectories and must
also remain the default diagnostic interpretation for binocular equip and
internal magnification until stronger ownership evidence exists.

Dialogue, Cinematic and post-cinematic recovery remain separate domains. Their
signals may invalidate or suspend an observation, but they do not establish a
new gameplay baseline.

Current evidence supports preservation strongly for the tested A/B/A pattern.
It does not justify adding a global Zoom owner or special semantics for 0 or
0.25 markers.

## 5. Baseline replacement

The following sequence is observed in the existing runtime data:

```text
A → distinct stable endpoint C → later gameplay observations at C
```

However, the current evidence does not contain an independent authoritative
event identifying this as a user FOV-setting change. Therefore automatic
replacement is **NOT ESTABLISHED**.

In particular, these conditions are insufficient by themselves:

- C is stable for N samples;
- C differs from A;
- no recent ZOOM observation exists;
- a wall-clock interval elapsed;
- C matches a known numeric value.

Until an independent configuration-change signal is found, a diagnostic
observer should retain the previous confirmed baseline and mark C as
`ReplacementCandidate` or `Unknown`.

## 6. Ambiguous contexts

If a previous baseline is valid and the new observation is ambiguous:

```text
retain previous baseline
do not replace
record ambiguity if diagnostics are enabled
```

If no baseline exists, remain `Unknown`. This is the required fail-closed
behavior for a future diagnostic prototype.

## 7. Current classification

```yaml
stable_native_endpoint: CONFIRMED
baseline_establishment: PARTIALLY ESTABLISHED
baseline_preservation: STRONGLY SUPPORTED for tested A/B/A trajectories
baseline_replacement: NOT ESTABLISHED
authoritative_configured_source: NOT ESTABLISHED
production_MatchGameplay: NOT IMPLEMENTED
```

## 8. Required signals for the next diagnostic prototype

The next diagnostic-only implementation should expose, without changing
behavior:

- current native gameplay FOV and source;
- retained baseline value and validity;
- baseline provenance;
- candidate value and candidate provenance;
- transition-boundary observations, explicitly historical;
- presentation/coordinator/Dialogue/recovery context;
- whether an observation was preserved, suspended, rejected or marked
  replacement-candidate.

It must not infer current zoom ownership from the last zoom direction and must
not promote a replacement solely from stability or elapsed time.

## 9. Combined runtime acceptance scenario

The next game run should combine the remaining expensive checks:

1. Establish a gameplay endpoint.
2. Perform ADS and verify A/B/A preservation.
3. Perform binocular equip and internal magnification; verify that neither
   transient endpoint silently replaces the retained baseline.
4. Change the gameplay FOV setting and observe whether the new endpoint can be
   distinguished from a modifier without an independent event.
5. Run Dialogue and recovery.
6. Run Cinematic ENTER/ACTIVE/EXIT with Task 9 telemetry enabled:
   writer input, cached ENTER FOV, enter aspect, cache validity, numeric guard,
   HorPlus application and output FOV.
7. Verify that ambiguous observations remain fail-closed and that no
   production behavior changes are attributed to this diagnostic model.

No separate runtime should be spent on baseline establishment alone.
