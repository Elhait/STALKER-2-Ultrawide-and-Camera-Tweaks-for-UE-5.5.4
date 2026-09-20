# Architecture A2–A6 Cleanup — Task Plan

## Objective

Apply only evidence-backed structural, naming, documentation, and thread-affinity inventory changes from the Architecture audit after A1 and the Security/Safety batch.

## Established evidence and current state

- A1 added explicit Dialogue lifecycle capability gating.
- Security/Safety S1–S7 repairs are present, with documented partial validation where applicable.
- `runtime.cpp` remains the production integration/lifetime boundary.
- Architecture audit identifies possible ownership mismatches for coordinator and shared HorPlus math, plus documentation and evidence gaps.

## Approved scope

- Audit confirmed extraction seams and perform only cohesive, behavior-neutral extraction.
- Correct misleading Coordinator/HorPlus placement and names only when source confirms the mismatch.
- Correct factual RuntimeState documentation.
- Produce source-only callback thread-affinity inventory without adding synchronization.
- Verify A6 documentation/tests do not overstate heuristic Dialogue classification.
- Update the A2–A6 report and task log.

## Explicit non-goals

- No Dialogue classifier redesign or threshold change.
- No new hook, mutex/atomic added speculatively, SafetyHook work, performance work, or runtime behavior change.
- No generic framework, full `runtime.cpp` rewrite, game launch, commit, or release.

## Expected files/areas

- `src/runtime.cpp` integration and relevant domain headers/sources.
- `src/gameplay/*`, `src/cinematics/*`, `src/dialogue/*`, tests, and factual docs.
- `research/reports/ARCHITECTURE_A2_A6_CLEANUP.md`.

## Batches and validation

1. Read-only source/dataflow audit for A2–A6 and identify safe/no-change findings.
2. Apply only confirmed naming/documentation/extraction changes.
3. Run focused harnesses, Runtime Evidence Replay, `test.cmd`, production and diagnostic builds, and `git diff --check`.
4. Perform read-only Git review and archive this plan under `research/completed/`.

## Risks and safe failure

- If an extraction changes lifecycle ownership or behavior, do not perform it;
  record `NO_CHANGE` or `GAP`.
- If ownership is shared rather than domain-specific, use a neutral name/path
  only when the migration is mechanical and semantics remain identical.
- Thread-affinity uncertainty is recorded, not “fixed” speculatively.

## Stop conditions

- Stop each finding independently when evidence is insufficient or behavior
  would change.
- Stop after deterministic validation and the report; runtime remains not performed.

## Final review

Compare changed paths with this plan, separate pre-existing worktree changes,
record A2–A6 statuses and limits, append `TASKLOG.md`, and archive the plan.
