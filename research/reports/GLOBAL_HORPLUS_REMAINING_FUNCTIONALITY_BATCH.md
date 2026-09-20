# Global Hor+ — Remaining Functionality Batch

Date: 2026-09-20

## Scope and outcome

This batch completed the repository-testable remainder of the Global Hor+
consolidation. It did not add hooks or change production FOV behavior. The
existing unified transforms, GameplayBaseline ownership, cached ENTER guard,
Dialogue isolation and AspectRecalculation restoration contract remain intact.

## Remaining-gap inventory

### IMPLEMENT_NOW — completed

- Dynamic-aspect continuity for native gameplay observations and retained
  GameplayBaseline state.
- Gameplay mode transition coverage for `HorPlus` and
  `AspectRecalculation`.
- Cinematic `NativeHorPlus` / `GameplayHorPlus` mode coverage.
- Cinematic aspect-policy cycle coverage (`Auto`, `Native`, forced `16:9`,
  forced `21:9`, forced `32:9`).
- Arbitrary-aspect and FOV sweeps, including noncanonical aspects and invalid
  NaN/Inf inputs.
- Stale transformed-evidence and cross-state baseline-contamination negatives.
- Runtime Evidence Replay corpus extension through `test.cmd`.

### NEEDS_RUNTIME_EVIDENCE

- Actual resolver installation, native callback ordering and executable
  identity for the combined build.
- Visual ADS/binocular and cinematic framing at the selected display aspect.
- Live F11 mode switching and F12 cinematic FovMode switching without reload.
- Auto versus forced cinematic aspect behavior at the real display/client
  viewport.
- Save/load or camera recreation provenance and recovery behavior.
- Dialogue coexistence and post-cinematic recovery in the combined session.
- The focused regression after legacy aspect-state cleanup.

### NEEDS_NEW_RESEARCH

None for the currently established Global Hor+ production contracts. Any new
UE ownership model, new hook, or replacement of the cached ENTER numeric guard
would be a separate research task.

### OPTIONAL_CLEANUP

Diagnostic-only MatchGameplay prediction state and historical telemetry may be
reviewed later. No deletion is justified by this batch; production/diagnostic
responsibilities must be audited separately after runtime validation.

## Deterministic coverage added

`runtime_evidence_replay_harness.cpp` now covers:

- native/aspect HorPlus sweeps across arbitrary aspects;
- supported and unsupported flag behavior;
- finite/NaN/Inf fail-closed behavior;
- changing-aspect Gameplay writer observations;
- GameplayBaseline preservation, pass-through clearing and transformed-input
  rejection;
- `HorPlus` / `AspectRecalculation` transition decisions;
- cinematic FOV mode math across authored FOV and aspect sweeps;
- invalid cinematic reference fail-closed behavior;
- exactly-one projection and no transformed-to-native reconstruction in the
  replayed contracts.

`config_persistence_harness.cpp` now verifies the complete cinematic
aspect-policy cycle in addition to the FovMode cycle and F12 configuration.

The tests assert invariants rather than timestamps, addresses or sequence
numbers. They do not simulate UE or hook installation.

## Runtime matrix

`GLOBAL_HORPLUS_RUNTIME_MATRIX.md` is the single combined runtime gate. It
removes broad FOV/aspect sweeps now covered by `test.cmd` and retains only
resolver, visual, live-switching, save/load, Dialogue coexistence and cleanup
checks that require the game.

## Validation

```yaml
implement_now_items: dynamic_aspect_and_deterministic_global_horplus_coverage
implemented: YES
deferred_runtime_evidence: resolver_visual_live_switch_save_load_dialogue_cleanup
deferred_research: NONE
dynamic_aspect_continuity: PASS
gameplay_mode_switching: PASS
cinematic_fov_mode_switching: PASS
cinematic_aspect_policy: PASS
exactly_once_transform: PASS
stale_state_protection: PASS
arbitrary_aspects: PASS
evidence_corpus_extended: YES
tests: PASS
build: PASS
runtime: NOT_PERFORMED
```

Build emitted only the known external Zydis C4201 unnamed-struct warnings.
`git diff --check` passed with normal line-ending normalization warnings.

## Explicit non-goals

- No production source behavior change.
- No new hook or resolver.
- No cached ENTER numeric-guard replacement.
- No Dialogue/ZOOM redesign.
- No deletion of transitional or diagnostic state.
- No game launch, commit or release.
