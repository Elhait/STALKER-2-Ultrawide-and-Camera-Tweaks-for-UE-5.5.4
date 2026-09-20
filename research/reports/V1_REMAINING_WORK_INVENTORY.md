# V1 Remaining Work Inventory

Date: 2026-09-20

## Current milestone

The combined runtime milestone is recorded in
`GLOBAL_HORPLUS_RUNTIME_PASS.md`. It covers Global HorPlus, Gameplay
HorPlus, NativeHorPlus, GameplayHorPlus, F11/F12, Auto/forced cinematic
aspect, ADS/binocular, Dialogue coexistence, save/load/camera recreation and
legacy-aspect cleanup regression. The cached ENTER numeric guard remains an
ambiguity without an observed defect.

## Product-area inventory

### Gameplay

`NO_ACTION`: Gameplay HorPlus and AspectRecalculation both passed the combined
runtime gate. Dynamic aspect, arbitrary aspect and invalid-input behavior are
deterministically covered.

### Cinematics

`NO_ACTION`: NativeHorPlus, GameplayHorPlus, ENTER baseline selection, EXIT
recovery and Auto/forced policy behavior passed the supplied runtime gate.
The cached ENTER guard remains `RESEARCH_IF_DESIRED`, not a confirmed defect.

### Dialogue

`NO_ACTION`: Dialogue policy/lifecycle coexistence passed. No new classifier
work is justified without contradictory runtime evidence.

### Transitions and recovery

`NO_ACTION`: F11 round trip and restoration cleanup regression passed. Existing
recovery state remains production-required.

### Configuration and hotkeys

`NO_ACTION`: F11, F12 and aspect-policy cycling are runtime-confirmed and
deterministically tested. Documentation/config-template consistency remains
polish only.

### Diagnostics

`POLISH`: keep reusable Diagnostics-gated telemetry, but periodically review
whether historical MatchGameplay prediction output is still worth shipping.
No deletion is safe in this batch because it remains used for runtime
evidence.

### Resolver and compatibility

`MAINTENANCE`: maintain executable identity, unique-match/decode validation and
the supported-build manifest. A future game patch requires a fresh identity
and resolver regression; no current defect was established.

### Tests

`MAINTENANCE`: continue expanding the evidence corpus only when a new runtime
log provides a deterministic invariant. The current corpus already covers the
stable mathematical/state contracts.

### User-facing behavior

`POLISH`: keep configuration names, defaults and mode descriptions aligned
with the validated behavior. No user-visible camera feature is currently
missing from the accepted runtime scope.

## Classification

```text
IMPLEMENT_NEXT:
  none confirmed by the current runtime milestone

POLISH:
  diagnostic-surface review after the current evidence is no longer needed
  configuration/template/help-text consistency

MAINTENANCE:
  executable identity/resolver compatibility updates for future game builds
  evidence-corpus additions when new deterministic runtime evidence appears

RESEARCH_IF_DESIRED:
  cached ENTER provenance-based guard replacement
  further UE ownership investigation without a reproduced production defect

NO_ACTION:
  Gameplay HorPlus
  NativeHorPlus and GameplayHorPlus
  F11/F12 and aspect policy switching
  ADS/binocular, Dialogue coexistence and save/load/recovery
```

`UNKNOWN` is preserved as uncertainty, not treated as a blocker.

## Proposed next pack

Because `IMPLEMENT_NEXT` is empty after the runtime PASS, no new behavioral
implementation pack is justified. The next bounded pack, if v1 work continues,
should be a maintenance/polish pack:

1. Resolver compatibility and executable-identity regression review for the
   next supported game build.
2. Configuration/template/user-facing naming consistency check for F11/F12,
   Gameplay modes and cinematic policies.
3. Diagnostic payload review: retain reusable evidence, remove only telemetry
   proven no longer needed by runtime validation.

This pack must not be presented as a new camera feature or used to reopen the
accepted HorPlus architecture without contradictory evidence.

## Final compact list

```makefile
IMPLEMENT_NEXT:
  none confirmed
POLISH:
  diagnostics review; configuration/help consistency
MAINTENANCE:
  resolver/executable compatibility; evidence corpus maintenance
RESEARCH_IF_DESIRED:
  cached ENTER provenance guard; new UE ownership research
NO_ACTION:
  validated Gameplay/Cinematic/Dialogue/transition behavior

proposed_next_pack:
  - resolver and executable-identity compatibility maintenance
  - configuration/template/user-facing consistency
  - diagnostics retention review after runtime evidence stabilizes
```

## Validation status

```yaml
runtime_milestone: GLOBAL_HORPLUS_RUNTIME_PASS
cleanup: BOUNDED
production_behavior_changed: NO_INTENDED
game_launch_in_this_task: NO
```
