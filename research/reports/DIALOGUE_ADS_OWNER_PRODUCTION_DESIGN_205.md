# Dialogue ADS-owner integration design — Steam 2.0.5

## Scope

This is a static production-design document based on the Wideboy v10 runtime
diagnostic. It does not change the classifier, build an ASI, or authorize
another runtime run.

## Evidence

- Both Wideboy signatures are unique in the identity-matching Steam 2.0.5
  image and share one native function.
- The diagnostic log contains 371 samples grouped into 36 contiguous runs:
  18 `IN` runs and 18 `OUT` runs, strictly alternating.
- Every `IN` run ramps `RAX+0x4C` from approximately `0` to `1` while
  `RAX+0x50` ramps from `1` to `0`.
- Every `OUT` run reverses that complement: `RAX+0x4C` from `1` to `0` and
  `RAX+0x50` from `0` to `1`.
- The same object identity remains stable within each transition.
- The supplied log contains cinematic EXIT/recovery and `AtomicReplayApplied`
  records without any Wideboy ADS records during that recovery.
- Runtime coverage for every weapon, optic, and non-ADS camera transition is
  not established.

## Required ownership model

Wideboy callbacks are transition samples, not physical input events. The
production state must therefore aggregate them:

```text
Inactive
  first validated IN sample  -> Entering
Entering
  IN samples                 -> Entering
  IN completion              -> Active
  first validated OUT sample -> Exiting
Active
  IN samples                 -> Active
  first validated OUT sample -> Exiting
Exiting
  OUT samples                -> Exiting
  OUT completion             -> Inactive
```

Dialogue exclusion applies to `Entering`, `Active`, and `Exiting`. It is
released only after a validated OUT completion, not on the first OUT sample.
This prevents the first ADS-release FOV samples from being mistaken for a
new Dialogue transition.

## Completion predicate

Use the already observed complementary pair, not a timer or a hard-coded FOV:

```text
IN complete:
    finite values
    same validated source/object identity
    rax4c >= 1 - blendEpsilon
    rax50 <= blendEpsilon

OUT complete:
    finite values
    same validated source/object identity
    rax4c <= blendEpsilon
    rax50 >= 1 - blendEpsilon
```

The exact epsilon must be selected from the existing float/state comparison
conventions and covered by harness tests. It must not be chosen from a user
FOV or a `70` dialogue target.

## Ownership and safety rules

1. The first validated IN sample sets the state to `Entering` immediately.
2. The first validated OUT sample sets the state to `Exiting` immediately.
3. A direction reversal before completion is handled as a new validated
   transition; the state never returns to `Inactive` merely because one
   sample is near a boundary.
4. Source/object identity changes invalidate the transition and fail closed:
   the ADS owner is cleared only after the new source has produced a complete
   validated transition. No Dialogue transform is applied while identity is
   ambiguous.
5. Non-finite fields, unreadable fields, out-of-range blend values, or unknown
   direction leave the native FOV path untouched. While the state is already
   `Entering`, `Active`, or `Exiting`, the conservative ADS exclusion is
   preserved; while `Inactive`, no ADS ownership is asserted. The first
   observed IN sample can be `0/0`, so a complementary sum of one is not a
   validity requirement.
6. The observer remains read-only. It must not rewrite XMM registers, FOV,
   aspect, flags, replay state, or coordinator state.
7. The ADS observer is independent of `Gameplay.Mode`, Cinematics policy, and
   the frozen `AspectRecalculation` operation.

## Dialogue integration boundary

The existing FOV descent heuristic must not be used as positive Dialogue
ownership. The intended order is:

```text
invalid input                         -> native pass-through
post-cinematic recovery exclusion     -> native pass-through
coordinator != Gameplay               -> native pass-through
ADS state Entering/Active/Exiting     -> native pass-through
positive native Dialogue fingerprint  -> existing Dialogue transform
ambiguous                             -> native pass-through
```

The ADS state is a negative exclusion only. It does not by itself establish a
Dialogue event, and it does not change the existing Dialogue FOV math.

The current target/FOV and `70` fingerprint remain separate evidence. This
document does not authorize replacing the classifier with `target == 70`.

## Hotkey and lifecycle interaction

- Dialogue policy snapshot semantics remain a separate repair task.
- Changing Dialogue policy must not reset ADS lifecycle state.
- Changing Gameplay mode must not reset ADS lifecycle state.
- Runtime shutdown resets the ADS state and releases both diagnostic/production
  hooks through the existing resource owner.
- A failed ADS resolver or ambiguous runtime sample fails closed: native
  Dialogue behavior remains unchanged and no ADS ownership is asserted.

## Static/unit coverage required before implementation

The harness should cover:

- first IN sample enters `Entering` immediately;
- IN completion reaches `Active`;
- first OUT sample reaches `Exiting`;
- OUT completion reaches `Inactive`;
- no early release on the first OUT sample;
- interrupted/reversed transitions do not create `Inactive` gaps;
- source/object change invalidates ownership safely;
- non-finite and out-of-range pairs fail closed;
- ADS exclusion covers all three non-inactive states;
- ordinary FOV descent without ADS remains eligible only for the separate
  Dialogue decision and is not classified as ADS.

## Status

```yaml
Wideboy ADS native transition:      RUNTIME CONFIRMED in tested scenario
Logical ADS aggregation design:     STATICALLY DEFINED
All weapon/optic coverage:           NOT ESTABLISHED
Dialogue positive discriminator:     NOT ESTABLISHED
Production implementation:           NOT STARTED
Further runtime test:                NOT REQUIRED before static implementation
```

## Static implementation status

The candidate batch is now implemented behind
`WIDEBOY_ADS_OWNER_INTEGRATION`:

- `dialogue::AdsLifecycle` provides `Inactive`, `Entering`, `Active`, and
  `Exiting` states.
- The first validated IN/OUT sample starts ownership immediately.
- Completion uses the paired blend endpoints, not a timer or FOV target.
- Invalid samples preserve an already-active exclusion and do not claim
  ownership while inactive.
- Dialogue receives a negative exclusion for all non-inactive ADS states.
- Resolver, hash and operand checks remain fail-closed and read-only.

Validation:

- `test.cmd`: PASS, including `ads_lifecycle=PASS` and the existing dialogue
  exclusion harness.
- `build-wideboy-ads-diagnostic.cmd`: PASS; only the known external Zydis
  C4201 warnings were emitted.
- Stable `STALKER2CameraTweaks.asi`: not replaced.
- Runtime: not yet run with the candidate integration.

The positive Dialogue fingerprint, removal of the generic descent heuristic,
and promotion to the stable ASI remain separate tasks.
