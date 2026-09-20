# Weapon Viewmodel FOV — O Profile Behavior

## Result

With `N` active and `F_99_P` disabled, the O profile changed viewmodel framing:

```text
O0  CORRECT baseline framing after EXIT
O20 OTHER — weapon farther after EXIT; ADS restored framing
O40 OTHER — weapon farther still; ADS restored framing
```

The full baseline `N + F_99_P + O0` installation was restored after testing.

## Confirmed behavior

```text
O0  → baseline
O20 → farther after cinematic EXIT
O40 → farther still after cinematic EXIT
```

This is behavioral proof that the O profile affects viewmodel FOV magnitude or
activates a magnitude input in the WVF implementation contained in N, without
requiring F_99_P. The monotonic O20/O40 difference strongly supports profile
selection behavior, but does not by itself prove the exact selector code.

ADS consistently returned O20 and O40 to the expected weapon position. This
confirms an ADS-associated refresh/re-evaluation response capable of repairing
or replacing the visible viewmodel state after cinematic EXIT. It does not yet
prove that ADS invokes the WVF implementation itself or uses the same consumer
as the O profile.

```text
O profile → WVF behavior/magnitude:    CONFIRMED behaviorally
O20/O40 monotonic framing difference:  CONFIRMED behaviorally
ADS refresh/re-evaluation response:    CONFIRMED behaviorally
ADS exact consumer/owner:              UNKNOWN
O marker selector semantics:           STRONG HYPOTHESIS, NOT PROVEN
O → FOV_* exact mapping:               NOT PROVEN
N runtime consumer/refresh owner:      UNKNOWN
```

## Scope and safety

- Reference files only; no production source or ASI changes.
- `F_99_P` remained disabled during O20/O40 testing.
- No runtime values were written.
- Complete reference installation was restored.

## Next gate

A future research-only pass may inspect cooked Blueprint serialization around
`WVF.WVF_C`, `.wvf`, `all-` and `DeferredActorSpawnFromClass`, or design a
targeted probe that identifies the state/consumer shared by cinematic EXIT and
ADS. Production integration still requires an identified consumer and visible
proof.
