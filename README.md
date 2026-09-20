# STALKER 2 Ultrawide and Camera Tweaks for UE 5.5.4

## Version 1.0.0

Source for `STALKER2CameraTweaks.asi`, a unified ultrawide, cinematic and camera/FOV fix for **S.T.A.L.K.E.R. 2: Heart of Chornobyl**. The current implementation is runtime-tested on Steam game build `2.0.6` with Unreal Engine `5.5.4`.

## Features

- Corrects gameplay aspect transitions on 16:9, 21:9 and 32:9 displays.
- Provides two gameplay correction modes: validated native `AspectRecalculation` and `HorPlus`, which preserves the game's original gameplay FOV changes and adapts them in real time to the current aspect ratio.
- Applies the gameplay framing correction atomically, without the old staged
  intermediate transition.
- Applies one atomic gameplay handoff at the start of post-cinematic native FOV
  recovery, removing the mod's additional post-cinematic flick.
- Re-arms gameplay correction after camera rebuilds, including death/load.
- Preserves the player's selected gameplay FOV.
- Corrects cinematic aspect and applies Hor+ cinematic FOV.
- Provides default `GameplayHorPlus` cinematic FOV that follows the selected Gameplay FOV, with `NativeHorPlus` as an independent alternative.
- Uses the game's runtime camera aspect rather than desktop dimensions.
- Dynamically follows `Auto` aspect changes during the same game session without requiring a restart.
- Supports custom cinematic framing independently of the physical display: `Auto`, `Native`, `16:9`, `21:9` or `32:9`.
- Provides FOV-aware dialogue zoom modes: `Native`, `Adaptive`, `Reduced` and `Disabled`.
- Optional runtime hotkeys switch gameplay immediately and select the aspect/FOV mode for the next cinematic or dialogue; they are disabled by default.
- Creates `STALKER2CameraTweaks.ini` automatically when it is missing.
- Resolves gameplay and cinematic hook locations through guarded signatures and refuses safely when validation is ambiguous or fails.

## Download and installation

Prebuilt releases and installation instructions are available on [Nexus Mods](https://www.nexusmods.com/stalker2heartofchornobyl/mods/2416).

For local builds:

1. Install [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) as `dsound.dll` in `Stalker2\\Binaries\\Win64`.
2. Remove previous `STALKER2UltrawideFix.asi`, `STALKER2GameplayAspectFix.asi` and their old INI/log files. Do not load old and new ASIs together.
3. Copy `STALKER2CameraTweaks.asi` and `STALKER2CameraTweaks.ini` to the same `Win64` directory.
4. Start the game normally.

The plugin creates `STALKER2CameraTweaks.log` beside the game executable. The startup log includes SHA-256 values for the loaded mod and game executable, which helps verify support reports.

## Configuration

```ini
; Most configuration changes require restarting the game.
; When runtime hotkeys are enabled, Gameplay changes apply immediately, while
; Cinematics and Dialogue selections apply to the next applicable event.

[Gameplay]
; Use true to enable gameplay aspect correction, or false to disable it.
Enabled=true
; Gameplay correction mode: HorPlus or AspectRecalculation.
;
; HorPlus - default and recommended mode. Preserves the game's native Gameplay
;          FOV changes and adapts them in real time to the current runtime aspect
;          ratio. Supports arbitrary/custom aspect ratios and native FOV changes
;          such as ADS and binocular zoom.
;
; AspectRecalculation - alternative mode that uses the game's native
;          aspect/projection transition to correct Gameplay framing while
;          preserving the selected Gameplay FOV.
;
; On custom windowed aspect ratios not represented by a native game aspect mode,
;          AspectRecalculation may return the window to the display's native
;          aspect/size when the game restores Auto. This limitation does not
;          apply to HorPlus.
;
; HorPlus examples when idle:
; Gameplay FOV 90°:  16:9 -> 90°; 21:9 -> approximately 106.69°;
;                    32:9 -> approximately 126.87°.
; Gameplay FOV 100°: 16:9 -> 100°; 21:9 -> approximately 116.04°;
;                    32:9 -> approximately 134.48°.
; Gameplay FOV 110°: 16:9 -> 110°; 21:9 -> approximately 124.95°;
;                    32:9 -> approximately 141.41°.
Mode=HorPlus

[Cinematics]
; Auto, Native, 16:9, 21:9, 32:9
AspectRatio=Auto
; GameplayHorPlus - default with Gameplay.Mode=HorPlus; follows changes to the
;                  game's Gameplay FOV while preserving authored variation.
;                  Falls back to NativeHorPlus when its context is unavailable.
;                  For authored cinematic FOV 90° with Gameplay FOV 90°:
;                  16:9 -> 90°; 21:9 -> approximately 106.69°;
;                  32:9 -> approximately 126.87°.
;                  With Gameplay FOV 112.6°:
;                  16:9 -> approximately 112.6°; 21:9 -> 118.51°;
;                  32:9 -> approximately 143.13°.
; NativeHorPlus   - alternative native/authored cinematic FOV mode independent
;                  of the player's Gameplay FOV setting.
;                  For authored cinematic FOV 90°:
;                  16:9 -> 90°; 21:9 -> approximately 106.69°;
;                  32:9 -> approximately 126.87°.
FovMode=GameplayHorPlus

[Dialogue]
; Native   - keep the game's original dialogue zoom, currently targeting 70°.
; Adaptive - preserve the native optical zoom strength relative to the current gameplay FOV.
; Reduced  - apply half of the Adaptive optical zoom strength.
; Disabled - keep the current gameplay FOV during dialogue.
Zoom=Adaptive

[Diagnostics]
; Optional read-only runtime telemetry. Not recommended for normal use;
; enable only for research sessions and additional runtime logging.
Enabled=false

[Hotkeys]
; Optional runtime controls for testing and comparing settings without restarting the game.
; Intended mainly for research and comparison; disable for normal use.
; Use true to enable all runtime hotkeys, or false to disable them.
Enabled=false

; Cycle the gameplay correction mode immediately:
; AspectRecalculation -> HorPlus -> AspectRecalculation.
; Supported keys: F1-F12, 0-9 and A-Z.
GameplayCycle=F9

; Cycle the cinematic aspect mode for the next cinematic:
; Auto -> Native -> 16:9 -> 21:9 -> 32:9 -> Auto.
; Does not affect a cinematic that is already playing.
; Supported keys: F1-F12, 0-9 and A-Z.
CinematicCycle=F10

; Cycle the cinematic FOV mode for the next cinematic:
; NativeHorPlus -> GameplayHorPlus -> NativeHorPlus.
; Does not affect a cinematic that is already playing.
; Supported keys: F1-F12, 0-9 and A-Z.
CinematicFovCycle=F11

; Cycle the dialogue zoom mode for the next dialogue:
; Native -> Adaptive -> Reduced -> Disabled -> Native.
; Does not affect a dialogue that is already in progress.
; Supported keys: F1-F12, 0-9 and A-Z.
DialogueCycle=F12
```

`Auto` follows the game's runtime camera aspect and applies matching Hor+ FOV, including arbitrary valid runtime aspects. `GameplayHorPlus` is the default cinematic FOV mode with `Gameplay.Mode=HorPlus`: it follows changes to the game's Gameplay FOV setting while preserving authored cinematic variation. `NativeHorPlus` remains available as an alternative that is independent of Gameplay FOV. Forced cinematic aspects should match the gameplay aspect for the intended seamless transition. Restart the game after manually editing the INI file. Runtime hotkey selections do not require a restart and apply to the next corresponding cinematic or dialogue.

## Build requirements

- Visual Studio 2022 17.14 or newer with the Desktop development with C++
  workload and the x64 MSVC tools component.
- The production source uses C++23-era language/library facilities. With the
  supported MSVC 17.14 toolset, `build.cmd` selects `/std:c++latest` because
    that compiler exposes the required C++23 feature set through that switch.
  - `build.cmd` creates the production `STALKER2CameraTweaks.asi` without
    high-rate research instrumentation. `build-diagnostic.cmd` creates the
    separate `STALKER2CameraTweaksDiagnostic.asi` with supported diagnostic
    instrumentation; it is not intended for performance comparison.
- [SafetyHook](https://github.com/cursey/safetyhook), including its bundled Zydis source.
- [spdlog](https://github.com/gabime/spdlog).

Place dependencies under `external/safetyhook` and `external/spdlog`, then run
`build.cmd` from this directory. The script discovers the supported Visual
Studio installation with `vswhere`; it does not depend on the author's local
drive path. Production compilation does not define research/test switches.

## Development and research tools

The following tools were used during development, reverse engineering and
validation of this project:

### Production development

- Visual Studio 2022 and MSVC with C++23 support.
- C++23, Windows SDK and PowerShell build/automation scripts.
- SafetyHook for guarded native hooks.
- Zydis for instruction decoding and structural signature validation.
- spdlog for runtime diagnostics and support logs.
- Ultimate ASI Loader for loading the ASI in the game.
- Git for source, research and evidence history.

### Static and runtime research

- Ghidra for executable analysis, callgraph reconstruction and static
  ownership research.
- UE4SS for reflected runtime inspection, object discovery, function hooks,
  `.usmap` generation and bounded Lua probes.
- CUE4Parse with .NET 10 for direct UE5 IoStore/Zen package inspection,
  cooked Blueprint export recovery and Kismet analysis.
- A custom `CUE4ParseWVF` research dumper for the targeted `WVF` and
  `WVF_Actor` packages.
- Custom UE4SS Lua probes for AnimScriptInstance, WVF lifecycle and
  `MPC_FOV.TanFOV` causal validation.
- `repak`, `retoc` and `UAssetToolRivals` for exploratory IoStore extraction
  attempts.
- Oodle runtime/tooling support as part of the UE5 IoStore and asset-tool
  investigation.

### Evaluated but not used as the final path

- UAssetGUI was considered for traditional `.uasset/.uexp` inspection, but
  the direct CUE4Parse path made it unnecessary.
- FModel was considered for package discovery, but it was not used as the
  authoritative WVF analysis path.

The research tools and probes are kept under `research/` and are not part of
the production release package.

## Tested scope and limitations

- Runtime-tested on Steam game build `2.0.6`; the tested executable identity is recorded in the startup log.
- Static resolver portability was also checked against Steam builds `2.0.2`, `2.0.3`, `2.0.4` and `2.0.5`; these older builds do not have separate runtime validation for this release.
- Gameplay tested at 16:9, 21:9 and 32:9, including startup, hot aspect switching, death/load rebuild and FOV preservation.
- `Auto` cinematics tested through `16:9 → 21:9 → 32:9 → 16:9 → 21:9 → 32:9` without restarting the game.
- Cinematics tested with `Native`, forced `16:9`, forced `21:9` and forced `32:9` policies on `5120x1440`.
- Forced 32:9 framing was also tested at `2560x1440` and correctly produced cinematic letterbox bars.
- `NativeHorPlus` and `GameplayHorPlus` cinematic FOV modes were tested with live F11 switching.
- F9 live switching between `AspectRecalculation` and `HorPlus` was tested without a reload, including ADS and binocular transitions.
- Dialogue coexistence, save/load camera recreation and cinematic EXIT/recovery were tested in the combined runtime session.
- The game retains its native post-cinematic FOV recovery; this release does
  not force or rewrite that native transition.
- Weapon/viewmodel FOV can remain incorrectly framed after a cinematic, load or gameplay-camera rebuild. This is a separate game-side issue and is not fixed by this mod.
- In windowed mode, a custom resolution whose aspect is not represented by a native game aspect mode may be resized by the game's native `AspectRecalculation` return to `Auto`. This does not mean arbitrary-aspect HorPlus is unsupported.
- Changing the game resolution during a session is validated for `AspectRatio=Auto`; restart the game after changing the configuration file itself.
- Signature resolution improves resilience to address relocation but does not guarantee compatibility with future patches. The plugin fails safely when validation does not pass.

## Screenshots

32:9 gameplay comparison, default FOV 90:

| Without Fix | Fix Enabled |
| --- | --- |
| ![32:9 without fix](screens/32-9%20default%20fov%2090%20without%20fix%20-%20nexus.jpg) | ![32:9 fix enabled](screens/32-9%20default%20fov%2090%20fix%20on%20-%20nexus.jpg) |

16:9 baseline, default FOV 90:

![16:9 baseline](screens/16-9%20default%20fov%2090%20without%20fix.png)

## Cinematic framing comparisons

The same cinematic framing policy can be selected independently of the
physical display aspect ratio. These examples show `Auto` at 16:9, and forced
16:9, 21:9 and 32:9 framing on a 32:9 display, plus forced 32:9 framing on a
16:9 display.

| Configuration | Example |
| --- | --- |
| `Auto` at 2560x1440 | ![Auto cinematic framing at 2560x1440](screens/Cutscene%20Auto-Default%20in%202560x1440.jpg) |
| Forced `16:9` at 5120x1440 | ![Forced 16:9 cinematic framing](screens/Cutscene%2016-9%20in%205120x1440.jpg) |
| Forced `21:9` at 5120x1440 | ![Forced 21:9 cinematic framing](screens/Cutscene%2021-9%20in%205120x1440%2Cjpg.jpg) |
| Forced `32:9` at 5120x1440 | ![Forced 32:9 cinematic framing](screens/Cutscene%2032-9%20in%205120x1440jpg.jpg) |
| Forced `32:9` at 2560x1440 | ![Forced 32:9 cinematic letterbox](screens/Cutscene%2032-9%20in%202560x1440.jpg) |

## Dialogue zoom comparisons

Dialogue zoom is calculated relative to the current gameplay FOV. The
comparison images below show the available production policies on a
`5120x1440` display.

| Configuration | Example |
| --- | --- |
| `Adaptive` / `Native` at gameplay FOV 90 | ![Dialogue Adaptive or Native zoom](screens/Dialog%20Default%20zoom%20or%20Adaptive%20for%2090%20game%20fov%20in%205120x1440.jpg) |
| `Reduced` at gameplay FOV 90 | ![Dialogue Reduced zoom](screens/Dialog%20Reduced%20zoom%20in%205120x1440.jpg) |
| `Disabled` | ![Dialogue Disabled zoom](screens/Dialog%20Disabled%20zoom%20in%205120x1440.jpg) |

`Native` preserves the game's original dialogue zoom, `Adaptive` preserves
its optical zoom strength relative to gameplay, `Reduced` applies half of the
Adaptive strength, and `Disabled` keeps the gameplay FOV during dialogue.

## Known viewmodel limitation

The game's weapon/viewmodel FOV may remain incorrectly framed after certain
cinematics, loads or gameplay-camera rebuilds. Aiming down sights or opening a
menu can refresh the game's viewmodel state. This mod does not change that
separate viewmodel path. [Weapon Viewmodel FOV](https://www.nexusmods.com/stalker2heartofchornobyl/mods/2422)
can be used alongside this mod for a more consistent viewmodel correction and
custom weapon FOV settings.

## Animated demonstrations

These recordings show the validated runtime behavior of the unified ASI:

| Area | Demonstration |
| --- | --- |
| Gameplay aspect correction | ![Gameplay aspect correction](screens/Gameplay.gif) |
| Cinematic framing and FOV | ![Cinematic framing and FOV](screens/Cutscene.gif) |
| Dialogue zoom policies | ![Dialogue zoom policies](screens/Dialog.gif) |

## License and credits

Original v1.0+ project code is source-available under the terms in
[LICENSE.md](LICENSE.md). Contributions and collaboration are welcome. You may
study and experiment with the source, create public development forks,
collaborate on changes, publish research and signatures, and share experimental
builds with collaborators or testers while developing changes, compatibility
fixes or contributions.

The main restriction is on publishing modified versions of this Project's
protected code as independent public end-user releases without permission.
General technical knowledge, research findings and compatibility tools are
outside the scope of the Project license. Work developed independently without
copying or adapting protected Project code is not restricted merely because
the Project was used as a technical reference. Exact unmodified mirrors of
official release packages are allowed.

With Elhait's permission, another developer may maintain or continue the
Project and publish releases through GitHub. This does not provide access to
Elhait's Nexus Mods account or permission to update the existing Nexus Mods
page.

This is not an OSI-approved open-source license. Third-party components remain
under their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Additional permission for GSC Game World is described separately in
[GSC_DEVELOPER_PERMISSION.md](GSC_DEVELOPER_PERMISSION.md).

BigChenga has separate permission to use Project implementations and solutions
directly in WIDEBOY Fixes. Elhait remains responsible for official Project
releases, with collaboration and upstream contributions as the intended path.
If Elhait permanently stops maintaining or abandons the Project, BigChenga may
maintain and publish a clearly attributed continuation. Temporary inactivity
or a period without releases does not activate this permission. See
[BIGCHENGA_PERMISSION.md](BIGCHENGA_PERMISSION.md).

Versions released before v1.0.0 were published under the MIT License. Those
historical grants remain valid and are not revoked by the v1.0+ terms.

Official release channels are this project's GitHub repository and Nexus Mods
page. Exact unmodified mirrors of official release packages are permitted.
Modified or independently rebuilt packages are not official releases unless
they are published under explicit maintainer or continuation permission from
Elhait.

This project is independent and is not a release of Lyall's STALKER2Tweak.

The [WIDEBOY Fixes by BigChenga](https://www.nexusmods.com/stalker2heartofchornobyl/mods/2337)
were used as a research reference for dialogue/camera FOV behavior. The
dialogue implementation in this project was independently reverse engineered
and runtime validated.

## Contact

For development questions, collaboration, licensing permissions or other
project-related inquiries, you can contact me through the links available
on my GitHub profile, including LinkedIn.

## Support development

- [Ko-fi](https://ko-fi.com/elhait)
- [Donatello](https://donatello.to/Elhait)
