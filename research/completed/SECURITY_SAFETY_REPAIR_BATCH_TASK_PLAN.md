# Security/Safety Repair Batch — Task Plan

## Objective

Close the confirmed S1–S7 security/safety findings with bounded production
repairs and deterministic validation, without redesigning camera behavior or
launching the game.

## Established evidence and current state

- S1: native hook and worker ABI boundaries do not yet have complete exception
  containment.
- S2: vendored SafetyHook requires source confirmation before any backport.
- S3: resolver validation has implicit/fixed read windows that must be audited
  against real executable spans.
- S4: cinematic hook installation currently exposes callbacks before commit.
- S5: staged config persistence needs explicit flush/close verification.
- S6: Auto viewport aspect validation accepts degenerate positive geometry.
- S7: `ResumeThread` failure can lose ownership of a suspended thread.

## Approved scope

- Implement only bounded S1–S7 repairs confirmed by source inspection.
- Add focused deterministic tests/fault injection where current abstractions
  permit it.
- Update the security/safety report and factual safety documentation.
- Preserve A1 capability gating, existing camera transforms, resolver uniqueness,
  production/diagnostic separation, and fail-closed behavior.

## Explicit non-goals

- No architecture rewrite or RuntimeState decomposition.
- No new UE hooks, Dialogue redesign, HorPlus redesign, or aspect whitelist.
- No dependency-wide upgrade; only a reviewed SafetyHook backport if source
  applicability is confirmed.
- No game launch, commit, release, or unrelated cleanup.

## Expected files/areas

- `src/plugin/runtime.cpp`, hook callback helpers, and cinematic initialization.
- `src/hooks/instruction_validator.*` and resolver span APIs.
- `external/safetyhook/*` only if S2 is confirmed and the minimal backport is
  reproducible.
- `src/config/config_repository.cpp`, `src/platform/win32/viewport.*`,
  `src/lifecycle/worker_lifecycle.*` or their current locations.
- Focused tests and `test.cmd` only as needed.
- `research/reports/SECURITY_SAFETY_REPAIR_BATCH.md`.

## Batches and validation

1. Source confirmation and pure safety helpers/contracts; focused harnesses.
2. S1–S7 bounded repairs and tests, retaining PARTIAL/GAP where injection or
   provenance is insufficient.
3. Full `test.cmd`, `build.cmd`, `build-diagnostic.cmd`, `git diff --check`,
   read-only Git review, report/task log update.

## Risks and safe failure

- Callback exceptions must produce pass-through or required control-flow
  completion, never unwind through generated/native ABI.
- Resolver uncertainty rejects the candidate rather than reading farther.
- Failed installation/persistence/lifecycle operations preserve the prior safe
  state and report failure.
- If S2 cannot be proven against the vendored version, do not modify vendored
  code; report the finding as PARTIAL/GAP.

## Stop conditions

- Stop a finding at PARTIAL/GAP when its requested contract cannot be proven
  deterministically without inventing a simulation or new hook.
- Stop after validation and report; runtime remains not performed.

## Final review

Compare changed paths with this plan, separate pre-existing worktree changes,
record each finding's status and validation limits, append a bounded task-log
entry, and archive this plan under `research/completed/`.
