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
Enabled=true
; AspectRecalculation or HorPlus.
Mode=HorPlus

[Cinematics]
; Auto, Native, 16:9, 21:9, 32:9
AspectRatio=Auto
; GameplayHorPlus or NativeHorPlus.
FovMode=GameplayHorPlus

[Dialogue]
; Native, Adaptive, Reduced or Disabled.
Zoom=Adaptive

[Diagnostics]
; Optional read-only runtime telemetry; not recommended for normal use.
Enabled=false

[Hotkeys]
; Optional runtime controls for testing and comparison; disabled by default.
Enabled=false
GameplayCycle=F9
CinematicCycle=F10
CinematicFovCycle=F11
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
