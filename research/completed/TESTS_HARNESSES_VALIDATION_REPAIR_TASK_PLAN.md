# Tests / Harnesses / Validation — Production Contract Coverage Task Plan

## Objective

Repair the unified deterministic test runner and add focused coverage for
currently untested production contracts in cinematic aspect handling, Dialogue
FOV math, instruction/span safety, gameplay camera writes/resolution, and
Windows writability checks.

## Established evidence and current state

- The independent validation audit found a concrete runner gap: cinematic
  selection was compiled but not executed, and one compile error check was
  missing.
- `cinematic_aspect.cpp` and `dialogue_fov.cpp` were not linked by any
  deterministic harness.
- Existing safety harnesses cover only subsets of the declared helper
  contracts.
- The repository intentionally uses lightweight standalone C++ harnesses and
  does not need a new test framework or UE simulation.

## Approved scope

- Make `test.cmd` compile/run/result propagation authoritative and add a
  machine-checkable runner inventory.
- Add focused cinematic aspect and Dialogue FOV harnesses.
- Extend instruction/rel32/span, gameplay camera write/resolver, and
  `IsWritable` coverage only for existing production contracts.
- Add metadata-only provenance fields to runtime fixtures only when evidence is
  available; preserve unknown provenance as unknown.
- Correct misleading test names/assertions without expanding runtime claims.

## Explicit non-goals

- No UE/runtime simulation, new hooks, CMake/CTest migration, CI framework,
  performance work, visual proof, callback-order proof, or game launch.
- No speculative production refactor. Any seam extraction must be small and
  behavior-neutral.

## Expected files/areas

- `test.cmd` and a small runner self-audit helper under `tests/`.
- `tests/cinematics`, `tests/dialogue`, `tests/platform`, `tests/gameplay`.
- Existing production helpers in `src/cinematics`, `src/dialogue`, `src/hooks`,
  `src/gameplay`, and `src/platform/win32` only as required for testability.
- `research/reports/TESTS_HARNESSES_VALIDATION_REPAIR_BATCH.md`.

## Batches and validation

1. Inventory runner registrations and repair compile/run error propagation.
2. Add cinematic aspect and Dialogue FOV production-contract harnesses.
3. Expand instruction/span, camera write/resolver and writability safety tests.
4. Correct misleading claims and add bounded fixture provenance metadata.
5. Run the self-audit, `test.cmd`, production/diagnostic builds and diff check.
6. Review the changed paths against this plan and write the report/task log.

## Risks and safe failure

- Tests must assert observable contracts and must not duplicate production
  formulas as expected values.
- Negative fixtures must remain within allocated buffers.
- If a resolver contract cannot be safely exercised without a broad refactor,
  leave that sub-coverage explicitly partial.
- A runner registration error must return non-zero before any green result is
  reported.

## Stop conditions

- Do not introduce a test framework or emulate UE.
- Do not claim runtime/hook/visual/performance coverage from deterministic
  tests.
- Stop after validation and report; runtime remains unperformed.

## Final review

Compare actual changed paths with this plan, preserve pre-existing staged/dirty
work, record remaining partial coverage, append the task log, and archive this
plan under `research/completed/`.
