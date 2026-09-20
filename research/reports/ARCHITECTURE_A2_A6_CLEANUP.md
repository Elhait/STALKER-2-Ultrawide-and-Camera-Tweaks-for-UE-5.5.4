# Architecture A2–A6 Cleanup

## Scope

This bounded batch followed the architecture audit after A1 and the safety
batch. It corrected only confirmed shared-domain naming, kept the runtime
integration boundary intact, corrected one factual documentation statement,
and recorded source-level callback affinity limits. No game launch was
performed.

## A2 — runtime integration seams

`src/plugin/runtime.cpp` remains the intentional composition and callback
integration boundary. Existing cohesive seams are already extracted into
feature-status, cinematic-initialization, camera-state, restoration, baseline,
and observation modules. Further extraction of callbacks would move lifecycle
ownership or require a new framework, so no additional A2 extraction was made.

Status: `NO_CHANGE` — the bounded audit found no additional behavior-neutral
seam whose ownership was clearer than the current integration boundary.

## A3 — ownership and naming corrections

Two source-confirmed mismatches were corrected without changing behavior:

| Former location/name | Current location/name | Evidence |
|---|---|---|
| `gameplay::CoordinatorState` in `src/gameplay/gameplay_state.*` | `camera::CoordinatorState` in `src/camera/presentation_state.*` | It coordinates Gameplay, CinematicActive and CinematicExiting across Gameplay, Cinematic, Dialogue and recovery callsites. |
| `cinematics::HorPlus` in `src/cinematics/cinematic_fov.*` | `camera::HorPlus` in `src/camera/horplus.*` | The same tangent-space projection is called by cinematic transforms and `gameplay::EvaluateHorPlus()`. |

Transition policy helpers remain in `gameplay` because their decision is still
gameplay-mode logic; they now accept the neutral camera presentation type. The
cinematic module still owns cinematic policy and transfer logic and delegates
the shared projection primitive to `camera`.

No duplicate implementations were retained. Tests and build entrypoints were
updated to link the new translation units.

Status: `FIXED`.

## A4 — factual documentation

`docs/ARCHITECTURE.md` now describes the translation-unit-private `RuntimeState`
as the primary integration/lifetime owner, rather than claiming that it is the
single owner of every process-resident production state object.

Status: `FIXED`.

## A5 — callback affinity inventory

The source provides synchronization and lifecycle mechanisms, but does not
establish an engine contract that native callbacks execute on one specific
thread. The native hook groups `ReplayManualTransition`/
`ApplyPendingGameplayModeTransition`, cinematic enter/exit/aspect callbacks,
`TraceDialogueBoundary`, and `TraceZoomIn`/`TraceZoomOut` are therefore
`THREAD_AFFINITY_NOT_ESTABLISHED` from source alone. `HotkeyLoop` and
`WorkerLifecycle::ThreadStartThunk` have explicit worker-thread creation and
stop-event handling, but this does not prove native callback affinity.

No speculative locks, thread assertions, or synchronization changes were
introduced. A stronger contract requires runtime/native evidence or an
authoritative engine callback guarantee.

## A6 — Dialogue classifier honesty

No classifier redesign was made. Current source/tests and existing reports
describe Candidate/Active as a bounded FOV-observation heuristic with
source/target hardening, not as game-owned Dialogue ground truth. A1
capability gates remain unchanged.

Status: `ACCEPTED_HEURISTIC_RISK`; no documentation correction was required.

## Relevant callsites after the change

- `src/plugin/runtime.cpp`: cinematic, gameplay, Dialogue and ZOOM integration
  use the neutral camera state/projection names.
- `src/gameplay/gameplay_state.*`: transition planning remains gameplay policy
  while using `camera::CoordinatorState`.
- `src/cinematics/cinematic_fov.*` and `src/gameplay/horplus_gameplay.cpp` use
  the same `camera::HorPlus` primitive.

## Validation

- `test.cmd`: `PASS` — all focused, dialogue, camera, cinematic, safety and
  runtime-evidence harnesses passed.
- `build.cmd`: `PASS`.
- `build-diagnostic.cmd`: `PASS`.
- `git diff --check`: required after final edits.
- Runtime: `NOT_PERFORMED`.

The builds emit the pre-existing vendor Zydis C4201 warnings; no new project
errors were observed.

## Manifest

```yaml
A2:
  status: NO_CHANGE
  extracted_seams: []
  behavior: NO_CHANGE
A3:
  status: FIXED
  ownership_naming: camera::CoordinatorState and camera::HorPlus
A4:
  status: FIXED
  canonical_wording: primary integration/lifetime owner
A5:
  status: THREAD_AFFINITY_NOT_ESTABLISHED
  production_change: NO
A6:
  status: ACCEPTED_HEURISTIC_RISK
  classifier_behavior_changed: NO
test_cmd: PASS
production_build: PASS
diagnostic_build: PASS
diff_check: PASS
runtime: NOT_PERFORMED
```
