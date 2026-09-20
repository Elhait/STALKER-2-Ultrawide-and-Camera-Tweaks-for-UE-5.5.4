# HorPlus Zoom Transition — Static Integration Design

## Scope

Gameplay HorPlus only. Dialogue and Cinematics are intentionally outside this batch.

## Established source behavior

The validated gameplay hook enters `ReplayManualTransition()` with the current native camera FOV in `context.xmm0.f32[0]`. In `Gameplay.Mode=HorPlus`, the function calls `ApplyHorPlusGameplay(context)` and returns.

`ApplyHorPlusGameplay()`:

- accepts only `Gameplay` or `CinematicActive` coordinator states;
- reads the current runtime aspect and flags from the validated camera source;
- calls the existing `gameplay::TryTransformHorPlus()` with the current native FOV;
- writes the transformed value back to the gameplay writer's `XMM0`;
- does not store the transformed value as a new native baseline.

Therefore every native gameplay-writer sample in a zoom trajectory is already transformed sample-by-sample. The writer path is the correct FOV application point.

## Wideboy callback finding

The existing Wideboy signatures are currently named `WideboyAdsIn`/`WideboyAdsOut` and hook the post-load compare boundary. At that boundary:

- `RAX+0x4C` or `RAX+0x50` contains the transition weight;
- `RSI+0x138` or `RSI+0x13C` contains the paired state value;
- `XMM0` contains the loaded transition weight;
- `XMM6` contains the paired loaded value;
- no native camera FOV is present in the callback contract.

The runtime logs confirm that the pair is used by ADS and controller camera pull, but the callback itself is a zoom-transition signal, not a FOV writer.

## Design decision

Do not call `TryTransformHorPlus()` directly from the Wideboy callback. That would transform a blend weight rather than a camera FOV and could produce a second transform if the same sample later reaches the validated gameplay writer.

The correct integration is:

```text
ZOOM_IN / ZOOM_OUT callback
    = native gameplay zoom transition signal

validated gameplay writer
    = native FOV source and sole HorPlus application point
```

The callbacks may be renamed and retained for neutral diagnostics or future correlation, but they must not become a second gameplay FOV writer.

## Invariants

1. `ZOOM_IN` and `ZOOM_OUT` never transform `XMM0` directly.
2. The validated gameplay writer applies at most one HorPlus transform to each native FOV sample.
3. Native FOV is always the input; transformed output is never reused as the next native input.
4. Dialogue logic is unchanged.
5. Cinematic logic is unchanged.
6. AspectRecalculation is unchanged.
7. If the gameplay writer cannot read a valid aspect/flags state, it leaves the native sample untouched.

## Implementation consequence

No new production FOV application hook is required for `ZOOM_IN/ZOOM_OUT`. The minimal safe source change is terminology neutralization of the existing diagnostic/integration scaffolding, followed by harness coverage proving that a sequence of native zoom samples is transformed exactly once through `TryTransformHorPlus()`.

If a future optimization wants to avoid observing every generic writer hit, it may use `ZOOM_IN/ZOOM_OUT` as a scheduling or telemetry signal, but it must not bypass the validated writer source without a separate one-to-one ordering proof.

## Status

- Native zoom pair: runtime-confirmed for ADS and controller pull.
- Sample-by-sample HorPlus path: statically confirmed at validated gameplay writer.
- Direct Wideboy-to-FOV transformation: rejected as unsafe and semantically incorrect.
- Dialogue/Cinematics changes: none.
- Production source implementation after this design: not yet applied.
