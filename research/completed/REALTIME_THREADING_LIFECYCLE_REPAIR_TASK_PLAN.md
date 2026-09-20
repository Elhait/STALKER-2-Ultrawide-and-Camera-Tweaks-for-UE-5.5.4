# Realtime / Threading / Lifecycle Repair Batch — Task Plan

## Objective

Revalidate the supplied R1–R7 findings against the current tree after A2–A6,
then apply only bounded lifecycle/threading/diagnostic repairs that are
supported by current source and deterministic harnesses.

## Established evidence and current state

- A1 capability gates and A2–A6 neutral camera naming are already present.
- Security S1–S7 repairs are present; this batch must not duplicate them.
- Cinematic selection is currently read from runtime atomics at multiple
  callbacks and needs one immutable per-cinematic selection snapshot if the
  current source confirms the generation gap.
- Hook installation uses StartDisabled in relevant cinematic paths, but the
  current tree must be checked for publication gates and gameplay exposure.
- Worker and SafetyHook repairs already exist and require revalidation before
  any further local change.

## Approved scope

- R1 cinematic selection snapshot with deterministic interleaving tests.
- R2 minimal callback publication/stopping gates for confirmed exposed domains.
- R3 join-result-aware controlled shutdown contract.
- R4 bounded vendored SafetyHook transaction-state repair only if current code
  still has the identified ambiguous outcome.
- R5 bounded worker startup cleanup only where current code remains unsafe.
- R6 diagnostic TLS reset repair, diagnostics-only.
- R7 diagnostic snapshot coherence repair only if source proves mixed-generation
  publication.
- Focused harnesses, existing test/build validation, report and task log.

## Explicit non-goals

- No performance optimization, logging redesign or generic hook framework.
- No new UE hooks, Dialogue redesign, lock-free rewrite or architecture rewrite.
- No game launch, commit, release, or speculative synchronization.

## Expected files/areas

- `src/plugin/runtime.cpp`, `src/plugin/worker_lifecycle.*`.
- `external/safetyhook/safetyhook.*` only for a confirmed R4 gap.
- Minimal new camera/config/lifecycle pure helpers and harnesses where needed.
- `test.cmd`, `research/reports/REALTIME_THREADING_LIFECYCLE_REPAIR_BATCH.md`,
  and the runtime matrix for deferred runtime scenarios.

## Batches and validation

1. Reaudit R1–R7 and classify each as confirmed, already fixed, or evidence
   gap. Add no code for unsupported findings.
2. Implement confirmed bounded repairs in dependency order: selection/gates,
   shutdown/worker ownership, SafetyHook transaction, diagnostic-only state.
3. Run focused harnesses, `test.cmd`, `build.cmd`, `build-diagnostic.cmd`, and
   `git diff --check`.
4. Perform read-only Git review, write the report/task log, and archive this
   plan.

## Risks and rollback / safe failure

- A selection snapshot must fall back to the current safe/default policy if a
  lifecycle is incomplete; it must never mix generations.
- Closed gates pass through native behavior and preserve observation-only
  lifecycle support.
- Failed joins retain ownership and report failure; loader-lock detach remains
  non-blocking.
- SafetyHook failures must preserve active ownership or restore bytes; no
  ambiguous disabled state may be claimed.
- Diagnostic-only repairs must not affect production builds.

## Stop conditions and phase gates

- Do not implement a finding whose current-tree evidence is absent.
- Stop if a repair requires OS-level/runtime proof unavailable to harnesses;
  record `PARTIAL` or `GAP` honestly.
- Stop after deterministic validation and report; runtime remains unperformed.

## Final review

Compare changed paths with this plan, separate pre-existing staged/dirty work,
record each R-status and validation limit, append `backlog/TASKLOG.md`, and move
this plan to `research/completed/`.
