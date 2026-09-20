# AspectRecalculation Custom-Windowed Limitation

## Scope

Read-only static follow-up to the final runtime regression. No production or
test changes were made, and the game was not launched for this investigation.

## Runtime observation

The regression session exercised a physical 32:9 display with a windowed custom
3:1 resolution. Gameplay HorPlus used the runtime aspect of approximately
3.0 correctly. After F11 switched from `HorPlus` to `AspectRecalculation`, the
native transition completed and the window returned to the physical/native
32:9-sized state rather than retaining the custom 3:1 window size.

The rest of the tested functionality remained correct: arbitrary-aspect
HorPlus math, ADS/binocular framing, cinematic paths, Dialogue lifecycle and
recovery, mode switching, and camera recreation.

## Source-level dataflow

### F11 mode transition

`src/plugin/runtime.cpp:SelectGameplayMode()` handles the F11-selected mode
change. For `HorPlus -> AspectRecalculation` it clears the pending HorPlus
transition, puts the replay state into `WaitingForAutomaticUpdate`, invalidates
the restoration state, clears the retained gameplay camera context and
invalidates the GameplayBaseline store (`runtime.cpp:3738-3748`). It does not
choose or write a window resolution.

For `AspectRecalculation -> HorPlus`, the transition is deferred until a
Gameplay camera-writer callback provides a readable runtime camera state. The
consumer then reads the retained restoration aspect and writes it back through
the validated camera object (`runtime.cpp:1172-1248`).

### Native AspectRecalculation path

The AspectRecalculation path observes the game's camera-writer state and uses
the native transition lifecycle. When the native constrained state is seen,
the production path can write the native aspect/flags pair through
`ApplyGameplayAspectFixAtomic()` (`runtime.cpp:1630-1669`). The write is to the
validated camera object fields at the known aspect and flags offsets; it is not
a display-mode or window-size operation.

The restoration store retains the last applicable runtime camera aspect and
source. It can be invalidated on mode change and updated only from valid
Gameplay writer observations (`runtime.cpp:597-610`, `runtime.cpp:1548-1567`,
`runtime.cpp:3360-3367`). Its responsibility is camera aspect restoration, not
window management.

### Auto and runtime aspect resolution

`ResolveAutoAspect()` obtains the current client viewport aspect through
`platform::win32::ReadClientViewportAspect()` and falls back to the native
16:9 aspect when unavailable (`runtime.cpp:1133-1153`). The viewport helper
only reads `GetClientRect`, with a display-settings read as fallback
(`src/platform/win32/viewport.cpp:18-39`).

The cinematic Auto path likewise resolves an aspect from the current
client/display viewport and writes the resolved value to the cinematic camera
object (`runtime.cpp:1268-1300`). This is separate from the Gameplay
AspectRecalculation transition.

## Checks against alternative causes

### Not an arbitrary-aspect HorPlus limitation

The gameplay HorPlus transform accepts a valid runtime aspect and applies the
projection conversion without a 21:9/32:9 whitelist. The aspect policy only
determines eligibility relative to the 16:9 native aspect; the transform itself
uses the supplied aspect (`src/gameplay/horplus_gameplay.cpp:10-29`,
`src/gameplay/aspect_policy.cpp:7-15`, `src/camera/horplus.cpp:7-12`). A custom
3:1 runtime aspect is therefore not rejected by the HorPlus math.

### Not loss of stored aspect by GameplayAspectRestorationState

The restoration state stores the observed aspect and source, supports explicit
invalidation, and restores the retained valid ultrawide aspect when the native
camera state permits it (`src/camera/gameplay_aspect_restoration.cpp:28-79`,
`runtime.cpp:1193-1248`). This is consistent with the observed correct
AspectRecalculation behavior before the final native Auto state.

### No mod-owned window resize

The source contains viewport/window discovery and reads only. The repository
search found no production use of window-size mutation APIs such as
`SetWindowPos`, `SetWindowLong`, `ChangeDisplaySettings` or
`SetDisplayConfig` in the relevant path. The mod writes camera aspect/flags and
reads the viewport; it does not directly restore a 32:9 window resolution.

## Confirmed facts versus inference

Confirmed statically:

- F11 selects the native AspectRecalculation transition path.
- The path uses native camera aspect/flags transitions and a temporary native
  state rather than an arbitrary-aspect mode supplied by the mod.
- The mod retains/restores camera aspect through
  `GameplayAspectRestorationState`.
- HorPlus accepts arbitrary valid runtime aspect values.
- The mod has no direct window-resolution write in this path.

Observed at runtime:

- A custom windowed 3:1 resolution on a physical 32:9 display returned to the
  full 32:9-sized window after the native AspectRecalculation transition.

Remaining causal inference:

- The exact internal game-owned step that changes the window size is not
  exposed by this source. The strongest explanation is that returning the
  native camera state to `Auto` lets the game select its physical/display
  native 32:9 state, and the game then restores the corresponding window
  dimensions. This is consistent with the runtime observation and with the
  absence of any mod-owned window write, but the window-manager side of that
  causal chain is not statically proven here.

## Classification

```yaml
classification: CONFIRMED_NATIVE_ASPECT_TRANSITION_LIMITATION
arbitrary_aspect_horplus: supported_by_source
gameplay_restoration_state: not_the_cause
mod_owned_window_resize: not_found
game_owned_auto_window_resize: runtime_observed_source_unproven
production_change: none
```

The limitation is specifically the interaction between a custom windowed
aspect that has no corresponding native game aspect mode and the game's native
AspectRecalculation/Auto transition. It is not evidence that arbitrary
runtime-aspect HorPlus or cinematic math is generally unsupported.

## Recommended v1 documentation wording

> HorPlus and aspect-aware camera/cinematic calculations support arbitrary
> runtime aspect ratios, including custom windowed ratios. AspectRecalculation
> relies on the game's native aspect-mode transition. If a windowed custom
> resolution uses an aspect ratio that is not represented by a native game
> aspect mode, returning that transition to Auto may restore the game's native
> display aspect and correspondingly change the window size. This limitation
> applies to the native AspectRecalculation transition; it does not mean that
> arbitrary-aspect HorPlus math is unsupported.

## Stop condition

No production code, tests, hooks, architecture, runtime configuration or
release files were changed. No game launch, separate runtime plan, optimization
or Git operation was performed.
