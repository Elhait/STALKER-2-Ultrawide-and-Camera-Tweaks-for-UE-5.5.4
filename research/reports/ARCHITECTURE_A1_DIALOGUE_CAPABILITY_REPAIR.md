# Architecture A1 — Dialogue Lifecycle Capability Repair

## Scope

This bounded repair makes Dialogue lifecycle dependencies explicit. It does
not add a native hook and does not change Dialogue classification, recovery
completion, ZOOM, cinematic math, Gameplay mode, or persistence semantics.

## Source confirmation

The pre-repair source establishes the dependency chain:

- `src/plugin/runtime.cpp:1972-1979` — `TraceCinematicExit()` arms
  `PostCinematicDialogueExclusion`.
- `src/plugin/runtime.cpp:456-465` — the recovery observer consumes Gameplay
  writer context and feeds that exclusion.
- `src/plugin/runtime.cpp:2947-2956` — `ReplayManualTransition()` is the
  production caller of the recovery observer.
- `src/plugin/runtime.cpp:2031-2039` — cinematic FOV ENTER/EXIT hooks are the
  available lifecycle observation seam.
- `src/plugin/runtime.cpp:2400` — Dialogue boundary installation is separate
  from those lifecycle hooks.

Therefore Dialogue's non-Native lifecycle contract cannot be represented by
Dialogue-boundary installation alone.

## Implemented contract

`plugin::DialogueLifecycleCapabilityAvailable()` is a pure capability rule:

```text
Native Dialogue                         -> available without dependencies
Non-Native Dialogue + cinematic observe
                         + Gameplay recovery observe -> available
Any missing required capability          -> unavailable (fail closed)
```

Initialization now:

1. Initializes requested cinematic presentation transactionally as before.
2. When presentation intervention is not requested but non-Native Dialogue or
   hotkeys require lifecycle observation, installs only the cinematic FOV
   ENTER/EXIT observation component. Aspect intervention remains disabled.
3. Publishes the lifecycle-observation capability separately.
4. Installs the Gameplay writer/recovery observer as before.
5. Installs Dialogue only when its current policy has the required capability.
6. Rejects a hotkey transition into a non-Native Dialogue policy when the
   capability is unavailable.

No new UE hook or alternate recovery seam was introduced.

## Capability matrix

| Dialogue policy | Cinematic lifecycle observation | Gameplay recovery observation | Result |
|---|---:|---:|---|
| Native | any | any | available; no lifecycle dependency |
| non-Native | yes | yes | available |
| non-Native | no | yes | unavailable; fail closed |
| non-Native | yes | no | unavailable; fail closed |
| non-Native | no | no | unavailable; fail closed |

Unrelated Gameplay/Cinematics status remains independently reported. The
observation-only cinematic path does not enable cinematic presentation
intervention.

## Validation

- Feature-status capability truth table: PASS.
- Existing post-cinematic exclusion lifecycle harness: PASS.
- Existing recovery-rearm harness: PASS.
- Full deterministic `test.cmd`: PASS.
- Production `build.cmd`: PASS.
- Diagnostic `build-diagnostic.cmd`: PASS.
- `git diff --check`: PASS.
- Runtime/game launch: NOT PERFORMED.

Builds emitted the existing vendor Zydis C4201 warnings only; no A1 compiler
errors occurred.

## Runtime acceptance matrix

See `ARCHITECTURE_A1_RUNTIME_MATRIX.md`. Runtime validation remains deferred to
the next planned game session because this task changes installation and
capability behavior, not a pure calculation.

## Final verdict

```yaml
cinematic_lifecycle_dependency: EXPLICIT
gameplay_recovery_dependency: EXPLICIT
cinematic_native_dialogue_non_native: PASS
gameplay_disabled_dialogue_non_native: FAIL_CLOSED
gameplay_resolver_failure: FAIL_CLOSED_FOR_NON_NATIVE_DIALOGUE
cinematic_resolver_failure: FAIL_CLOSED_FOR_NON_NATIVE_DIALOGUE
full_configuration: PASS
observation_vs_intervention_split: PASS
dialogue_fail_closed_contract: PASS
unrelated_feature_failure_isolation: PASS
tests: PASS
production_build: PASS
diagnostic_build: PASS
runtime: NOT_PERFORMED
```
