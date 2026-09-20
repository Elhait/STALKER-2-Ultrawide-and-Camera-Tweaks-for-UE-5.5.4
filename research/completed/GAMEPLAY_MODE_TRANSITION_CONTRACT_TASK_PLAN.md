# Task 7 — Gameplay Mode Transition Contract

## Objective

Make runtime gameplay mode changes explicit and deterministic in both
directions: `AspectRecalculation <-> HorPlus`.

## Established evidence and current state

- The runtime mode is selected by config/hotkey and the gameplay writer already
  dispatches to `HorPlus` or `AspectRecalculation`.
- AspectRecalculation owns replay/staged aspect state; HorPlus owns its own
  gameplay FOV application path.
- A mode enum change alone does not currently define cleanup/invalidation for
  the previous mode's live state.
- Dialogue Tasks 4–6 and ZOOM are independent contracts.

## Approved scope

- Audit current mode storage, hotkey/config flow, ReplayState, source/aspect
  observations, readiness and HorPlus diagnostic state.
- Add an explicit mode transition boundary with separate A->H and H->A
  cleanup semantics.
- Preserve startup semantics and make same-mode selection a no-op.
- Defer physical gameplay transition while cinematic ownership is active if
  immediate application is not established as safe.
- Add focused mode-transition harness coverage and transition-only logging.

## Explicit non-goals

- No HorPlus math or gameplay writer ownership change.
- No exact-aspect whitelist or forced cinematic target reuse.
- No Dialogue, ZOOM, Cinematics policy, CinematicActive guard or
  AspectRecalculation normalization redesign.
- No unrelated performance work, game launch, Git commit/release or stable
  ASI replacement.

## Expected files/areas

- `src/plugin/runtime.cpp` for mode selection/transition dispatch and state
  cleanup.
- Existing gameplay state/aspect helpers only if required by the audit.
- `tests/gameplay/` and `test.cmd` for focused transition coverage.
- `backlog/TASKLOG.md` after implementation and review.

## Batches and validation

1. Audit current mode transition flow and define A->H/H->A postconditions.
2. Implement the smallest explicit transition boundary, including safe
   cinematic deferral if required by current ownership.
3. Add harness cases for startup, both directions, staged state, repeated
   switching, no-op, 16:9, arbitrary aspects, invalid aspect and cinematic
   deferral semantics.
4. Run full `test.cmd`, combined diagnostic build, normal production compile
   and `git diff --check`.
5. Perform read-only Git review, archive this plan and update `TASKLOG.md`.

## Risks and safe failure

- Never write a guessed aspect during a transition.
- Preserve native state and defer when the actual runtime aspect is invalid or
  coordinator ownership is not a valid gameplay boundary.
- Do not reset unrelated Dialogue or cinematic lifecycle state.

## Stop conditions and phase gates

- If an immediate physical A->H restore cannot be established safely, record
  the precise partial status instead of inventing a fallback.
- Stop after Task 7; do not begin Task 8 automatically.
- Runtime validation remains deferred to the combined regression session.

## Expected final Git review

Confirm changed paths match this plan, identify untouched subsystems and
separate static/harness/build evidence from runtime status.
