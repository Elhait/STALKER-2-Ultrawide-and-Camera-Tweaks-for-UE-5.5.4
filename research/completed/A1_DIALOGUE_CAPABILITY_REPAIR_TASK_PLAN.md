# A1 Dialogue Capability Repair — Task Plan

## Objective

Make Dialogue lifecycle capability dependencies explicit and fail closed when non-Native Dialogue lacks the cinematic lifecycle observation or Gameplay recovery observation it requires.

## Established evidence and current state

- Cinematic EXIT arms post-cinematic Dialogue exclusion.
- Recovery release observation is currently performed from the Gameplay writer callback.
- Existing installation predicates can expose Dialogue without both observation paths.
- No safe alternate recovery seam has been established; no new UE hook is approved.

## Approved scope

- Add a pure capability decision contract and deterministic matrix coverage.
- Install cinematic FOV lifecycle observation independently of cinematic presentation intervention when required.
- Gate non-Native Dialogue on cinematic lifecycle observation and Gameplay recovery observation.
- Keep Native Dialogue independent and preserve unrelated feature isolation.
- Correct the directly affected safety/architecture documentation and add A1 reports/runtime matrix.

## Explicit non-goals

- No new native hook or resolver.
- No Dialogue classifier, recovery predicate, ZOOM, Cinematic math, Gameplay mode, or persistence redesign.
- No speculative alternate recovery seam.
- No game launch, commit, release, or unrelated cleanup.

## Expected files/areas

- `src/plugin/feature_status.*`
- `src/plugin/runtime.cpp`
- `tests/feature_status/feature_status_harness.cpp`
- affected Dialogue/Cinematics documentation
- `research/reports/ARCHITECTURE_A1_DIALOGUE_CAPABILITY_REPAIR.md`
- `research/reports/ARCHITECTURE_A1_RUNTIME_MATRIX.md`

## Batches and validation

1. Capability contract and initialization wiring; focused feature-status and existing lifecycle harnesses.
2. Documentation and reports; `test.cmd`, production build, diagnostic build, and `git diff --check`.
3. Read-only Git review against this plan; archive this plan in `research/completed/`.

## Risks and safe failure

- If a required observer cannot be installed, non-Native Dialogue remains unavailable and unrelated features retain their own status.
- Observation-only cinematic installation must not enable presentation intervention.
- Existing Native Dialogue behavior remains available without the new dependency.

## Stop conditions

- Stop if implementation requires a new hook, changes an unrelated state machine, or reveals an unestablished recovery contract.
- Stop after reports, validation, and read-only Git review. Runtime remains not performed.

## Final review

Review changed paths against this plan, distinguish pre-existing worktree changes from A1 changes, record validation limits, and add the bounded result to `backlog/TASKLOG.md`.
