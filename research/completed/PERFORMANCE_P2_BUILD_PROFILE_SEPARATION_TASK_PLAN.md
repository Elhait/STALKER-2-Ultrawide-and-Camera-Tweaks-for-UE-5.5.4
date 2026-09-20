# Performance P2 — Production and Diagnostic Build Profile Separation

## Objective

Separate the canonical production ASI build from the research-heavy diagnostic
instrumentation build while keeping one source tree and identical production
camera behavior.

## Established evidence and current state

- P1 contained runtime logging/probe overhead when Diagnostics.Enabled=false.
- The canonical build still unconditionally defines high-rate research
  instrumentation macros.
- The project already has runtime diagnostics gating and a shared source tree.
- Production and diagnostic artifacts must not be confused or installed
  interchangeably.

## Approved scope

- Keep `build.cmd` as the production build with no high-rate research defines.
- Add a separate `build-diagnostic.cmd` using the same source list and the
  existing diagnostic defines.
- Give the diagnostic artifact an explicit diagnostic filename.
- Preserve config-controlled runtime gating inside the diagnostic artifact.
- Keep common compiler/linker settings aligned.
- Update only build documentation/reporting needed to explain the profiles.

## Explicit non-goals

- No runtime source behavior changes.
- No hook, resolver, FOV, Dialogue, GameplayBaseline or coordinator changes.
- No deletion of diagnostic source or telemetry.
- No performance benchmark or game launch in this batch.
- No release packaging or Git commit.

## Expected files or areas

- `build.cmd`
- new `build-diagnostic.cmd`
- implementation report and `backlog/TASKLOG.md`

## Validation

- Run full `test.cmd`.
- Build production with `build.cmd`.
- Build diagnostic with `build-diagnostic.cmd`.
- Verify both outputs exist and have distinct names.
- `git diff --check`.
- Read-only Git review against this plan.
- No game launch.

## Risks and safe failure

- Production must not include the high-rate research defines.
- Diagnostic build must retain all currently supported diagnostic paths.
- If either profile diverges in common source inputs or fails to build, stop
  without changing runtime code.

## Stop conditions

- Stop after both builds, tests, diff review and report.
- Do not begin P3 measurement-driven optimization.

## Final review

Compare changed paths with this plan, confirm the production artifact remains
the stable release name, and record runtime/performance validation as pending.
