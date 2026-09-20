# Persistence / Runtime State / Recovery Repair — Task Plan

## Objective

Close the four current-tree state-coherence findings from the persistence and
runtime-state audit without changing recovery heuristics or introducing a new
configuration/state framework.

## Established evidence and current state

- Cinematic selection snapshot capture and gates exist, but current consumers
  still need source-level verification for active-snapshot authority.
- The replaced cinematic aspect instruction is a native aspect-only store;
  the current closed-gate fallback must be checked for extra flag writes.
- Dialogue context invalidation exists on AspectRecalculation and must be
  compared with the HorPlus path.
- Config loading uses sequential last-valid occurrence semantics; persistence
  and template synchronization require duplicate-key verification.
- Recovery-liveness concerns are runtime hypotheses and are explicitly outside
  this repair.

## Approved scope

- P1 accepted cinematic snapshot authority and duplicate-ENTER ordering.
- P2 exact native aspect-only fallback for closed/stopping/invalid gates.
- P3 shared native gameplay context invalidation before mode-specific behavior.
- P4 deterministic managed duplicate-key persistence semantics and round-trip
  harnesses.
- Only obvious local cleanup if deterministic evidence proves it safe.

## Explicit non-goals

- No recovery timeout/generation repair, Dialogue heuristic redesign, ZOOM
  change, camera math change, new hook, config framework, or architecture
  rewrite.
- No game launch, commit, release, or speculative OS synchronization.

## Expected files/areas

- `src/plugin/runtime.cpp`, `src/cinematics/*`, `src/gameplay/*`.
- `src/config/config_repository.*`, `src/config/config_template.*`.
- Focused cinematic, Dialogue and config harnesses; `test.cmd`/`build.cmd`.
- `research/reports/PERSISTENCE_RUNTIME_STATE_RECOVERY_REPAIR_BATCH.md` and
  the runtime matrix.

## Batches and validation

1. Reconfirm P1–P4 against current source and preserve already-fixed gates.
2. Implement P1/P2/P3 bounded production repairs.
3. Implement P4 canonical managed-config persistence and round-trip tests.
4. Run focused harnesses, corpus, `test.cmd`, production/diagnostic builds and
   `git diff --check`.
5. Perform read-only Git review, write report/task log, archive this plan.

## Risks and safe failure

- A rejected/duplicate ENTER must not change an active snapshot.
- Closed cinematic aspect hooks must perform only the original aspect store.
- Dialogue invalidation must use native/current context before HorPlus output.
- Persistence must preserve last-known-good files and make reported success
  equal to the value loaded after restart.
- Recovery-liveness semantics remain unchanged.

## Stop conditions

- Do not repair a finding contradicted by current source.
- Do not add timeout or invalidation behavior for runtime-only hypotheses.
- Stop after deterministic validation and report; runtime remains unperformed.

## Final review

Compare changed paths with this plan, preserve pre-existing staged/dirty work,
record remaining runtime evidence and archive this plan under
`research/completed`.
