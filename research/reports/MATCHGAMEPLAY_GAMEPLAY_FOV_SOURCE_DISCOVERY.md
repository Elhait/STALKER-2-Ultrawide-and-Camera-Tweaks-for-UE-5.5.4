# MatchGameplay — Gameplay FOV Source Discovery

## Scope

Research-only source and evidence inventory for a future cinematic
`MatchGameplay` mode. No production code, harness, build or game runtime was
changed or run.

The objective was to identify an authoritative or stable gameplay-FOV source
that is separate from transient camera modifiers such as ADS, binoculars and
other zoom transitions.

## Binary identity gate

The current installed executable was inspected before any executable/Ghidra
analysis:

```text
Observed SHA-256:
61BC1E030740CEBC30CF1DAD0C86CF65E39E12FF0500225821D684181E08D56B

Image base: 0x140000000
Sections: 11
Observed .text RVA: 0x1000
Observed .text virtual size: 0x7CCC008
Observed .text raw size: 0x7CCC200
Observed image size: 0xB963000
```

The reusable historical executable evidence in this repository was produced
against a different image/layout, including a historical `.text` size of
130,818,560 bytes (`0x7CC2200` in the identity record). The current image
therefore does not satisfy the historical static-analysis identity gate.

Result:

```yaml
current_hash: observed, but no matching current Ghidra image established
historical_static_image: not valid for current executable
executable_static_analysis: STOPPED
```

No current-image Ghidra conclusion is promoted by this report.

## Current source/dataflow findings

### Validated gameplay writer

The current source hooks the validated gameplay writer at the output boundary
and receives the current writer `XMM0` value. `ApplyHorPlusGameplay()` then
reads the camera context aspect and flags and evaluates HorPlus for the current
sample.

The current production source does not read a separate configured gameplay-FOV
setting before this transform.

### Existing current runtime state

The following values exist in the current source, but none is an authoritative
configured gameplay setting:

| Candidate | Classification | Reason |
| --- | --- | --- |
| writer `XMM0` / source camera FOV | TRANSIENT CAMERA VALUE | Changes for ADS, binoculars, dialogue, recovery and other transitions |
| `[RSI+0x230]` from historical matched-writer evidence | TRANSIENT CAMERA VALUE | It is the native value at the writer boundary, not a durable setting source; current-image proof is not re-established |
| `g_lastGameplayCameraFov` | DERIVED/OBSERVED LAST SAMPLE | Stores the last observed writer value, not a configured value |
| `g_horPlusTelemetryNativeFov` | DERIVED/DIAGNOSTIC OBSERVATION | Telemetry state, not production ownership or user configuration |
| `g_dialogueBaseline` | TRANSIENT DIALOGUE BASELINE | Captured for a Dialogue lifecycle and explicitly unsuitable as gameplay configuration |
| `g_cinematicTransformedFov` | DERIVED CINEMATIC VALUE | Episode-local cinematic output/cache, not gameplay baseline |

The existing source therefore correctly keeps `configured=UNKNOWN` in
diagnostic output. There is no safe current-code shortcut that turns the last
writer sample into `ConfiguredGameplayFov`.

## Previously investigated FOV metadata

Existing historical reports contain reflected/native metadata for
`SetFieldOfView` and related camera methods. Those reports establish names and
registration-family candidates, but they do not establish a current-image
standalone accessor, durable settings storage, or a validated path from that
metadata to the user's gameplay FOV.

Because the current executable failed the historical identity/layout gate,
those findings are retained as research leads only:

```yaml
SetFirstPersonFieldOfView metadata: OBSERVED IN HISTORICAL EVIDENCE
current configured-FOV storage: NOT ESTABLISHED
current standalone accessor: NOT ESTABLISHED
```

The metadata must not be used as a production hook or as proof that the value
is the requested gameplay setting.

## Candidate classification

```yaml
AUTHORITATIVE CONFIGURED SOURCE:
  NOT ESTABLISHED

STABLE BASELINE SOURCE:
  NOT ESTABLISHED in current source

DERIVED/INFERRED SOURCE:
  last gameplay writer sample exists,
  but is not authoritative and must not be promoted automatically

TRANSIENT CAMERA VALUE:
  writer XMM0 / RSI+0x230 class — established as the current camera sample

CURRENT_IMAGE_NATIVE_ACCESSOR:
  NOT ESTABLISHED; executable audit stopped at identity gate
```

## Current cache and practical baseline options

The current production code has no configured-FOV cache that could safely feed
`MatchGameplay`. The reusable diagnostic state can describe the last native
writer sample, but it cannot distinguish a user setting from a transient
modifier by itself.

If an authoritative source remains unavailable, the least invasive runtime
strategies are:

1. Observe stable Gameplay-owned writer samples only while no Dialogue,
   Cinematic, Recovery or active ZOOM transition is present. This can produce
   a `Stable Baseline Candidate`, not an authoritative configured value.
2. Correlate a deliberate in-game FOV-setting change with the first stable
   post-change Gameplay sample. This still needs a runtime contract proving
   that the setting change is not represented only as another transient camera
   modifier.
3. Re-run current-image static analysis only after a matching identity header
   and known runtime anchor are available, then inspect a validated accessor or
   storage writer rather than guessing from metadata names.

None of these strategies should be implemented as `ConfiguredGameplayFov`
until its provenance is established.

## Required MatchGameplay inputs

The future mathematical contract will require separate evidence for:

```text
base/configured gameplay FOV
native authored cinematic FOV
cinematic reference aspect
effective runtime cinematic aspect
```

The current source already has the latter three only in separated/episode-local
forms: cinematic ENTER `XMM0`, the native reference aspect constant and the
resolved cinematic aspect. The base/configured gameplay FOV remains unknown.

No MatchGameplay formula is proposed in this report.

## Verdict

```yaml
authoritative gameplay FOV source: NOT ESTABLISHED
stable baseline source: NOT ESTABLISHED
transient writer FOV: CONFIRMED AS NON-AUTHORITATIVE
current production cache sufficient for MatchGameplay: NO
historical SetFieldOfView lead: DEFERRED / NOT PROMOTED
current-image executable audit: STOPPED BY IDENTITY GATE
production changes: NONE
runtime required next: YES, only after selecting telemetry strategy
```

This does not block MatchGameplay permanently. It establishes that the next
useful step is either a matching current-image native source audit or a bounded
runtime experiment for a stable baseline candidate. It does not justify
hardcoding `90` or treating any zoom-modified writer sample as configured FOV.
