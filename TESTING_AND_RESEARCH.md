# Testing And Research Summary

## Current unified mod — v1.0.0

The current artifact is `STALKER2CameraTweaks.asi`, intended to replace the
older `STALKER2UltrawideFix.asi` and `STALKER2GameplayAspectFix.asi`. Do not
load old and new files together; remove
the older ASI before installing the unified one.

The production configuration has four independent areas:

```ini
[Gameplay]
Enabled=true
Mode=HorPlus

[Cinematics]
AspectRatio=Auto
FovMode=GameplayHorPlus

[Dialogue]
Zoom=Adaptive

[Hotkeys]
Enabled=false
GameplayCycle=F9
CinematicCycle=F10
CinematicFovCycle=F11
DialogueCycle=F12
```

`Gameplay.Enabled` controls gameplay aspect correction. `Cinematics.AspectRatio`
selects automatic, native or forced cinematic framing. `Dialogue.Zoom` supports
`Native`, `Adaptive`, `Reduced` and `Disabled`; the default `Adaptive` mode
preserves the native optical zoom strength relative to the actual gameplay FOV.
`Gameplay.Mode` supports `AspectRecalculation` and `HorPlus`; `HorPlus`
preserves the game's original gameplay FOV changes and adapts them to the
current aspect ratio. `Cinematics.FovMode` supports `GameplayHorPlus` and
`NativeHorPlus`. Hotkeys are optional testing controls, disabled by default,
and support `F1-F12`, `0-9` and `A-Z`. F9 switches gameplay mode immediately;
F10 selects cinematic aspect, F11 selects cinematic FOV mode and F12 selects
dialogue zoom mode for the next corresponding lifecycle. They do not rebuild
an already active cinematic or dialogue.

The INI is created automatically beside the ASI when missing. `Auto` uses the
runtime camera aspect and updates when the game resolution changes during the
same session. `Native` bypasses both cinematic hooks; forced 16:9, 21:9 and
32:9 modes provide custom cinematic framing with matching FOV. The obsolete
`FovCorrection` option is not part of the unified v1.0.0 configuration. Settings
from previous INI files are not migrated; managed descriptions and categories
are still synchronized when the new INI already exists.

## Research progression

The project evolved through these bounded evidence phases:

```text
Gameplay camera-state discovery
→ gameplay writer identification
→ dynamic signature resolution
→ camera rebuild / re-arm lifecycle
→ cinematic lifecycle discovery
→ shared authoritative camera state
→ cinematic aspect-writer provenance
→ live cinematic FOV consumption boundary
→ Hor+ FOV feasibility
→ combined cinematic correction
→ unified gameplay/cinematic coordinator
→ post-cinematic handoff investigation
→ 21:9 gameplay regression analysis
→ dynamic runtime Auto aspect policy
→ GameData dialogue-FOV semantic audit
→ dialogue configuration/ownership and target-assignment audit
→ WIDEBOY runtime-boundary reference audit
→ historical-2.0.4 dialogue boundary discovery
→ dialogue live-sample lifecycle classification
→ Adaptive and optical half-strength feasibility
→ EXIT discontinuity diagnosis and recovery anchoring
→ production dialogue integration and hotkey/persistence validation
→ cross-patch dialogue resolver validation
→ post-cinematic atomic handoff investigation
→ physical FOV setter and native bypass investigation
→ native bypass deferred after bounded static/runtime research
→ production combined atomic integration
→ cross-patch production resolver audit 2.0.2–2.0.5
→ production runtime validation on Steam 2.0.5
→ production runtime validation on Steam 2.0.6
→ v1.0.0 release preparation
```

The detailed historical plans are preserved in the
[`research archive`](research/), while the active follow-up work remains in
the [`backlog`](backlog/).

## Historical Steam 2.0.4 evidence

The following sections preserve evidence collected from the Steam 2.0.4
executable. They are historical research evidence, not the current v1.0.0
runtime-validation basis. Current production runtime validation is on Steam
2.0.6; older-build runtime support is not claimed without separate runtime
validation.

### Gameplay

- The gameplay camera writer is resolved through a unique executable `.text`
  signature and validated by decoding `MOVSS [RBX+0x30], XMM0`.
- The generalized ultrawide predicate accepted the observed aspect above native
  16:9 and preserved that source aspect during the historical two-pass
  correction. v1.0.0 production uses the validated atomic apply instead.
- 21:9 startup, manual `21:9 → 16:9 → 21:9`, death/load camera rebuild and
  32:9 regression were user-tested successfully.
- The player's selected FOV is preserved.
- The separate weapon/viewmodel FOV issue after loading on 21:9 is a known
  game-side problem and is outside this fix.

### Native gameplay aspect reevaluation control

- A read-only UE4SS automatic dump captured the same live `CameraComponent`,
  `Stalker2.CameraManager` ownership references and `PlayerCameraManager`
  across a manual native gameplay aspect transition. Numeric object IDs may
  change after reloads; the evidence is tied to the same live objects within
  the tested session.
- The tested sequence was: `32:9` gameplay with the incorrect vertical
  framing, manual `16:9` with aspect constraint enabled and correct vertical
  framing, then return to `32:9` after the native recalculation.
- The observed states were:

  ```text
  A — 32:9 before transition:
      FOV=90, AspectRatio=3.555556, bConstrainAspectRatio=false

  B — 16:9 constrained transition:
      FOV=90, AspectRatio=1.777778, bConstrainAspectRatio=true

  C — 32:9 after transition:
      FOV=90, AspectRatio=1.777778, bConstrainAspectRatio=false
  ```

- `AspectRatioAxisConstraint` remained `MaintainXFOV` (`Axis=1`) and
  `bOverrideAspectRatioAxisConstraint` remained `false` in the captured
  states; no axis-policy transition was observed.
- The final correct 32:9 gameplay result therefore does not require the live
  camera `AspectRatio` to equal the physical display aspect. States A and C
  share `FOV=90` and `bConstrainAspectRatio=false` but have different camera
  aspect state and different visual results.
- The result confirms a native/settings-driven reevaluation sequence, not a
  single sufficient final property state. A storage-only property write is
  not equivalent to the native transition and is not promoted as a solution.
- This control intentionally excludes the cinematic CameraTweaks branch. The
  gameplay fix is not treated as the source of this native behavior; it is a
  separate production workaround that preserves the player's FOV while
  replaying the useful gameplay transition behavior.
- Confirmed: native A/B/C transition, same Camera/CameraManager/PCM
  ownership, authored FOV preserved at `90`, and no observed axis-enum
  transition. Unresolved: the exact native operation/evaluation event that
  performs the reevaluation, where the downstream view/projection rebuild is
  triggered, and whether an equivalent path can be invoked during a
  cinematic without the Hor+ FOV rewrite.

### Cinematics — historical 2.0.4 evidence

- Legacy 2.0.3 and historical 2.0.4 transition topology was reconstructed;
  current signature resolution is based on semantic instruction patterns, not
  fixed cinematic RVAs.
- The cinematic aspect store and ENTER/EXIT live-FOV consumer callsites are
  uniquely signature-resolved and fail closed on ambiguity or validation
  failure.
- The validated current boundaries are the aspect store equivalent of
  `RVA 0x6B7CB05` and live-FOV callsites equivalent to
  `RVA 0x2EE6936`/`0x2EE69A7` in the historical 2.0.4 image.
- On 21:9, runtime aspect `2.38889` produces correct cinematic framing and
  Hor+ FOV. On 32:9, runtime aspect `3.55556` produces Hor+ FOV about
  `126.87` from authored FOV `90`.
- Forced 16:9, 21:9 and 32:9 cinematic framing was user-tested. Forced 32:9
  at 2560x1440 correctly produced cinematic letterbox bars.
- `Auto` was user-tested without restarting through
  `16:9 → 21:9 → 32:9 → 16:9 → 21:9 → 32:9`; each cinematic aspect store and
  ENTER FOV boundary used the current aspect. Cinematic EXIT recovery into
  gameplay also passed at each tested aspect.
- Forced 21:9 uses the canonical 3440x1440 aspect `2.3888889`, producing
  cinematic FOV about `106.688` from authored FOV `90`.
- Native cinematic EXIT FOV recovery remains game-owned and untouched.
- Startup logs record uppercase SHA-256 identities for the loaded ASI and game
  executable. The historical 2.0.4 game identity was
  `2ECC5D19FE37F97E3F7F2467D652B299B5A47F010FA49FD803A49A4A6930A409`.

### Dialogue — historical 2.0.4 evidence

- The packaged `CoreVariables` reference established `DialogFOVDefault=70.0`
  as the native dialogue baseline. `CutsceneFOVDefault` remains separate and is
  not modified by this feature.
- Configuration/default registration and `DisplayFOV`/`CurrentFOV` parameter
  plumbing were audited but were not promoted as dialogue lifecycle owners.
- The WIDEBOY dialogue reference was used only to identify a plausible runtime
  boundary. The historical 2.0.4 equivalent resolved uniquely at hook boundary
  `RVA 0xD20F77`, where `XMM1` carries live native dialogue FOV samples.
- The boundary was proven to carry the native live stream, not a one-shot target:
  gameplay around `110` descends smoothly to `70` during dialogue and recovers
  to gameplay after EXIT. The boundary is not traversed during the tested
  ADS-only scenario.
- The former broad candidate searches for `DisplayFOV`, `CurrentFOV`, compact
  camera fields and downstream projection consumers did not identify a safer
  native target owner. They remain historical/deferred evidence, not production
  dependencies.
- Production dialogue policies are:
  - `Native` — pass through the game's original dialogue stream, targeting its
    native `70°` dialogue FOV.
  - `Adaptive` — preserve the native optical zoom strength relative to the
    actual gameplay baseline `G`, using projection-space geometry.
  - `Reduced` — apply half of the Adaptive optical zoom strength in projection
    space.
  - `Disabled` — hold the captured gameplay baseline `G` through dialogue.
- Adaptive and Reduced were validated at high gameplay FOV baselines near
  `90`, `110` and `120`; endpoint values matched the optical model and recovery
  returned to the actual captured gameplay baseline.
- The initial transformed EXIT path exposed the native EXIT sample jump. The
  validated EXIT anchor/recovery state now starts from the transformed dialogue
  endpoint and maps native recovery smoothly back to `G`, preserving native
  timing without camera writes or custom timers.
- Cinematic isolation resets dialogue transient state during cinematic active and
  recovery phases. The first dialogue after a cinematic captured the gameplay
  baseline rather than the cinematic FOV, and two sequential dialogues recovered
  without stale state.
- Historical production-candidate runtime validation on Steam 2.0.4 passed
  `Native`, `Adaptive`, `Reduced` and `Disabled`, including sequential cycles,
  cinematic-to-dialogue isolation, ADS-only specificity and configurable
  hotkey selection. This does not extend the current v1.0.0 runtime claim to
  Steam 2.0.4.

## Compatibility boundary

Gameplay, cinematic and dialogue boundaries use guarded signature resolution.
The current v1.0.0 production implementation was runtime-validated on Steam
2.0.6. The production resolver set was statically validated across Steam
2.0.2–2.0.5 despite relocated RVAs. This is static cross-patch portability
evidence only; runtime support for Steam 2.0.2–2.0.5 is not claimed without
separate runtime validation. A future executable identity still requires
fresh resolver and runtime validation.

The current v1.0.0 production artifact is runtime-validated on Steam 2.0.6.
Its current release-candidate ASI SHA-256 is
`D04A43E28DB5DFFD10D88B6F30BEF8FEC2A560E1CD9949FEA31FAF485DA0E7BC`.
The production and release-assets ASI files have the same hash. The diagnostic
artifact is built separately and is not part of the production release.

## Closed and deferred research

- Static interpolation/scalar-shape candidate ranking was closed after
  runtime rejection of unrelated candidates.
- Legacy transition-hub mapping and live-FOV consumption recovery are closed;
  the current live-FOV boundary is confirmed.
- Aspect writer provenance and immediate-patch feasibility are closed for the
  tested path.
- Post-EXIT atomic B/C scheduling was tested and closed as a production
  solution: it preserved mechanics but did not remove the visible seam.
- The v1.0.0 atomic gameplay apply and first-descending-sample
  `RecoveryStart` handoff passed production runtime validation on Steam 2.0.6;
  the old staged `0x5` replay is not used by the production path.
- Native cinematic FOV bypass research remains deferred. The game's native
  post-cinematic FOV recovery is preserved and is not rewritten by v1.0.0.
- Downstream writer/projection candidate searches were closed for the current
  evidence set without a promoted renderer consumer.
- Dialogue parameter plumbing and compact-field target-owner searches were
  closed or deferred after the concrete live boundary was established.
- The WIDEBOY-derived dialogue boundary was independently mapped and validated;
  its direction heuristic was not copied into production.
- Dialogue feasibility and production promotion passed with the four-policy
  lifecycle model, EXIT recovery anchor, cinematic isolation and ADS-specificity
  guard.
- Weapon/viewmodel ownership research remains deferred pending a new validated
  object or downstream projection anchor.
- Dynamic resolution changes during a running session are validated for
  `AspectRatio=Auto` across 16:9, 21:9 and 32:9. Configuration file changes
  still require a game restart.
- The native gameplay aspect reevaluation sequence above is validated from a
  bounded UE4SS runtime capture. The exact triggering native function and
  downstream projection owner were not identified; no compatibility or
  cinematic behavior claim follows from this control alone.

### Subtitle horizontal centering — deferred / rejected for production

- The subtitle displacement reproduces on Steam 2.0.5 without
  `STALKER2CameraTweaks`, including at `2560x1440` and `5120x1440`; it is
  therefore not attributed to the camera/aspect mod.
- UE4SS located the live `SubtitleView` widget. Its
  `SetRenderTranslation()` moves the complete subtitle composition — speaker
  name, punctuation, dialogue text and background — while leaving the
  dialogue-choice UI unaffected.
- Child-level `HorizontalAlignment`, `VerticalAlignment`, `Justification`,
  width override and fixed per-child offsets did not correct final placement.
- Fixed `SubtitleView` offsets and resolution/aspect-specific offset tables are
  rejected because rendered composition width changes with speaker name,
  dialogue length, wrapping and content.
- UE4SS Lua did not expose reliable post-layout `FGeometry` or viewport
  geometry. A bounded C++/Slate Batch 1 inspection found no Unreal/Slate
  headers, UObject/Slate bridge, viewport geometry interface or established
  safe ABI anchor in the current ASI project; no research probe was built.
- Production changes: none. No subtitle hook, UI write, Slate integration or
  heuristic correction is part of the release.
- Status: `SUBTITLE GEOMETRY RESEARCH — DEFERRED`; the fix remains rejected
  until an independent safe post-layout geometry interface is established.

#### UE4SS runtime evidence

The following observations were made in the Steam 2.0.5 game session with
UE4SS and are retained as research evidence only. All property changes were
temporary runtime experiments and were cleared by restarting or reloading the
game; none were added to the production ASI.

- The actor dump `1789298599-ue4ss_actor_data.csv` did not provide the live UMG
  instance. UE4SS Lua found exactly one live object:

  ```text
  SubtitleView_C /Engine/Transient.Stalker2GameEngine_2147482609:
  BP_SML_C_2147482553.SubtitleView_C_2147461272
  ```

- The live `USubtitleView` fields identified from the UE4SS headers were:
  `SpeakerDialogText`, `TwoPoint`, `NameBox`, `SubtitileBorder`,
  `SubtitileContainer` and `TextDialog`.

- The initial widget relationships were:

  ```text
  Overlay_1
  ├─ SubtitileContainer → SubtitileBorder → TextDialog
  ├─ NameBox → SpeakerDialogText + TwoPoint
  └─ separate dialogue-choice UI elements
  ```

- The live text values were confirmed independently:

  ```text
  TextDialog         = "Здоров!"
  SpeakerDialogText  = "Вітя Бусел"
  TwoPoint           = ":"
  ```

- Initial observed layout values included:

  ```text
  TextDialog.CommonTextObj.Justification = 0
  SpeakerDialogText.CommonTextObj.Justification = 2
  TextDialog.GetDesiredSize().X ≈ 85.64999
  SubtitileContainer.WidthOverride = 800
  SubtitileContainer.HeightOverride = 100
  SubtitileContainer slot padding = L0 T0 R0 B50
  NameBox slot = H3 / V1
  ```

  The observed `H3/V1` on `NameBox` corresponds to right/top in the tested
  Overlay layout; changing it to center did not change the final rendered
  position.

- Temporary child-level tests were performed and visually rejected:

  ```text
  TextDialog.CommonTextObj:SetJustification(2)
  SubtitileContainer.Slot:SetHorizontalAlignment(2)
  SubtitileBorder:SetHorizontalAlignment(2)
  TextDialog.Slot:SetHorizontalAlignment(2)
  SubtitileContainer.WidthOverride = 0
  ```

  The values changed or the calls returned successfully, but the rendered
  subtitle position did not follow them.

- `RenderTransform` fields reported zero translation, unit scale and zero
  angle. Direct field edits did not affect rendering. The callable
  `SetRenderTranslation()` did affect rendering:

  ```text
  TextDialog +500                  → moved only the dialogue text
  SpeakerDialogText +500           → moved the speaker text separately
  NameBox +500                     → moved the name and colon together
  SubtitleView +500                → moved the complete subtitle composition
                                      while dialogue-choice UI stayed in place
  ```

  The `SubtitleView` root test was the only confirmed whole-block control
  point. Fixed trial values such as `+500`, `+650` and a compensating `-150`
  on a child were diagnostic only and are explicitly rejected as a solution.

- Hiding `TextDialog` immediately removed `"Здоров!"`, confirming that it was
  the active rendered text. Hiding `SubtitileBorder` removed both the text and
  its background, confirming their parent relationship.

- `FindAllOf("FadeoutScreen")` returned no live instances during the tested
  dialogue. The subtitle was therefore not attributed to a separate active
  `UFadeoutScreen` layer.

- `GetCachedGeometry()` returned a wrapper, but the UE4SS Lua binding did not
  expose usable numeric absolute position/size or local-size methods. A broad
  parent/child traversal was attempted once and caused a game crash; it was
  abandoned and is not part of the accepted evidence method.

- Final UE4SS conclusion:

  ```text
  SubtitleView whole-block translation: CONFIRMED
  Child alignment/justification control: NOT EFFECTIVE
  Post-layout FGeometry via Lua: NOT AVAILABLE
  Fixed offset: REJECTED
  Production UI write: NONE
  ```

## Testing limits

## 2026-09-14 — Weapon Viewmodel FOV research boundary

- Reference WVF behavior is established: `.wvf` profile selection and
  viewport/FOV math write `/Game/_Stalker_2/Materials/MPC/MPC_FOV.MPC_FOV`:
  `TanFOV`.
- The `TanFOV → visible weapon/viewmodel framing` relationship was causally
  validated through one controlled UE4SS intervention with confirmed readback.
- The reference runtime bootstrap is established: `WVF_C` spawns
  `S2Dev_Event_Watcher_C` and `WVF_Actor_C`; the actor's profile path reaches
  the `TanFOV` write.
- Standalone ASI access remains unestablished. No validated,
  patch-resilient UObject/UClass/UFunction registry or invocation bridge is
  present in the current architecture. No guessed offsets, layouts, RVAs or
  `ProcessEvent` ABI were accepted.
- Therefore weapon/viewmodel FOV is deferred at the production boundary, not
  because the controlling mechanism is unknown:

  ```text
  Reference mechanism       CONFIRMED
  TanFOV causal ownership   CONFIRMED
  UE4SS MPC access          CONFIRMED
  Native ASI bridge         DEFERRED
  Repair timing/value       DEFERRED
  Custom Weapon FOV         DEFERRED
  Production implementation NONE
  Production changes        NONE
  ```

- The active WVF feature-development branch is closed. Reopen only if a new,
  independently grounded UE reflection/invocation bridge becomes available.

- Build success proves compilation and linking only.
- A signature match is not hook proof without decode and runtime evidence.
- The current production gameplay/cinematic handoff is runtime-validated on
  Steam 2.0.6. Older-build evidence remains static portability evidence only.
- Dialogue runtime behavior is covered by the current 2.0.6 production test
  scope; no runtime compatibility claim is made for older Steam builds.
- Dialogue runtime validation covers Native, Adaptive, Reduced and Disabled,
  high-FOV baselines, smooth ENTER/EXIT recovery, sequential cycles,
  cinematic-to-dialogue isolation, ADS-only specificity and F9/F10 policy
  selection. Runtime hotkeys apply to the next lifecycle, not an active one.
- `NativeHorPlus`, `GameplayHorPlus`, `Auto`, forced `16:9`, forced `21:9` and
  forced `32:9` were all runtime-tested on the native 5120x1440 display or in
  the Auto hot-switch sequence.
- The game's native post-cinematic FOV recovery remains a visible native
  transition in some scenarios and is intentionally untouched.
- Subtitle horizontal positioning is a separate vanilla UI issue. It was
  reproduced without the mod, and no production compatibility or fix claim is
  made for it.

## Release checklist

- Remove older `STALKER2UltrawideFix.asi` and `STALKER2GameplayAspectFix.asi` before installing the unified
  ASI.
- Include only `STALKER2CameraTweaks.asi`, its INI and the required release
  documentation in the release package.
- Keep research ASIs, historical binaries, logs and Ghidra projects out of the
  release archive.
- Preserve the runtime identity line in support reports.
- The v1.0.0 release candidate contains only the production ASI, INI, README,
  license and third-party notices. Diagnostic ASIs, historical binaries,
  logs and research files remain outside the release archive.
