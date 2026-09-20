# DialogueClassifierV2 practical repair design

Date: 2026-09-18  
Scope: read-only design audit. No production code, ASI build, new hook or runtime launch.

## Status boundary

The ideal positive ownership path remains a research checkpoint:

```yaml
APC::IsInStaticDialog(): runtime oracle for tested static dialogue: CONFIRMED
standalone ASI access: NOT ESTABLISHED
native Dialogue ENTER/EXIT seam: NOT FOUND
UDialogManager executable path: NOT ESTABLISHED
XPlayDialog* executable path: NOT ESTABLISHED
```

This document therefore designs a conservative heuristic fallback. It must not be described as a positive ownership detector.

## A. Confirmed defects being repaired

### A1. Generic FOV false-positive

The current classifier treats a generic descending FOV as Dialogue when:

```text
valid FOV
+ coordinator == Gameplay
+ policy != Native
+ descending input
→ Candidate / Active
```

UE4SS ground truth has shown `IsInStaticDialog=false` while this path still creates a Candidate/Active lifecycle.

### A2. Post-cinematic false-positive

In HorPlus mode, `TraceCinematicExit()` returns the coordinator to `Gameplay` immediately so the EXIT remains seamless. Native FOV recovery can still be descending afterward. The Dialogue hook then sees the same generic eligibility and misclassifies the recovery.

The repair must exclude this known recovery window without delaying or reverting HorPlus cinematic ownership.

### A3. Policy mutation during an active lifecycle

The current hotkey writes `g_runtimeDialoguePolicy` directly. The active callback reads that same value on every sample. Therefore F10 can mutate an already active lifecycle, contrary to the documented “next dialogue” semantics.

This is a separate repair from ownership classification, but the design must reserve `selectedPolicy` and `activePolicy` semantics for it.

## B. Existing signals reusable without new hooks

| Signal | Meaning | Reliability as “not Dialogue” | False-negative risk | Available now |
| --- | --- | ---: | ---: | --- |
| `CoordinatorState::CinematicActive` | Cinematic owns the camera | High | Low | Yes |
| `CoordinatorState::CinematicExiting` | Existing AspectRecalculation recovery owns transition | High | Low | Yes |
| Cinematic ENTER reset | New cinematic invalidates prior Dialogue state | High | Low | Yes |
| Cinematic EXIT event + native target FOV | Starts known post-cinematic recovery | High for recovery | Low if released at convergence | Yes |
| camera source replacement | Camera context changed | High | Medium if a real Dialogue swaps source | Yes |
| material source/FOV discontinuity | Existing stale-state invalidation | High for current lifecycle | Medium | Yes |
| invalid/non-finite FOV | Sample is unusable | High | Low | Yes |
| unsupported coordinator state | Ownership is not Gameplay | High | Low | Yes |
| AspectRecalculation handoff pending | Existing gameplay replay owns transition | High | Low | Yes, only where mode arms it |
| HorPlus cinematic transition | Requires explicit new exclusion state | Candidate until implemented | Medium | Partly; current EXIT event is available |
| `g_postExitTraceArmed` | Diagnostic observation lifetime | Not reliable as lifecycle ownership | High | Yes, but currently not a release contract |

Important: `g_postExitTraceArmed` is currently set on EXIT and is not a suitable production suppression lifetime by itself. A repair must use an explicit deterministic recovery state or a dedicated lifecycle marker, not the diagnostic flag.

## C. Proposed state machine

The existing phases remain `Inactive`, `Candidate`, `Active`, and `Exiting`. V2 adds no positive ownership claim; it adds explicit rejection/invalidation gates.

### Inactive

Input is ignored when any of the following holds:

- FOV is invalid or outside the existing safe range;
- coordinator is not `Gameplay`;
- policy is `Native`;
- cinematic EXIT recovery exclusion is active;
- a source/context change has invalidated the current observation.

When eligible, the first stable valid sample establishes only a provisional baseline and enters `Candidate`. It does not transform FOV.

### Candidate

Candidate is a bounded suspicion state, not Dialogue ownership.

Keep Candidate only while all of the following remain true:

- same camera source/context;
- coordinator remains `Gameplay`;
- policy remains eligible for the selected policy snapshot;
- samples remain finite and coherent;
- no cinematic ENTER/EXIT or known replay transition invalidates the sample.

Candidate may promote to Active only after a coherent descent develops. A single lower sample is not enough evidence. The exact sample count/threshold is not established by current evidence and must remain a candidate implementation parameter until harness/runtime validation.

### Active

Active means only that the heuristic has accepted a coherent FOV transition and may apply the selected policy. It does not mean native Dialogue ownership is proven.

Any source change, large discontinuity, unsupported coordinator, policy snapshot mismatch or cinematic transition resets to `Inactive` and passes through native FOV.

Ascending samples move to `Exiting`; they do not immediately prove a new lifecycle.

### Exiting

The existing baseline recovery rule may return to `Inactive` when the input returns within the established recovery tolerance. If the source changes or a new cinematic begins, reset immediately.

## D. Post-cinematic exclusion contract

### Required behavior

```text
Cinematic EXIT
→ mark post-cinematic recovery exclusion
→ HorPlus remains immediate/seamless
→ Dialogue hook passes native recovery samples unchanged
→ release exclusion at deterministic native recovery completion
→ next valid descent may create a fresh Candidate
```

The exclusion must not restore the old `CinematicExiting → Gameplay` delay in HorPlus mode, must not arm AspectRecalculation, and must not alter cinematic FOV or AspectRecalculation behavior.

### Recommended release condition

Use the existing native writer/recovery boundary already used by the mod, not a timer.
The exclusion is explicitly two-stage:

```text
WaitingForRecovery:
  keep exclusion active, including when the first sample is already at target

Recovering:
  enter only after a same-source finite sample moves away from target
  by more than the existing recovery epsilon
```

Release only after the recovery stage has started:

```text
same expected camera source
+ finite current FOV and finite EXIT target
+ recovery has first moved away from the EXIT target by more than the existing epsilon
+ abs(current FOV - EXIT target) <= existing recovery epsilon
→ clear post-cinematic Dialogue exclusion
```

An equal/near-target first sample does not prove that native recovery started and must not release the exclusion. If source changes or an invalid sample appears, cancel the exclusion and reset Dialogue state; do not continue transforming the unknown camera transition. A new cinematic ENTER always resets/re-arms lifecycle state. This two-stage condition is now implemented and covered by the helper harness.

### Immediate Dialogue after cinematic

The exclusion must end at actual recovery convergence. A real Dialogue beginning after convergence is eligible immediately. A Dialogue beginning before convergence may be conservatively missed; that is the explicitly accepted v1 tradeoff until a positive native signal exists.

## E. Candidate confirmation rules

These are candidate hardening rules, not all established production facts:

1. Preserve the existing finite-FOV and `Gameplay`/non-`Native` gates.
2. Require same-source continuity from provisional baseline through promotion.
3. Require more than one coherent descending sample before Active.
4. Reject or reset on a large FOV discontinuity, source replacement or coordinator change.
5. Do not use target `70`, `[RSI+0x2C]`, exact aspects or fixed FOV values as ownership evidence.
6. Bound Candidate lifetime by state continuity, not a wall-clock timer; if no coherent descent develops, return to `Inactive`.
7. When evidence is ambiguous, pass native FOV and prefer a false negative over arbitrary gameplay/cinematic transformation.

The exact promotion threshold and candidate lifetime still require harness coverage and one bounded runtime validation. They should not be silently chosen as if proven by the current logs.

## F. Hotkey policy snapshot contract

Separate configuration selection from active lifecycle policy:

```text
selectedPolicy = value changed by F10
activePolicy   = snapshot taken when Candidate is accepted/promoted
```

Recommended semantics:

- F10 while `Inactive`: update `selectedPolicy`; next accepted lifecycle uses it.
- F10 while `Candidate`: update `selectedPolicy`, but do not mutate the Candidate’s already selected policy if Candidate has been accepted as a transition. If Candidate has not yet crossed the promotion boundary, either restart Candidate or retain the snapshot; choose one explicitly in implementation and test it. Conservative choice: reset Candidate and require a fresh transition under the new selection.
- F10 while `Active` or `Exiting`: update `selectedPolicy` only; current lifecycle continues with `activePolicy`.
- EXIT/recovery: release `activePolicy` and return to no active snapshot.
- Native policy always disables the Dialogue transform and resets any heuristic lifecycle.

This is a separate implementation batch and must not be mixed into the post-cinematic exclusion patch.

## G. Risks and possible false negatives

- A generic camera transition may still resemble a coherent Dialogue descent; V2 remains heuristic.
- Conservative post-cinematic exclusion can miss a Dialogue that starts before recovery convergence.
- Requiring coherent samples can delay or miss very short Dialogue transitions.
- Source continuity may reject a valid Dialogue if the game legitimately swaps camera context at Dialogue entry.
- Treating F10 during Candidate as a reset is safe but may require the user to start a new transition.
- No rule here establishes coverage of every Dialogue type represented by the game’s `IsInStaticDialog()` oracle.

## H. Minimal implementation batches

### Batch 1 — confirmed post-cinematic exclusion only

- Add one explicit recovery-exclusion lifecycle state owned by the existing Cinematic EXIT/native recovery path.
- Reset Dialogue state on ENTER, EXIT and source invalidation as currently required.
- Suppress only Dialogue Candidate/Active creation while the known native EXIT recovery is pending.
- Keep a `WaitingForRecovery` stage until a same-source sample moves away from
  the captured target; release only from the subsequent `Recovering` stage at
  deterministic convergence.
- Do not change HorPlus EXIT ownership, cinematic transforms or AspectRecalculation replay.

### Batch 2 — classifier lifecycle hardening

- Separate Candidate acceptance from Active promotion.
- Add same-source/coherence checks and bounded invalidation.
- Keep thresholds derived from existing epsilons or introduce only explicitly justified constants.
- Preserve fail-closed native pass-through on ambiguity.

### Batch 3 — selected/active Dialogue policy snapshot

- Add `selectedPolicy` and `activePolicy` ownership.
- Make F10 change selection only.
- Snapshot on Candidate acceptance/promotion according to the chosen rule.
- Use the snapshot through Active/Exiting, then release it.

Do not combine these batches into one patch. Each batch needs its own static/harness validation and review before runtime testing.

## I. Runtime validation matrix

The eventual validation should be compressed into two candidate runs after static/harness tests:

### Run 1 — gameplay/cinematic negative controls

Cover in one session:

- HorPlus mode;
- AspectRecalculation mode;
- ordinary gameplay FOV transition;
- ADS hold/release;
- cinematic ENTER/EXIT;
- immediate post-EXIT gameplay;
- Dialogue policies Native/Adaptive/Reduced/Disabled where practical.

Expected: no Dialogue Candidate/Active during generic gameplay, ADS or cinematic recovery; cinematic and gameplay framing remain unchanged.

### Run 2 — positive Dialogue and policy lifecycle

Cover:

- static Dialogue;
- Dialogue immediately after cinematic if feasible;
- F10 before Dialogue;
- F10 during Active Dialogue;
- Dialogue EXIT and next Dialogue.

Expected: confirmed tested Dialogue path still receives the selected policy; F10 during Active does not mutate the active lifecycle; the next Dialogue receives the new selection; any conservative miss is recorded explicitly.

## Final decision

```yaml
Positive ownership:
  status: DEFERRED / RESEARCH CHECKPOINT
  reason: oracle confirmed, standalone seam unavailable

Practical classifier v2:
  status: DESIGN READY, NOT IMPLEMENTED

Batch 1:
  status: strongest justified first repair
  evidence: confirmed post-cinematic false-positive

Batch 2:
  status: candidate hardening, requires harness/runtime validation

Batch 3:
  status: separate confirmed lifecycle contract repair

Production changes: Batch 1 implemented; Batch 2/3 unchanged
ASI build: PASS
Runtime validation: previous build exposed premature release at a neutral
first sample; corrected two-stage build is not runtime-validated

## Batch 1 implementation result

Phase 0 passed from the current source:

- `TraceCinematicExit()` captures the native EXIT target before arming the new exclusion.
- The existing validated gameplay writer is reached in both HorPlus and AspectRecalculation paths.
- HorPlus reaches the writer observation before its existing early return.
- AspectRecalculation retains its existing `CinematicExiting` recovery/replay path.
- `kRecoveryEpsilon` is the existing native recovery convergence tolerance.

Implemented only Batch 1:

- Added `dialogue::PostCinematicRecoveryExclusion` as the production state/predicate helper.
- Arm on Cinematic EXIT; reset on Cinematic ENTER, source replacement, invalid sample and runtime reset.
- Suppress Dialogue boundary processing while the known recovery is active.
- Hold a `WaitingForRecovery` stage when the first same-source sample is already
  at or near the captured target; enter `Recovering` only after a later finite
  sample moves away by more than the existing epsilon.
- Release only after that recovery stage returns to the target within the same
  existing epsilon.
- Left normal Candidate/Active/Exiting rules, HorPlus math, cinematic transforms, AspectRecalculation replay and F9/F10 behavior unchanged.

Validation:

- `test.cmd`: PASS, including the neutral-first-sample, recovery-start and
  convergence cases in `post_cinematic_exclusion=PASS` and all existing harnesses.
- `build.cmd`: PASS; only existing external `Zydis.h` C4201 warnings were emitted.
- Previous runtime evidence found the premature neutral-sample release; the
  corrected two-stage build has not been launched in-game.

Batch 2 and Batch 3 remain separate and were not implemented.
```
