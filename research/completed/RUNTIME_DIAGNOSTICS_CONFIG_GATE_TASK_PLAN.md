# Runtime Diagnostics Config Gate Task Plan

## Objective

Replace the current separate combined diagnostic build requirement with one
canonical ASI that contains the current supported telemetry and enables it only
when `[Diagnostics] Enabled=true` is configured.

## Established evidence and current state

- Current diagnostic compilation is controlled by build-time macros.
- The combined diagnostic build currently enables the active CameraState,
  ZOOM transition and HorPlus FOV-state telemetry through separate defines.
- The production ASI therefore does not contain all of that telemetry, while
  the diagnostic ASI requires a separate build and filename.
- Historical research probes use many unrelated macros and have different
  safety/ownership contracts; they must not be silently promoted into the
  stable product.

## Approved scope

1. Add a persisted `[Diagnostics] Enabled=false` configuration value with
   template synchronization and parser/harness coverage.
2. Add a small diagnostics runtime module that owns the enabled gate and
   diagnostic-only lifecycle state/helpers.
3. Compile the current supported combined telemetry into the canonical ASI and
   gate its installation, state updates and output at runtime:
   - CameraStateSnapshot;
   - ZOOM_IN/ZOOM_OUT read-only edge telemetry;
   - HorPlus FOV-state telemetry.
4. Keep production camera transformations, Dialogue/Cinematics behavior,
   resolver logic and failure behavior independent of the diagnostics flag.
5. Keep existing diagnostic build scripts as compatibility wrappers or clearly
   mark them obsolete only after the canonical path is validated.

## Explicit non-goals

- Do not promote historical Ghidra/research probes or their hooks.
- Do not change Candidate, Dialogue, HorPlus, Cinematics,
  AspectRecalculation or ZOOM behavior.
- Do not add new telemetry fields in this batch.
- Do not remove old build scripts destructively.
- Do not launch the game in this implementation batch.

## Expected files or areas

- `src/config/feature_config.hpp/.cpp`
- `src/config/config_repository.cpp`
- `src/config/config_template.cpp`
- `src/diagnostics/diagnostic_runtime.hpp/.cpp`
- `src/plugin/runtime.cpp`
- `tests/config/config_persistence_harness.cpp` or a focused diagnostics
  harness
- `build.cmd`, `test.cmd` and the combined diagnostic wrapper only as needed
- `research/reports/RUNTIME_DIAGNOSTICS_CONFIG_GATE.md`
- `backlog/TASKLOG.md`

## Implementation batches

### Batch 1 — Config and diagnostics module

Add the default-off config value, synchronized INI text, parser state and a
small runtime gate with reset/enable semantics. The default must remain quiet
and fail closed.

### Batch 2 — Active telemetry integration

Compile the supported combined telemetry into the canonical build and gate
hook installation, snapshot updates and diagnostic logs. No production owner
or camera write may depend on the gate.

### Batch 3 — Harness/build review

Add config/gate assertions, run the full harness, production build and
diagnostic-enabled compile path, inspect the resulting diff, and stop before
runtime.

## Validation

- Config persistence/template harness: PASS.
- New diagnostics gate harness: PASS.
- Full `test.cmd`: PASS.
- Canonical production build with diagnostics compiled in: PASS.
- `git diff --check`: PASS.
- No game launch in this batch.

## Risks and safe failure

- Diagnostics default off; a missing, malformed or unavailable setting must
  leave telemetry disabled without affecting feature availability.
- Diagnostic hooks must not be installed when disabled.
- Disabling diagnostics must not reset or mutate production camera state.
- Existing research-only macros remain outside this migration if their owner
  or safety contract is not established.

## Stop conditions and phase gates

- Stop if integrating a telemetry block requires production behavior changes.
- Stop if a historical probe cannot be separated cleanly from supported
  telemetry.
- Stop after static/harness/build validation; runtime is a separate gate.

## Expected final Git review

Confirm only config, diagnostics module, supported telemetry integration,
harness/build wiring, report, task log and archived plan changed. Record the
runtime toggle matrix for the next game session: `Enabled=false` and
`Enabled=true` in the same canonical ASI.
