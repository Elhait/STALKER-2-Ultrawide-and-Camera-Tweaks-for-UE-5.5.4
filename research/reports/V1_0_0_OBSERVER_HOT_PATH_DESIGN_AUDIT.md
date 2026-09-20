# v1.0.0 Observer Hot-Path Design Audit

## Scope

Bounded static review of the Gameplay-disabled observer path after the
template-synchronization safety fix. No production source, build, game run or
runtime probe was changed or executed in this audit.

## Current path

```text
camera-writer hit
  -> ReplayManualTransition
  -> Gameplay=false branch
  -> SafeRead(RSI + aspect offset)
  -> VirtualQuery + memcpy
  -> atomic g_lastObservedAspect store
```

The observer is read-only, but it performs `VirtualQuery` and a memory copy on
a repeatedly-called camera-writer callback. This remains a plausible hot-path
cost candidate, not proven stutter causality.

## Option review

| Option | Result | Reason |
|---|---|---|
| A — keep `SafeRead` per hit | Baseline only | Semantically conservative, but retains the expensive per-hit query. |
| B — derive aspect from hook context / `XMM0` | Rejected | The validated instruction is `MOVSS [RBX+0x30], XMM0`; `XMM0` carries the camera FOV value, not the aspect value read by the observer. This is not a proven equivalent replacement. |
| C — reduce observation frequency | Not justified | Changes cache timing and observation semantics without evidence that a bounded sampling point is sufficient. |
| D — eliminate continuous observer when it is installed only for `Cinematics=Auto` | Preferred | Auto cinematic aspect resolution uses the current client viewport aspect. `g_lastObservedAspect` is not the Auto source and no required consumer was found that needs continuous camera-writer observation. |

## Preferred bounded design

When `Gameplay.Enabled=false`, do not install the shared gameplay camera-writer
hook solely because `Cinematics=Auto` is enabled. Leave the cinematic aspect
store and cinematic FOV hooks unchanged. Auto aspect continues to resolve from
the platform viewport path at the existing cinematic lifecycle boundary.

This removes the repeated `VirtualQuery`/`memcpy` observer work without
changing the established cinematic policy or gameplay correction behavior.

## Remaining uncertainty

Static analysis cannot prove that the game's native aspect-setting UI uses the
camera-writer hook for an unrelated side effect. A targeted runtime validation
after implementation would therefore be required to confirm that removing the
observer does not alter native aspect-setting behavior in the
`Gameplay=false, Cinematics=Auto` configuration.

## Decision

```text
Preferred design: D
Production change justified: YES, bounded
Implementation status: COMPLETE
Runtime validation status: PASS — USER-RUN TARGETED CHECK
Helper.hpp review: separate architecture/code-quality work
```

## Implementation result

Design D was approved and implemented in `src/plugin/runtime.cpp`. The shared
camera-writer hook is now installed only when `Gameplay.Enabled=true`; the
previous read-only observer is no longer installed for
`Gameplay.Enabled=false + Cinematics=Auto`.

Static validation confirms that the cinematic aspect-store hook, cinematic FOV
hooks, Auto viewport-aspect source and Gameplay-enabled path remain reachable.
The worker lifecycle, feature-status and config-persistence harnesses pass,
the production ASI builds successfully, and `git diff --check` passes.

```text
Source/static validation       PASS
Harness validation              PASS
Production build                PASS
Runtime validation              NOT YET PERFORMED
Stutter causality               NOT CLAIMED
```

## Targeted runtime result

User-run log identity:

```text
modSha256  = D989EA42D98F3C7044AA0CB88388B18A248248715410C6E9D4AC1602623FDEBB
gameSha256 = E7B481A97C02D80581FAB0BECE940214A88EBE30211088A00129845A039F9293
```

Observed:

```text
Gameplay.Enabled=0
Gameplay aspect fix disabled by configuration; camera-writer observer bypassed.
Initialization summary: Gameplay=DISABLED CinematicAspect=AVAILABLE CinematicFOV=AVAILABLE Dialogue=AVAILABLE.
Auto cinematic aspect resolved from client/display viewport: aspect=3.55556.
Global cinematic ENTER: ... Gameplay replay suppressed.
Cinematic aspect store: ... aspect=3.55556 policy=Auto source=runtime-camera.
Global cinematic EXIT: ... Gameplay replay suppressed until native recovery.
```

The native AspectRatio menu was available and responsive. Cinematic Auto
resolved and applied the viewport aspect. Gameplay replay/correction was not
applied after EXIT because Gameplay was explicitly disabled; that is the
expected configuration contract, not a regression.

```text
Observer installation bypassed       PASS
Native AspectRatio interaction       PASS
Cinematics Auto                     PASS
Gameplay-disabled replay suppression PASS / EXPECTED
Stutter causality                    NOT CLAIMED
```

The bounded observer batch is complete. Full Batch 4 regression remains a
separate gate.
