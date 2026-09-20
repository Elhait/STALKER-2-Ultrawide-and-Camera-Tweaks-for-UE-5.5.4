# V1 Remaining Work Inventory + Camera Architecture Cleanup

## Objective

Record the supplied Global Hor+ runtime milestone, audit current camera/FOV
state ownership and diagnostics, remove only unambiguous obsolete debt, and
define one bounded next functional pack for v1.

## Established evidence and current state

- The user-provided combined runtime session reports PASS for Global Hor+,
  Gameplay/Native/Gameplay cinematic modes, F11/F12, Auto/forced aspect,
  ADS/binocular, Dialogue coexistence, save/load/camera recreation and legacy
  aspect cleanup.
- The current repository contains the unified transform, GameplayBaseline,
  observation and restoration layers, plus a Runtime Evidence Replay corpus.
- The cached ENTER numeric guard remains an ambiguity without an observed
  runtime defect and is explicitly out of cleanup scope.

## Approved scope

- Update durable reports/evidence to distinguish supplied runtime results from
  static/test evidence.
- Audit FOV/camera globals, diagnostic prediction state, transitional state,
  duplicate telemetry and old names/contracts.
- Remove only state or code proven to have no production, diagnostic or test
  consumer.
- Add or update deterministic tests only where cleanup requires protection.
- Produce the V1 remaining-work inventory and one proposed next pack.

## Explicit non-goals

- No speculative architecture rewrite or new hook.
- No new gameplay/cinematic behavior.
- No deletion of UNKNOWN state.
- No replacement of the cached ENTER numeric guard.
- No Dialogue redesign, release work, commit or game launch.

## Expected files/areas

- `src/plugin/runtime.cpp` and directly referenced camera/diagnostic headers;
- existing focused harnesses and `test.cmd`, only if cleanup requires it;
- `research/reports/V1_REMAINING_WORK_INVENTORY.md`;
- `research/reports/CAMERA_ARCHITECTURE_CLEANUP.md`;
- runtime evidence fixtures/reports and `backlog/TASKLOG.md`.

## Batches

1. Repository-wide state/dataflow inventory and runtime milestone update.
2. Bounded cleanup of only unambiguous obsolete code, if found.
3. Focused tests, full test/build/diff validation.
4. Final reports, task-log entry, read-only Git review, and plan archival.

## Risks and safe failure

- Diagnostic state may still be needed for the combined runtime corpus; retain
  it unless every consumer is accounted for.
- Do not infer that a field is obsolete from a name, a write, or a report that
  predates the current production cutover.
- If ownership or runtime relevance is UNKNOWN, leave the code untouched and
  classify it for later review.

## Stop conditions and phase gates

- Stop after inventory, bounded cleanup, deterministic validation and reports.
- Any need for new runtime evidence, reverse engineering, hooks or behavior
  changes becomes the proposed next pack, not an expansion of this task.

## Final review

- Compare actual changed paths with this plan.
- Record completed, remaining, deferred and not-runtime-validated items.
- Archive this plan under `research/completed/` only after validation.
