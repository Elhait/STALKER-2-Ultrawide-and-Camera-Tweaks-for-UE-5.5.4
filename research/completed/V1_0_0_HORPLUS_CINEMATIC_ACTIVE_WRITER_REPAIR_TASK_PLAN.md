# v1.0.0 HorPlus Cinematic-Active Writer Repair — Task Plan

## Objective

Make HorPlus preserve the validated cinematic FOV transform while the
cinematic is active, using the existing gameplay writer boundary and without
changing the Cinematics subsystem.

## Established evidence and current state

- Runtime diagnostic confirms cinematic ENTER computes `90 -> 126.87`.
- During `CinematicActive`, the validated gameplay writer receives a separate
  native `XMM0=90` with aspect `3.55556` and flags `0x5`.
- The diagnostic marks HorPlus eligible but the production helper applies
  nothing because it currently accepts only `CoordinatorState::Gameplay`.
- After EXIT, the coordinator is `Gameplay` and HorPlus applies immediately;
  the user observes correct framing with no transition.

## Approved scope

- Allow the existing HorPlus writer transformation in both `Gameplay` and
  `CinematicActive` coordinator states.
- Keep the same native-input transformation, eligibility and single-writer
  ownership.
- Preserve the existing `AspectRecalculation` branch and all Cinematics hooks.
- Retain the current diagnostic trace for one confirmation run.

## Explicit non-goals

- Do not change cinematic FOV math, resolver, aspect store or EXIT logic.
- Do not change AspectRecalculation, Dialogue, scanner, flags or config.
- Do not add hooks, timers, caches, projection-owner research or new RE
  anchors.
- Do not remove diagnostic instrumentation before runtime confirmation.

## Expected files or areas

- `src/plugin/runtime.cpp`
- Existing diagnostic output only; no new production configuration.

## Implementation and validation batches

1. Change only the HorPlus coordinator-state gate.
2. Run existing harnesses, production build, diagnostic build and
   `git diff --check`; do not launch the game.
3. User performs one HorPlus cinematic runtime test.

## Risks and safe failure

- Risk: applying HorPlus to a non-gameplay FOV source. The permitted state is
  limited to the validated `CinematicActive` and `Gameplay` states and keeps
  the existing aspect/flags/input validation.
- No transformed value is stored as a new base; each invocation uses native
  writer input.

## Stop conditions and phase gates

- Stop after the two builds and static checks.
- Do not make further cinematic changes based on one result.
- Runtime success is not assumed until the user confirms visual cinematic FOV.

## Expected final Git review

- Confirm one production condition changed and diagnostics remain gated.
- Confirm AspectRecalculation and Cinematics implementation are untouched.
- Record runtime validation as pending until the user reports the test.
