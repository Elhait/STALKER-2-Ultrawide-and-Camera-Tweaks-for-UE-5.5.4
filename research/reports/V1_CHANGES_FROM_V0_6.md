# фактичні зміни між v0.6.0 та поточним v1 working tree

## Scope і база порівняння

- База: Git tag `v0.6.0`.
- Поточний стан: working tree/index на гілці `main`; зміни ще не є новим
  Git commit.
- Аналізовано: `git diff v0.6.0` у всьому repository.
- Зафіксований diff: 475 paths, приблизно 37,878 доданих і 2,929 видалених
  рядків. Частина обсягу припадає на історичні reports, task plans,
  переміщення research-файлів і видалення object-файлів, а не на production
  behavior.
- Runtime у межах цього diff-а не запускався. Наявні runtime claims нижче
  посилаються на вже збережені reports/evidence, а не на повторну перевірку.

## Короткий підсумок v1

Порівняно з v0.6.0 repository перейшов від компактнішого gameplay/cinematic
fix-а до модульної camera/FOV системи з окремими production state/evidence
типами, deterministic harnesses, runtime-evidence fixtures, diagnostics gate,
окремими production/diagnostic build paths і розширеною safety/resolver
інфраструктурою.

## Production source changes

### 1. Camera/FOV modules

Додано окремі модулі:

- `src/camera/horplus.*` — спільна Hor+ projection math;
- `src/camera/fov_observation.*` — typed native/transformed FOV observations,
  space/provenance/validity metadata;
- `src/camera/gameplay_baseline.*` — retained Gameplay baseline projection;
- `src/camera/gameplay_aspect_restoration.*` — production authority для
  aspect restoration;
- `src/camera/camera_state_snapshot.*` — derived camera-state telemetry;
- `src/camera/presentation_state.*` — presentation/coordinator state.

Ці модулі відокремлюють native evidence, transformed evidence, retained
semantic baseline і поточний presentation state.

### 2. Gameplay

Додано/виділено:

- `src/gameplay/horplus_gameplay.*` — arbitrary-aspect Gameplay HorPlus;
- `src/gameplay/aspect_policy.*` — aspect policy resolution;
- `src/gameplay/gameplay_camera.*` — gameplay camera writer/resolver path;
- `src/gameplay/gameplay_state.*` — gameplay mode/lifecycle state.

У конфігурації з'явилися два production gameplay modes:

- `AspectRecalculation` — native game aspect transition path;
- `HorPlus` — direct aspect-aware FOV correction, що зберігає оригінальні
  Gameplay FOV changes, включно з ADS/binocular trajectories.

Додано runtime mode switching і retained aspect restoration state. Legacy
aspect pair було прибрано після production cutover; camera source/FOV state,
потрібний Dialogue invalidation, збережено.

### 3. Cinematics

Додано окремі модулі:

- `src/cinematics/cinematic_aspect.*`;
- `src/cinematics/cinematic_fov.*`;
- `src/cinematics/cinematic_initialization.*`;
- `src/cinematics/cinematic_selection.*`.

Production cinematic configuration тепер має:

- `AspectRatio=Auto`, `Native`, `16:9`, `21:9`, `32:9`;
- `FovMode=GameplayHorPlus` — default v1 path, який використовує retained
  native Gameplay FOV baseline;
- `FovMode=NativeHorPlus` — alternative path, незалежний від Gameplay FOV.

Обидва cinematic FOV режими використовують unified baseline/transform path;
відрізняється reference/target baseline, а не окрема копія Hor+ math.

Додано cinematic selection snapshot/generation, ENTER context handling,
cached-transformed ENTER exclusion для diagnostics/prediction і fallback при
недоступному Gameplay context.

### 4. Dialogue

Додано/виділено:

- `src/dialogue/dialogue_fov.*` — Native/Adaptive/Reduced/Disabled policy math;
- `src/dialogue/dialogue_state.*` — lifecycle, Candidate, Active, Exiting,
  Recovery/Rearm state.

Поточні production additions включають:

- FOV-aware dialogue policies;
- policy snapshot semantics;
- Candidate source/target context hardening;
- post-cinematic exclusion;
- native-target recovery contract;
- bounded recovery re-arm;
- capability dependency checks.

### 5. Configuration and hotkeys

Додано повний config repository/template layer:

- `src/config/feature_config.*`;
- `src/config/config_repository.*`;
- `src/config/config_template.*`.

Поточний v1 default profile:

```ini
[Gameplay]
Mode=HorPlus

[Cinematics]
AspectRatio=Auto
FovMode=GameplayHorPlus
```

Hotkey order changed to:

- F9 — GameplayCycle;
- F10 — CinematicCycle;
- F11 — CinematicFovCycle;
- F12 — DialogueCycle.

Runtime hotkey selections are persisted to the INI and apply to the next
applicable cinematic/dialogue where specified.

### 6. Diagnostics and evidence publication

Додано:

- `src/diagnostics/diagnostic_runtime.*`;
- `src/diagnostics/matchgameplay_prediction.hpp`;
- `[Diagnostics] Enabled=false` runtime gate;
- CameraState, ZOOM, HorPlus and MatchGameplay diagnostic provenance.

Diagnostics are read-only and are not intended to own production decisions.
Cached transformed ENTER samples are explicitly labelled/excluded from native
MatchGameplay prediction.

### 7. Hooks, resolver і platform safety

Додано/виділено модулі:

- `src/hooks/hook_set.*`;
- `src/hooks/instruction_validator.*`;
- `src/hooks/signature_scanner.*`;
- `src/hooks/signatures/signature_definitions.hpp`;
- `src/platform/win32/memory.*`;
- `src/platform/win32/sha256.*`;
- `src/platform/win32/viewport.*`;
- `src/platform/win32/window.*`;
- `src/plugin/worker_lifecycle.*`;
- `src/plugin/feature_status.*`;
- `src/plugin/runtime.*`.

Це додало guarded signature resolution, instruction/decode validation,
feature availability/status reporting, runtime identity logging, worker
lifecycle management і безпечні failure paths.

## Tests and deterministic validation

Порівняно з v0.6.0 додано 37 test/fixture paths, зокрема:

- camera observation, baseline, restoration і snapshot harnesses;
- gameplay mode, HorPlus, zoom transition, resolver і camera safety harnesses;
- cinematic aspect/FOV/selection/initialization/recovery harnesses;
- Dialogue FOV, Candidate hardening, policy snapshot, post-cinematic
  exclusion і recovery-rearm harnesses;
- config persistence and hotkey harnesses;
- diagnostics gate and MatchGameplay prediction harnesses;
- platform memory, viewport, instruction validation і signature scanner tests;
- runtime evidence fixtures для gameplay, cinematics, dialogue і transitions;
- `tests/regression/runtime_evidence_replay_harness.cpp`;
- `tests/runner/test_cmd_audit.ps1`.

У repository зафіксовані deterministic invariants для:

- arbitrary aspects і FOV sweeps;
- invalid/NaN input fail-closed behavior;
- exactly-once HorPlus projection;
- transformed-to-native provenance protection;
- stale-state prevention;
- mode transitions;
- Gameplay baseline retention;
- cinematic NativeHorPlus/GameplayHorPlus behavior;
- Dialogue Candidate contradiction handling;
- restoration and recovery paths.

## Build and packaging changes

- Додано `build-diagnostic.cmd`.
- `build.cmd` і diagnostic build розділені за output/profile semantics.
- `test.cmd` став unified deterministic test entrypoint.
- Object/build artifacts організовані під `build-artifacts/`; historical
  research probes/traces винесені з production `src/` у `research/`.
- Частина старих `.obj` і одноразових probe source paths видалена або
  переміщена з active source tree як repository housekeeping.
- Додано документацію для architecture, safety invariants і supported build
  manifest.

## Documentation and research changes

До repository додано або оновлено:

- README/Nexus v0.6/v1-facing configuration and feature descriptions;
- `SUMMARY.md` і `TESTING_AND_RESEARCH.md`;
- `docs/ARCHITECTURE.md`;
- `docs/SAFETY_INVARIANTS.md`;
- `docs/SUPPORTED_BUILD_MANIFEST.md`;
- reports з architecture, security/safety, realtime/lifecycle, persistence,
  performance і tests/harnesses audits;
- reports про Global HorPlus consolidation, GameplayBaseline migration,
  Cinematic ENTER cutover, Dialogue hardening і runtime evidence corpus;
- completed/deferred/rejected task-plan archive;
- додаткові UE4SS/Ghidra/CUE4Parse research tools і runtime probes.

Ці файли є research/history/navigation material і не є production runtime
кодом.

## Що не стало v1 production functionality

- Weapon/viewmodel FOV post-cinematic correction не інтегровано в standalone
  ASI architecture; воно залишається окремою сумісною рекомендацією.
- Subtitle centering/alignment research залишається deferred.
- Native UE reflection bridge і частина Dialogue ground-truth research
  залишаються deferred.
- Cached ENTER numeric guard має статично відому ambiguity, але observed
  double-transform runtime defect не встановлений.
- Performance blocker не встановлений; inclusive runtime benchmark залишається
  окремим measurement concern.
- Custom-windowed limitation стосується native `AspectRecalculation`, а не
  arbitrary-aspect HorPlus загалом.

## Packaging caveat

`release-assets/` не входить до tracked paths, що потрапили у `git diff
v0.6.0`, тому фактичний стан loose release INI/ASI/ZIP потрібно перевіряти
окремо перед пакуванням v1. Цей report описує repository diff і не є release
manifest або доказом остаточного package contents.

## Validation state

У межах попередніх bounded batches repository містить reports про PASS для
deterministic harnesses, production/diagnostic builds і combined runtime
validation. Цей файл не повторює ці команди і не перетворює наявний report на
нову runtime validation.

```yaml
base_revision: v0.6.0
comparison: git diff v0.6.0
changed_paths: 475
production_source_reorganized: YES
global_camera_fov_layer: ADDED
gameplay_horplus: ADDED/INTEGRATED
cinematic_gameplay_horplus: ADDED/INTEGRATED
dialogue_lifecycle_hardening: ADDED/INTEGRATED
diagnostic_gate: ADDED
deterministic_harness_corpus: ADDED
production_diagnostic_build_split: ADDED
weapon_viewmodel_fov_fix: NOT_INTEGRATED
runtime_reexecuted_for_this_report: NO
release_package_revalidated_for_this_report: NO
```
