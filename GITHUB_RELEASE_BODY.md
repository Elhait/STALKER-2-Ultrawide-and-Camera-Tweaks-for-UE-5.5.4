### STALKER 2 Ultrawide and Camera Tweaks for UE 5.5.4 — v1.0.0

Unified gameplay and cinematic aspect/FOV fix for S.T.A.L.K.E.R. 2, runtime-tested on Steam build 2.0.6 with the UE 5.5.4 target.

**Highlights**

- Gameplay correction for 16:9, 21:9 and 32:9 displays.
- `AspectRecalculation` and `HorPlus` gameplay modes.
- `HorPlus` preserves the game's original gameplay FOV changes and adapts them to the current runtime aspect, including ADS and binocular trajectories.
- Atomic gameplay apply and post-cinematic recovery handoff, preserving the game's native recovery timing.
- Automatic re-arm after camera rebuilds, including death/load.
- Cinematic aspect policies: `Auto`, `Native`, `16:9`, `21:9` and `32:9`.
- Cinematic FOV modes: `GameplayHorPlus` and `NativeHorPlus`.
- `GameplayHorPlus` follows the selected Gameplay FOV while preserving authored cinematic variation.
- FOV-aware dialogue zoom modes: `Native`, `Adaptive`, `Reduced` and `Disabled`.
- Live runtime hotkeys: F9 Gameplay, F10 cinematic aspect, F11 cinematic FOV and F12 dialogue.
- Read-only diagnostic telemetry behind `[Diagnostics] Enabled=false`; disabled by default and intended only for research sessions.
- Guarded signature resolution, instruction validation and safe refusal on mismatch.
- SHA-256 logging for the loaded ASI and game executable.

**Configuration**

```ini
[Gameplay]
; Enables the gameplay aspect-ratio correction.
;
; true  - enable the selected gameplay correction mode.
; false - leave the game's original gameplay camera/aspect behavior untouched.
Enabled=true

; Gameplay correction mode: HorPlus or AspectRecalculation.
;
; HorPlus - default and recommended mode. Preserves the game's native Gameplay FOV changes and adapts them in real time to the current runtime aspect ratio. Supports arbitrary/custom aspect ratios and native FOV changes such as ADS and binocular zoom.
;
; AspectRecalculation - alternative mode that uses the game's native aspect/projection transition to correct Gameplay framing while preserving the selected Gameplay FOV.
;
; On custom windowed aspect ratios that are not represented by a native game aspect mode, AspectRecalculation may return the window to the display's native aspect/size when the game restores Auto. This limitation does not apply to HorPlus.
;
; HorPlus examples when idle:
; Gameplay FOV 90°:  16:9 -> 90°; 21:9 -> approximately 106.69°; 32:9 -> approximately 126.87°.
; Gameplay FOV 100°: 16:9 -> 100°; 21:9 -> approximately 116.04°; 32:9 -> approximately 134.48°.
; Gameplay FOV 110°: 16:9 -> 110°; 21:9 -> approximately 124.95°; 32:9 -> approximately 141.41°.
Mode=HorPlus


[Cinematics]
; Controls how cinematics are framed independently from your physical display.
;
; Auto   - use the current runtime viewport aspect ratio. Supports arbitrary valid aspect ratios and is recommended for most users.
;
; Native - leave the game's original cinematic aspect and FOV behavior untouched without cinematic FOV/aspect correction from the mod. On ultrawide displays, Native may look similar or identical to 16:9 because this is how the game currently presents its cinematics without intervention from the mod.
;
; 16:9   - force 16:9 cinematic framing.
;
; 21:9   - force 21:9 cinematic framing, regardless of the physical display.
;
; 32:9   - force 32:9 cinematic framing, regardless of the physical display.
;
; Forced modes can also be used on displays with a different aspect ratio. For example, 32:9 on a 16:9 display produces a wider cinematic presentation with black bars above and below.
; When using Gameplay HorPlus, forcing a cinematic aspect that differs from the actual Gameplay aspect intentionally introduces an aspect/FOV transition when entering or leaving Cinematics.
; For seamless HorPlus transitions, use Auto or force the same aspect ratio that is actually used during Gameplay.
AspectRatio=Auto

; Cinematic FOV mode: GameplayHorPlus or NativeHorPlus.
;
; GameplayHorPlus - default and recommended FOV mode for Gameplay HorPlus. Uses the current native Gameplay FOV as the cinematic baseline, so changing the FOV setting in the game also changes the resulting cinematic FOV while preserving the cinematic's authored FOV variation.
;                  Examples for authored cinematic FOV 90° with Gameplay FOV 90°: 16:9 -> 90°; 21:9 -> approximately 106.69°; 32:9 -> approximately 126.87°.
;                  With Gameplay FOV 112.6°, the same cinematic becomes approximately 112.6° at 16:9, 118.51° at 21:9 and 143.13° at 32:9.
;                  Falls back safely when the required Gameplay context is unavailable.
;
; NativeHorPlus   - alternative mode that applies Hor+ to the native/authored cinematic FOV independently of the player's Gameplay FOV setting.
;                  For authored cinematic FOV 90°: 16:9 -> 90°; 21:9 -> approximately 106.69°; 32:9 -> approximately 126.87° regardless of Gameplay FOV.
FovMode=GameplayHorPlus


[Dialogue]
; Controls the camera zoom applied during dialogue.
;
; Native   - use the game's original dialogue zoom behavior.
;            Example: 90° gameplay FOV -> 70° during dialogue.
;            Example: 110° gameplay FOV -> 70° during dialogue.
;
; Adaptive - preserve the game's original optical zoom strength relative
;            to the current gameplay FOV.
;            Example: 90° gameplay FOV -> 70° during dialogue.
;            Example: 110° gameplay FOV -> approximately 90° during dialogue.
;
; Reduced  - apply half of the Adaptive optical zoom strength.
;            Example: 90° gameplay FOV -> approximately 80° during dialogue.
;            Example: 110° gameplay FOV -> approximately 100° during dialogue.
;
; Disabled - disable additional dialogue zoom and preserve normal gameplay framing.
;            Example: 90° gameplay FOV -> 90° during dialogue.
;            Example: 110° gameplay FOV -> 110° during dialogue.
Zoom=Adaptive


[Diagnostics]
; Enables optional read-only runtime telemetry. It is not recommended for normal use of the mod; enable it only for research sessions and additional runtime logging.
; false - keep diagnostic telemetry disabled.
; true  - enable CameraState, ZOOM and HorPlus FOV telemetry.
Enabled=false


[Hotkeys]
; Optional runtime controls for changing and comparing modes without restarting
; the game. Changes made through hotkeys are saved to this configuration.
;
; Hotkeys are intended primarily for testing, comparison and configuration;
; they can remain disabled during normal gameplay.
; true  - enable all runtime hotkeys listed below.
; false - disable all runtime hotkeys.
Enabled=false

; Cycle the gameplay correction mode immediately: AspectRecalculation -> HorPlus -> AspectRecalculation.
; Supported keys: F1-F12, 0-9 and A-Z.
GameplayCycle=F9

; Cycle the cinematic aspect mode for the next cinematic: Auto -> Native -> 16:9 -> 21:9 -> 32:9 -> Auto. Does not affect a cinematic that is already playing.
; Supported keys: F1-F12, 0-9 and A-Z.
CinematicCycle=F10

; Cycle the cinematic FOV mode for the next cinematic: GameplayHorPlus -> NativeHorPlus -> GameplayHorPlus. Does not affect a cinematic that is already playing.
; Supported keys: F1-F12, 0-9 and A-Z.
CinematicFovCycle=F11

; Cycle the dialogue zoom mode for the next dialogue: Native -> Adaptive -> Reduced -> Disabled -> Native. Does not affect a dialogue that is already in progress.
; Supported keys: F1-F12, 0-9 and A-Z.
DialogueCycle=F12
```

Manual INI changes require a game restart. When hotkeys are enabled, Gameplay changes apply immediately; Cinematic and Dialogue selections apply to the next corresponding event and do not rebuild an active lifecycle.

For seamless Gameplay/Cinematic transitions, use `Gameplay.Mode=HorPlus`, `Cinematics.FovMode=GameplayHorPlus` and `Cinematics.AspectRatio=Auto`, or use a forced cinematic aspect matching the actual Gameplay aspect.

**Installation**

1. Install Ultimate ASI Loader.
2. Place `dsound.dll` in `Stalker2\Binaries\Win64`.
3. Remove previous `STALKER2UltrawideFix.asi`, `STALKER2GameplayAspectFix.asi` and their old INI/log files.
4. Copy `STALKER2CameraTweaks.asi` and `STALKER2CameraTweaks.ini` into the same folder.
5. Launch the game.

**Tested**

- Steam build 2.0.6 with UE 5.5.4.
- Static resolver portability across Steam builds 2.0.2, 2.0.3, 2.0.4 and 2.0.5; runtime validation of the current production implementation is on 2.0.6.
- Gameplay at 16:9, 21:9 and 32:9, including F9 switching, ADS/binocular transitions, camera recreation and death/load rebuilds.
- Cinematic `Auto` aspect changes, forced 16:9/21:9/32:9 framing and arbitrary runtime aspect handling.
- `NativeHorPlus` and `GameplayHorPlus`, including live F11 switching.
- Dialogue Native, Adaptive, Reduced and Disabled modes, recovery and coexistence with Cinematics/ADS.
- Save/load camera recreation, cinematic EXIT/recovery and F10/F12 lifecycle switching.

**Known issues and limitations**

- Weapon/viewmodel FOV can remain incorrectly framed after a cinematic, load or gameplay-camera rebuild. This is a separate game-side path and is not modified by this release. [Weapon Viewmodel FOV](https://www.nexusmods.com/stalker2heartofchornobyl/mods/2422) can be used alongside this mod.
- In windowed mode, a custom resolution whose aspect is not represented by a native game aspect mode may be resized by the game's native `AspectRecalculation` return to `Auto`. This does not mean arbitrary-aspect `HorPlus` is unsupported.
- Manual configuration changes require a restart; hotkey selections do not.
- Future patches may require updated signatures. The resolver fails safely when validation is ambiguous or unsuccessful.
- The native post-cinematic FOV recovery remains game-owned and is not rewritten by this release.

Do not load this release together with old `STALKER2UltrawideFix.asi` or `STALKER2GameplayAspectFix.asi` files.

The release archive includes the production ASI, default INI, README, license and third-party notices. Diagnostic ASIs, research files, logs and historical binaries are not included.
