# Dialogue vs Cinematic Ownership Detection Audit

Date: 2026-09-18  
Scope: read-only source comparison; no production changes, build or runtime launch.

## Evidence status

- **CONFIRMED:** the production source has separate Cinematic ENTER and EXIT resolver/hook paths.
- **CONFIRMED:** the Dialogue hook is installed at the generic FOV-blend boundary already identified by the Steam 2.0.5 runtime trace.
- **STATICALLY ESTABLISHED:** Cinematic resolver validation checks unique matches, decoded operands, callsite shape, surrounding virtual-call topology and a shared executable consumer target.
- **CONFIRMED by supplied runtime evidence:** `APC::IsInStaticDialog()` produces `false → true → false` for the tested static-dialogue path.
- **NOT ESTABLISHED:** standalone ASI access to `APC::IsInStaticDialog()`.
- **NOT ESTABLISHED:** that the Cinematic callsites are a direct exported boolean ownership API. The source establishes a dedicated lifecycle-associated callsite topology; the exact engine semantic is inferred from that topology plus runtime behavior.

## 1. Current Cinematic ENTER path

### Resolver and hook

`src/hooks/signatures/signature_definitions.hpp` defines a dedicated `CinematicEnter` signature. It identifies a scalar FOV load followed by the cinematic consumer call chain. The resolver is `ResolveCinematicFovCallsites()` in `src/plugin/runtime.cpp`.

The ENTER validation path:

1. `FindAll(g_executable, CinematicEnter)` must return exactly one match.
2. `ValidateEnterBoundary()` decodes the instruction and requires the expected FOV/register shape.
3. The callsite must be a valid relative call.
4. The surrounding bytes must contain `EnterVcallPair`.
5. The resolved ENTER call target must be executable.
6. ENTER and EXIT must resolve to the same executable consumer target.

The hook is installed at the validated ENTER callsite by `InstallCinematicFovComponent()`.

### Callback and coordinator transition

`TraceCinematicEnter()`:

- cancels deferred/recovery research state;
- resets atomic EXIT handoff state;
- clears the previous cinematic transformed-FOV snapshot;
- applies the existing cinematic FOV transform to the incoming authored FOV when policy permits;
- writes the transformed value back to `context.xmm0`;
- stores `CoordinatorState::CinematicActive`.

The ownership transition is therefore explicit in the mod state machine:

```text
validated Cinematic ENTER callsite
→ TraceCinematicEnter()
→ cinematic FOV transform
→ CoordinatorState::CinematicActive
```

## 2. Current Cinematic EXIT path

### Resolver and hook

The resolver first searches `CinematicExit`. If no legacy match exists, it searches the explicitly supported `CinematicExitIndexed` topology. Exactly one EXIT match is required.

`ValidateExitBoundary()` or `ValidateIndexedExitBoundary()` requires:

- a decoded `MOVSS` FOV source with the expected base/index/displacement shape;
- a valid relative call;
- the surrounding `ExitVcallPair`;
- the same executable consumer target already validated for ENTER.

This is a separate source/callsite contract from the DialogueBoundary signature.

### Callback and coordinator transition

`TraceCinematicExit()`:

- records the native EXIT target FOV;
- arms post-EXIT observation state;
- clears cinematic FOV state;
- calls `gameplay::ResolveCinematicExitTransition()`;
- either enters `CinematicExiting` and arms the existing AspectRecalculation handoff, or returns directly to `Gameplay` when recovery is not owned;
- resets Dialogue runtime state on the non-owned recovery branch.

The production transition is:

```text
validated Cinematic EXIT callsite
→ TraceCinematicExit()
→ ResolveCinematicExitTransition()
→ CinematicExiting + existing handoff
   or Gameplay + no handoff
```

The native FOV recovery after EXIT is intentionally separate from EXIT detection. `TraceCinematicExit()` observes the EXIT boundary and records the target; it does not claim to create the engine's subsequent native interpolation trajectory.

## 3. What actually proves Cinematic ownership

### Established

Cinematic ownership is represented by a dedicated, independently resolved pair of callsites with:

- separate ENTER and EXIT signatures;
- unique-match fail-closed resolution;
- decoded FOV operands;
- surrounding ENTER/EXIT virtual-call topology;
- shared executable consumer target validation;
- explicit coordinator transitions in the callback;
- separate lifecycle/recovery handling.

This is materially stronger than classifying a generic FOV value by direction. It gives the mod a positive lifecycle boundary associated with the cinematic callsite topology.

### Not established

The source alone does not prove that the hooked callsites are a direct `IsCinematicActive`/`OnCinematicStarted` API. The exact native engine meaning of the surrounding consumer chain is not named in the source. The safe statement is:

> The mod owns a dedicated ENTER/EXIT callsite pair whose validated topology and runtime behavior identify the cinematic lifecycle boundary; the FOV transformation is performed after that boundary is observed.

## 4. Current Dialogue path

`DialogueBoundary` is a signature around the generic native FOV-blend function. Its validated hook is installed at `matches.front() + kDialogueBoundaryHookOffset`, after the virtual call sequence that reaches the FOV blend consumer.

`TraceDialogueBoundary()` receives a live FOV value from `context.xmm6` and then applies this classifier:

```text
valid incoming FOV
+ CoordinatorState::Gameplay
+ policy != Native
+ descending FOV
→ Candidate / Active
```

Exit is similarly inferred from an ascending FOV, and recovery is inferred when the value returns near the provisional baseline.

The callback has no established positive Dialogue ownership signal. `[RSI+0x2C]` is the native FOV target/end value, not a Dialogue flag. Supplied UE4SS correlation proves that `APC::IsInStaticDialog()` has the desired game-owned state for tested static dialogue, while the standalone ASI path currently cannot read it directly.

The existing `ReplayManualTransitionOriginal()` source/FOV invalidation is a safeguard that clears stale Dialogue state after a camera-source change or material FOV jump. It is not a positive Dialogue detector.

## 5. Side-by-side architecture

| Property | Cinematic | Dialogue |
|---|---|---|
| Hook native meaning | Dedicated ENTER/EXIT callsite topology | Generic FOV-blend boundary |
| Resolver | Separate ENTER and EXIT signatures | One DialogueBoundary signature |
| Structural validation | Decode, operands, callsite, surrounding vcall pair, shared consumer target | Unique match and expected hook bytes |
| Positive ownership signal | Dedicated lifecycle-associated boundary | None established in standalone ASI |
| ENTER detection | Hook callback at validated ENTER callsite | Candidate from FOV baseline/descent |
| EXIT detection | Hook callback at validated EXIT callsite | Ascending FOV and baseline recovery |
| Depends on FOV direction | No for ownership; FOV is transformed at the boundary | Yes, directly |
| Known false-positive | None established in supplied evidence | Ground-truth-confirmed outside real Dialogue |
| Game-owned state | Native lifecycle-associated callsite behavior | `APC::IsInStaticDialog()` confirmed through UE4SS for tested static dialogue |
| Standalone access | Existing production resolver/hook | Direct state access not established |
| Coordinator integration | Explicit `CinematicActive`, `CinematicExiting`, `Gameplay` transitions | Uses `coordinator == Gameplay` only as eligibility |

## 6. Transferable design principle

The transferable principle is not to copy Cinematic code or reuse its hook. It is the ownership separation:

```text
native lifecycle boundary
→ establish feature ownership/state

generic camera/FOV boundary
→ transform values only while ownership is already established
```

For Dialogue, the desired architecture would be:

```text
native Dialogue ENTER
→ Dialogue ownership = active
→ snapshot selected Dialogue policy

generic FOV-blend boundary
→ apply the active policy only while Dialogue owns the lifecycle

native Dialogue EXIT
→ clear ownership and active policy
```

This is a design target, not an established implementation path. No production Dialogue repair is selected by this audit.

## 7. Next Dialogue search target

The next search target should be the class of native seams that creates or clears the state later exposed by `APC::IsInStaticDialog()`, in this order:

1. native lifecycle function or state setter surrounding the `IsInStaticDialog()` state transition;
2. Dialogue manager dispatch or begin/end event;
3. explicit static-dialogue enter/exit call path;
4. only then, a camera-mode transition that is proven to be Dialogue-owned.

Do not search for another FOV target offset, another hard-coded target such as `70`, or another generic descent pattern. The current evidence specifically rejects those as ownership contracts.

## Final classification

```yaml
CONFIRMED:
  - Dedicated production Cinematic ENTER/EXIT hook pair exists.
  - Cinematic callbacks explicitly own coordinator transitions.
  - Dialogue hook is a generic FOV-blend observation/transformation boundary.
  - Dialogue FOV heuristic has ground-truth-confirmed false-positives.
  - APC::IsInStaticDialog() is a valid tested static-dialogue oracle via UE4SS.

STATICALLY ESTABLISHED:
  - Cinematic resolver uniqueness and structural validation requirements.
  - Shared ENTER/EXIT executable consumer target requirement.
  - Dialogue phase transitions are derived from FOV direction/baseline.
  - Dialogue invalidation is a safeguard, not ownership detection.

NOT ESTABLISHED:
  - Direct standalone ASI access to IsInStaticDialog().
  - A production-positive Dialogue lifecycle seam.
  - That the Cinematic callsite pair is a direct named engine ownership API.

PRODUCTION CHANGES: NONE
ASI BUILD: NONE
RUNTIME VALIDATION: NONE
```
