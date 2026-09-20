# v1.0.0 Final Quality Hardening — Task Plan

## Objective

Close the two changes justified by the final Architecture Quality Pass:

1. require explicitly readable committed memory before `ReadMemory()` copies;
2. correct FeatureStatus harness evidence wording to its actual enum-level
   scope.

## Established evidence and current state

- `platform::win32::ReadMemory()` checks committed, non-guard and non-noaccess
  memory plus range, but does not require a readable Windows protection class.
- `SafeRead()` delegates to `ReadMemory()` throughout production runtime paths.
- `tests/feature_status/feature_status_harness.cpp` constructs feature-status
  enum values directly; it does not execute hooks, initialization rollback or
  runtime orchestration.
- A1–A5 closure is approved; all other quality-pass items are dispositioned.

## Approved scope

- Add a minimal readable-protection predicate internal to the existing Win32
  memory module and apply it only to `ReadMemory()`.
- Add a bounded platform-memory harness for readable, execute-only, noaccess
  and guard-page behavior where the operating system supports each setup.
- Correct only evidence wording that attributes orchestration guarantees to
  the FeatureStatus harness.

## Explicit non-goals

- No hook, resolver, PE scanner, `WorkerLifecycle`, build-system, DI or
  `helper.hpp` redesign.
- No caller changes unless compilation proves they are required.
- No write-semantic change, new memory framework or game launch.
- No rewriting of historically accurate reports.

## Expected files

- `src/platform/win32/memory.cpp`.
- `tests/platform/memory_harness.cpp`.
- Only reports/docs with inaccurate FeatureStatus evidence wording.

## Batches

### Q1 — ReadMemory protection contract

Implement the readable predicate and platform-memory harness.

Validation: new harness, existing WorkerLifecycle/FeatureStatus/
ConfigPersistence harnesses, production build and `git diff --check`.

### Q2 — Evidence accuracy

Correct overstated FeatureStatus harness claims without changing source or
claiming orchestration coverage.

Validation: targeted wording review and `git diff --check`.

## Risks and safe failure

- A protection classification mistake could reject normal readable pages;
  test explicit Windows page classes and preserve existing readable behavior.
- Execute-only/guard setup may be unsupported or architecture-dependent;
  record an actual limitation rather than simulating a pass.
- Do not broaden a documentation correction into historical record rewriting.

## Stop conditions

- Stop if the readable predicate requires a broad memory/PE refactor.
- Stop if a test scenario cannot be created reliably and document the concrete
  operating-system limitation.
- Stop after the two approved changes and validation; do not announce
  architecture freeze automatically.

## Final review

Compare changed paths with this plan, record validation and limits in
`backlog/TASKLOG.md`, and report completed, unchanged and not-runtime-
validated items.
