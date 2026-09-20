# Camera State Model Architecture Audit — Steam 2.0.5/2.0.6 Source Review

## Scope

Read-only source/state-model audit. No production source, behavior, hooks,
diagnostics, build outputs or runtime state were changed. No new game run and
no 2.0.6 Ghidra analysis were performed.

The audit compares the proposed two-axis model:

```text
PresentationContext: Gameplay | Cinematic
InteractionContext:  Free | Common | Dialogue
TransitionPhase:     Stable | Entering | Exiting
```

against the current implementation.

## Executive result

The proposed separation is directionally correct, but it should be introduced
as a snapshot of independent substates rather than as a replacement enum:

```text
PresentationContext  already exists partially as CoordinatorState.
Dialogue lifecycle   already exists as a separate phase machine.
Common zoom          exists only as native ZOOM_IN/ZOOM_OUT evidence; it is not
                     currently represented in production state.
TransitionPhase      is distributed across DialoguePhase, coordinator and
                     native replay/recovery state.
```

The minimum useful future model is therefore a read-only derived view first,
not a new owner that rewrites all existing state machines:

```text
PresentationContext = CoordinatorState-derived
InteractionContext  = Dialogue lifecycle or neutral gameplay zoom evidence
TransitionPhase     = existing Dialogue/recovery/coordinator phase-derived
```

`ZOOM_IN`/`ZOOM_OUT` should remain neutral transition evidence. They should not
be renamed to ADS ownership and should not become Dialogue ownership signals.

The candidate structure for a future snapshot is:

```cpp
CameraState {
    PresentationState presentation;
    ZoomTransitionState zoom;
    DialogueState dialogue;
    GameplayMode gameplayMode;
    CameraEvidence evidence;
}
```

This is an audit model, not an approved implementation shape. The important
property is that Presentation, Zoom, Dialogue and GameplayMode are not forced
into one global super-enum.

Every field must retain provenance:

```text
Observed fact             native event or numeric observation
Derived/confirmed state   validated lifecycle state
Classifier hypothesis     Candidate or other provisional inference
```

For example, Cinematic ENTER and ZOOM callbacks are observations; `FOV≈70` is
a numeric observation; Dialogue `Candidate` is a hypothesis; Dialogue `Active`
is a confirmed/derived lifecycle state. `Candidate` must never be presented as
authoritative camera ownership.

## Current distributed state map

| Current source/state | Actual role | Status |
| --- | --- | --- |
| `gameplay::CoordinatorState` | `Gameplay`, `CinematicActive`, `CinematicExiting`; presentation/recovery boundary | CONFIRMED BY SOURCE |
| `ReplayState` | AspectRecalculation replay/handoff state, not generic camera ownership | CONFIRMED BY SOURCE |
| `dialogue::Phase` | `Inactive`, `Candidate`, `Active`, `Exiting`, `RearmPending` | CONFIRMED BY SOURCE |
| `PolicySnapshot` | selected policy vs active lifecycle policy | CONFIRMED BY SOURCE |
| `RecoveryRearm` | source-aware native-target convergence and cancellation | CONFIRMED BY SOURCE; first-target completion repair is not runtime-validated after latest build |
| `PostCinematicRecoveryExclusion` | suppresses Dialogue inference during native post-cinematic recovery | CONFIRMED BY SOURCE |
| `ZOOM_IN`/`ZOOM_OUT` hooks | diagnostic/native transition samples; no production owner state | CONFIRMED BY SOURCE and runtime for tested 2.0.6 ADS trajectory |
| `runtimeGameplayMode` | `AspectRecalculation` vs `HorPlus` production dispatch | CONFIRMED BY SOURCE |
| native writer `XMM0` | current native FOV input at validated writer boundary | CONFIRMED BY SOURCE; runtime telemetry confirms observed values |
| `RSI+0x2C` | native target/end FOV in the inspected blend context | CONFIRMED BY SOURCE for inspected path and consistent runtime traces |
| aspect/flags at camera source | effective runtime aspect/policy-state inputs | CONFIRMED BY SOURCE |

## Proposed model mapped to current code

### Presentation context

`CoordinatorState::Gameplay` is the current gameplay presentation boundary.
`CinematicActive` is the cinematic presentation boundary. `CinematicExiting`
is a transitional presentation/recovery state, not a stable presentation
context.

This supports the proposed `PresentationContext`, but the current coordinator
also carries recovery timing. A future derived model should preserve that
distinction instead of flattening `CinematicExiting` into either stable state.

### Interaction context

Dialogue has a real production lifecycle and policy snapshot. Neutral gameplay
zoom does not: the source currently exposes ZOOM signals only in the diagnostic
path. The proposed `Common` context is therefore a valid architectural target,
but it is **NOT ESTABLISHED as a production state**.

The tested ZOOM path must remain neutral:

```text
ZOOM_IN / ZOOM_OUT
    = native transition samples
    ≠ ADS ownership
    ≠ Dialogue ownership
```

No source evidence justifies making `Common` imply ADS, or making every zoom
transition a Dialogue candidate.

### Transition phase

The phase concept already exists, but in separate owners:

```text
Dialogue Candidate / Active / Exiting / RearmPending
Coordinator CinematicActive / CinematicExiting
ReplayState AppliedConstrainPass / Complete
native ZOOM samples
```

These should not be merged blindly. A future `TransitionPhase` can be a
derived reporting layer until there is a concrete ambiguity it must resolve.

## Evidence provenance and update authority

| Substate/evidence | Provenance | Current update authority | Threading/serialization status |
| --- | --- | --- | --- |
| Presentation/coordinator | mod-derived state from Cinematic ENTER/EXIT and writer recovery | cinematic hooks and gameplay writer | atomic coordinator; Dialogue state additionally uses `g_dialogueMutex` |
| Dialogue Candidate | classifier hypothesis from FOV trajectory | Dialogue boundary callback | mutex-protected, but not native ownership evidence |
| Dialogue Active/Exiting/RearmPending | confirmed/derived mod lifecycle after classifier activation | Dialogue boundary callback and reset paths | mutex-protected; policy snapshot belongs to lifecycle |
| Native target/source | native numeric observation and source identity | Dialogue boundary callback reads context | guarded `SafeRead`; source change cancels recovery |
| ZOOM_IN/OUT | native transition observation | diagnostic ZOOM hooks | thread-local edge snapshots; authoritative lifecycle not established |
| Gameplay mode | config/hotkey-derived policy | config initialization and hotkey loop | atomic mode/transition flags |
| runtime aspect/flags | native numeric observation | gameplay writer/mode transition path | guarded reads; mode transition defers if unavailable |

The current source does not provide one centralized serialization point for all
these authorities. A future snapshot should be assembled at existing callback
boundaries, not become a new callback that competes with them.

## Source identity and generation

The latest stale-ownership defects justify a minimal invalidation concept:

```text
camera source changes
or cinematic/presentation epoch changes
    ↓
observations and subordinate lifecycles from the old generation
    ↓
cannot complete or transform the new camera state
```

Current source already has partial protection:

- `RecoveryRearm` stores a native source and cancels on source change;
- post-cinematic exclusion stores source and native target evidence;
- cinematic ENTER/EXIT reset several subordinate states;
- gameplay mode transitions clear mode-specific transition state.

Current source does **not** have one explicit `CameraSourceId` or
`presentationEpoch` shared by Dialogue, ZOOM and all FOV telemetry. A shared
generation could remove a class of stale-state bugs, but its exact ownership
and update points are not established. This is a recommended design question,
not an implementation decision from this audit.

## Transition contract

| Current state | Observable evidence | Allowed next state | Expected FOV behavior | Current mod action |
| --- | --- | --- | --- | --- |
| Gameplay + no Dialogue lifecycle | coordinator Gameplay; Dialogue Inactive | Common transition, Dialogue Candidate, CinematicActive | native gameplay FOV or native zoom samples | HorPlus transforms current gameplay writer input in HorPlus mode; AspectRecalculation owns its existing path |
| Gameplay + ZOOM_IN | native ZOOM_IN samples; no positive Dialogue signal | Common/Entering candidate only as a derived interpretation | native zoom trajectory continues sample-by-sample | no Dialogue ownership; no separate ZOOM FOV transform |
| Gameplay + neutral zoom | ZOOM samples stop; no Dialogue state | Gameplay stable/Common Exiting → Free | native writer returns toward native gameplay state | downstream writer path handles current input |
| Gameplay + Dialogue Candidate | FOV descent heuristic only; no positive ownership proof | Active or Inactive | no transform until Candidate is confirmed | Candidate hardening may cancel or activate |
| Gameplay + Dialogue Active | active policy snapshot valid | Exiting | Dialogue transform uses current native samples | Dialogue FOV transform only |
| Gameplay + Dialogue Exiting | ascending native trajectory and Dialogue lifecycle | RearmPending or reset/cancel | Dialogue exit transform while lifecycle owns it | native target/source recovery validation |
| Gameplay + Dialogue RearmPending | source and native target observed | Inactive on first valid target-converged sample; Cancel on source change | no stale ownership after completion | `RecoveryRearm` completes against native target, not `baselineG` |
| Cinematic stable | Coordinator CinematicActive; cinematic hook ownership | CinematicExiting; Dialogue may coexist if separately evidenced | cinematic aspect/FOV policy | cinematic hooks own cinematic changes; gameplay correction is not the owner |
| Cinematic + Dialogue | possible overlap; current code does not positively establish all such cases | cinematic exit and/or Dialogue lifecycle transition | Dialogue may exist without its own FOV trajectory | do not infer Dialogue solely from FOV |
| CinematicExiting | native EXIT trajectory; coordinator transition | Gameplay after native/replay boundary depending mode | native recovery trajectory | HorPlus returns Gameplay immediately; AspectRecalculation uses existing handoff/replay path |
| Gameplay mode switch | F11/config mode transition | target mode after safe writer boundary | HorPlus transforms current input; AspectRecalculation retains native validated correction | existing mode dispatch and deferral logic |

## Dialogue endpoint model

The current Dialogue subsystem already has an implicit endpoint model, but it
contains more than one kind of endpoint and they must not be collapsed:

```text
ObservedNativeTarget       = native target/end FOV, e.g. RSI+0x2C
ExpectedNativeDialogueEndpoint = native dialogue camera endpoint if available
ExpectedModDialogueEndpoint    = policy-derived output target
ExpectedGameplayEndpoint       = native/configured gameplay endpoint
```

The current transformations in `src/dialogue/dialogue_fov.cpp` use a native
reference pair of `90` and `70` to derive projection-space behavior:

```text
AdaptiveTarget(baseline)
ReducedTarget(baseline)
TransformProjectionSample(...)
TransformExitSample(...)
```

This gives a useful normalized model but not yet a fully arbitrary configured
FOV contract:

```text
A = expected endpoint for the specific context
B = expected endpoint for the specific Dialogue policy
A → B = Dialogue entry
B → A or ObservedNativeTarget = Dialogue exit/recovery, only when source evidence
                                 establishes that equivalence
```

Status:

- `90 ↔ 70` as the tested runtime anchor: CONFIRMED BY RUNTIME.
- Native target as recovery endpoint evidence: CONFIRMED BY SOURCE/RUNTIME for
  the inspected blend context.
- Universal endpoint computation independent of the internal `90/70` reference:
  NOT ESTABLISHED.
- `±0.1°` as a separate endpoint-validation tolerance: PLAUSIBLE design,
  not currently a production contract.
- FOV proximity alone identifying Dialogue: explicitly rejected by runtime
  evidence.

The correct ordering remains:

```text
Dialogue ownership/lifecycle evidence
    → expected endpoint validation
    → Dialogue FOV transformation
```

not:

```text
FOV ≈ 70
    → assume Dialogue
```

## Cinematic + Dialogue overlap

The source coordinator guard rejects Dialogue processing whenever the
coordinator is not `Gameplay`, while the cinematic path owns ENTER/EXIT and
can reset Dialogue state at lifecycle boundaries. This means the current code
does not model `Cinematic + Dialogue` as a simultaneous production-owned
combination.

The proposed overlap is nevertheless a valid compatibility requirement because
the game may contain dialogue inside cinematic content and that dialogue may
not produce a separate Dialogue FOV trajectory.

Precise status:

- Cinematic and Dialogue are conceptually not mutually exclusive in the game:
  CONFIRMED BY PROVIDED RUNTIME/RESEARCH EVIDENCE.
- Current standalone ASI can independently own both at the same time:
  NOT ESTABLISHED.
- A cinematic Dialogue sequence must create a separate Dialogue FOV transform:
  NOT ESTABLISHED and should not be assumed.

Therefore no new combined state should be implemented from this audit alone.

Conceptually, `Cinematic + Dialogue` remains a permitted combination. The
current ASI does not need to materialize that combination as one unified owner
unless an observable native/mod signal requires it for correct FOV behavior.

## Mode interaction

`SelectGameplayMode` and `ResolveGameplayModeTransition` already provide a
bounded mode transition contract:

```text
AspectRecalculation → HorPlus
    invalidate/re-arm physical transition as needed
    defer until Gameplay and runtime aspect is readable

HorPlus → AspectRecalculation
    clear HorPlus transition state/telemetry
    restore existing AspectRecalculation ownership
```

The HorPlus writer path accepts `Gameplay` and `CinematicActive`, with a
cinematic cached-value guard. AspectRecalculation retains its existing replay
and handoff path when HorPlus is not selected.

This supports mode separation. It does not establish that a unified state
object should own both mechanisms.

## Ambiguities the model would help remove

1. A native FOV descent would be recorded as an observation inside a known
   lifecycle, not treated as ownership by itself.
2. Neutral zoom would have a named `Common` interpretation without reviving
   ADS-specific ownership semantics.
3. Dialogue recovery would be an explicit endpoint transition rather than a
   generic “stable near baseline” condition.
4. Cinematic recovery would remain distinct from gameplay zoom and Dialogue.
5. Mode dispatch would remain an orthogonal policy axis instead of being
   inferred from FOV shape.
6. Source/generation invalidation would make the provenance of a subordinate
   observation explicit instead of allowing an old lifecycle to explain a new
   writer sample.

## Where centralization would add little value

- Replacing the existing `CoordinatorState`, `ReplayState` and `DialoguePhase`
  immediately would add coupling without new evidence.
- Making `ZOOM_IN/OUT` mutate a global owner would create a new false-positive
  surface and possibly replace stale Dialogue with stale Common; they are
  currently strongest as neutral evidence until their lifecycle is established.
- Moving Dialogue endpoint math into a global camera router would mix Dialogue,
  Cinematics and Gameplay ownership that the current validated paths keep
  separate.
- Adding a full enum for unobserved combinations such as `Cinematic + Common`
  would be speculative.

## Minimal implementation sequence if later approved

1. Add a pure, read-only `CameraStateSnapshot`/derived-view type containing
   independent presentation, zoom evidence, Dialogue lifecycle, gameplay mode,
   source/generation and native evidence fields.
2. Define provenance and update authority for every field; populate the view at
   existing boundaries only.
3. Add harness tests for valid combinations and explicitly rejected stale
   ownership/generation cases.
4. Use the snapshot first for telemetry and invariant checks, not behavior.
5. Only after those checks pass, consider replacing one heuristic at a time
   with a stronger existing signal.

The first implementation task should be a separate bounded design-to-code
batch for a read-only snapshot and harness coverage. It must not change FOV
math, Dialogue policy, ZOOM behavior, Cinematic behavior or AspectRecalculation.

## Final status

```yaml
Camera state is distributed:                 CONFIRMED BY SOURCE
Independent-substate snapshot is useful:     PLAUSIBLE / strongly supported
Presentation context already exists:         CONFIRMED BY SOURCE
Dialogue lifecycle is independently owned:   CONFIRMED BY SOURCE
Common zoom production lifecycle exists:     NOT ESTABLISHED
ZOOM as neutral observation:                 CONFIRMED BY SOURCE/RUNTIME
Dialogue endpoint uses native target:        CONFIRMED for recovery path
Universal A/B endpoint model:                NOT ESTABLISHED
Cinematic + Dialogue overlap in game:       CONFIRMED by provided evidence
Standalone simultaneous ownership:           NOT ESTABLISHED
Shared source/generation epoch:              NOT ESTABLISHED
Immediate full state-machine rewrite:        NOT JUSTIFIED
Read-only derived snapshot as next batch:    RECOMMENDED
```
