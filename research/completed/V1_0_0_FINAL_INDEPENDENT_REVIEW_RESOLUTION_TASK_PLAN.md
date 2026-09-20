# v1.0.0 Final Independent Review Resolution Task Plan

## Objective

Resolve the bounded A–K findings from the final independent review using the
smallest justified production, test-infrastructure, repository, or
documentation changes before architecture freeze.

## Established evidence and current state

- The feature set and validated gameplay/cinematic/dialogue behavior are frozen.
- Previous architecture and safety hardening batches passed their available
  build and harness validation, but final Steam 2.0.5 in-game regression has
  not been rerun after the latest changes.
- The independent review identified factual contract gaps, test/repository
  hygiene gaps, and two bounded safety candidates; it did not justify CMake,
  DI, a new coordinator class, transactional HookSet, hot unload, or broad
  subsystem rewrites.
- The current checkout contains substantial prior uncommitted work. That work
  is preserved and is outside this plan unless a listed path must be updated.

## Approved scope

- A: reconcile `RuntimeState` ownership naming and documentation.
- B: reconcile the executable SHA-256 identity contract with production code.
- C: establish the actual `WorkerLifecycle` owner-thread contract and make the
  self-join harness evidence truthful; fix only a reachable ordering defect.
- D: verify/enforce the documented process-resident lifetime model without
  adding hot unload.
- E: document callback thread-affinity and mutable-field concurrency
  invariants; fix only a confirmed reachable race.
- F: define a warning policy for owned production source.
- G: add one repository-native Windows test entry point.
- H: remove tracked generated harness executables while preserving source and
  intentional release/history artifacts.
- I: add a human-readable supported-build/resolver manifest.
- J: document trusted-input preconditions for resolver primitives.
- K: add the bounded gameplay write overflow guard if the current paths are
  semantically equivalent.

## Explicit non-goals

- No new user-facing features or reverse-engineering research.
- No game launch or injected ASI runtime test.
- No CMake/MSBuild migration, DI framework, artificial coordinator class,
  transactional HookSet rewrite, general PE parser, hot `FreeLibrary` support,
  or broad RAII rewrite.
- No changes to validated FOV/aspect/dialogue policies, resolver matching
  semantics, hook ordering, configuration defaults, or release behavior except
  a directly justified safety correction.

## Expected files and areas

- `src/plugin/runtime.*`, `src/plugin/worker_lifecycle.*`
- `src/platform/win32/memory.*`
- `src/gameplay/gameplay_camera.*`
- `src/hooks/signature_scanner.*`
- `build.cmd`, `.gitignore`, `tests/`, and a new repository test runner
- `docs/ARCHITECTURE.md`, `docs/SAFETY_INVARIANTS.md`,
  `docs/UPDATING_GAME_VERSION.md`, and a supported-build manifest
- relevant review/closure report and `backlog/TASKLOG.md`

## Batches

1. Read-only current-flow verification for A–K and exact tracked-output
   inventory.
2. Minimal source and contract corrections for A–F, J, and K where justified.
3. Test runner, harness evidence correction, and tracked-output cleanup for G
   and H.
4. Supported-build/resolver manifest and documentation synchronization for I.
5. Build, unified tests, individual harnesses, Git/output review, and factual
   consistency review.

## Validation

- Production `build.cmd` succeeds.
- Unified test entry point succeeds and returns non-zero on failure.
- WorkerLifecycle, FeatureStatus, ConfigPersistence, and platform-memory
  harnesses pass individually.
- `git diff --check` passes.
- `git ls-files` contains no generated harness `.exe` or `.obj` outputs in
  approved build-output areas.
- Documentation matches the actual source contracts.
- The game is not launched; runtime regression remains a later Batch 4 gate.

## Risks and safe failure

- Preserve all unrelated user changes and stop if an approved finding cannot be
  resolved without changing frozen feature behavior.
- Do not delete ambiguous binaries or evidence. Remove only confirmed tracked
  generated harness outputs within the approved scope.
- If a source change exposes a new concrete architecture blocker, document it
  separately and stop rather than expanding this plan.
- If warning enforcement requires broad cosmetic changes, keep the policy
  documented without introducing unrelated cleanup.

## Stop conditions and phase gates

- Stop before implementation if current control flow contradicts the review
  premise and the smallest safe disposition is not clear.
- Stop after validation and report A–K dispositions; do not declare
  `ARCHITECTURE FREEZE` automatically.
- Do not begin Batch 4 runtime regression within this task.

## Final Git review

Before reporting completion, inspect status, affected-path diff, tracked output
inventory, and recent history. Report completed, deferred, blocked, and not
runtime-validated items separately, and record the bounded result in
`backlog/TASKLOG.md` only after the implementation and validation match this
plan.
