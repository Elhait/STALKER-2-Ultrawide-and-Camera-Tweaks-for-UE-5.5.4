# Runtime Evidence Regression Corpus — Foundation Task Plan

## Objective

Create a small, deterministic runtime-evidence replay corpus from already
recorded FOV, restoration, cinematic and Dialogue evidence, and run it through
`test.cmd` without simulating UE or changing production behavior.

## Established evidence and current state

- Existing pure harnesses already cover HorPlus, GameplayBaseline, cinematic
  baseline selection, restoration and Dialogue Candidate rules.
- Research reports contain concrete runtime values such as native gameplay FOV
  `112.623`, 32:9 aspect `3.55556`, cinematic authored FOV `90`, HorPlus result
  `126.87`, and GameplayHorPlus result approximately `143.132`.
- Runtime logs themselves are not part of the canonical source tree; fixtures
  will retain only the minimum factual values needed for deterministic checks.

## Approved scope

- Add machine-readable C++ fixture records under `tests/fixtures/runtime/`.
- Add one small replay harness under `tests/regression/`.
- Cover gameplay HorPlus, restoration, NativeHorPlus/GameplayHorPlus cinematic
  behavior and Dialogue Candidate context behavior.
- Add arbitrary-aspect/FOV sweeps and invalid/fail-closed invariants where the
  existing pure APIs make this deterministic.
- Integrate the harness into `test.cmd`.
- Create the required research report.

## Explicit non-goals

- No production behavior or hook changes.
- No UE/runtime simulation, log parser, timestamp/address assertions or giant
  test framework.
- No game launch, commit, release work or unrelated harness rewrites.

## Expected files/areas

- `tests/fixtures/runtime/gameplay/`
- `tests/fixtures/runtime/transitions/`
- `tests/fixtures/runtime/cinematics/`
- `tests/fixtures/runtime/dialogue/`
- `tests/regression/runtime_evidence_replay_harness.cpp`
- `test.cmd`
- `research/reports/RUNTIME_EVIDENCE_REGRESSION_CORPUS_FOUNDATION.md`

## Implementation batches

1. Add minimal fixture records with evidence provenance comments.
2. Add replay harness using existing pure production helpers/stores.
3. Integrate and run focused/full deterministic tests.
4. Build and perform read-only Git review.
5. Write report and stop before runtime.

## Risks and safe failure

- Risk: accidentally turning a recorded pointer/timestamp into a semantic
  contract. Fixtures must test invariants and use opaque source tokens only for
  source-continuity behavior.
- Risk: introducing a second implementation of HorPlus math. The harness must
  call existing production helpers.
- Safe failure: invalid fixture inputs must produce pass-through, fallback or
  rejected decisions through existing APIs.

## Validation and stop conditions

- Focused replay harness PASS.
- Full `test.cmd` PASS.
- `build.cmd` PASS.
- `git diff --check` PASS.
- Stop if production code changes become necessary; extract no new production
  abstraction without a separate bounded task.
- Runtime remains `NOT_PERFORMED`.

## Expected final Git review

Confirm only fixture/test integration, report, task plan archival and required
task-log documentation changed. Preserve all pre-existing user changes.
