# Runtime Diagnostics Config Gate

## Scope

This bounded batch replaces the need for a separate combined diagnostic ASI
for the currently supported read-only telemetry. The canonical ASI now contains
the CameraStateSnapshot, ZOOM_IN/ZOOM_OUT and HorPlus FOV-state diagnostics,
but they remain disabled unless the user sets:

```ini
[Diagnostics]
Enabled=true
```

The default is `Enabled=false`.

Historical research probes and their executable-specific hooks were not
promoted into this gate.

## Implementation

- Added `FeatureConfig::diagnosticsEnabled` with default-off parsing.
- Added synchronized `[Diagnostics]` template text for new and existing INI
  files.
- Added `src/diagnostics/diagnostic_runtime.*` as the thread-safe runtime gate.
- Compiled the supported telemetry into the canonical `build.cmd` path.
- Gated diagnostic hook installation, state updates and diagnostic output.
- Kept gameplay, cinematic, dialogue, mode-transition and resolver behavior
  independent of the diagnostics flag.
- Kept existing diagnostic wrapper scripts for compatibility.
- When diagnostics are disabled, startup now reports that telemetry hooks were
  intentionally not installed instead of reporting a failed installation.

## Validation

- Full `test.cmd`: PASS.
- Config persistence/template coverage: PASS.
- Diagnostics gate harness: PASS.
- Canonical production `build.cmd`: PASS.
- Existing HorPlus FOV diagnostic wrapper build: PASS.
- `git diff --check`: PASS; only normal line-ending warnings were reported.
- No game launch and no runtime validation in this batch.

## Runtime matrix for the next session

Run the same canonical ASI twice, changing only `[Diagnostics] Enabled`:

1. `false`: quiet production path; no supported diagnostic hooks or telemetry.
2. `true`: CameraStateSnapshot, ZOOM_IN/ZOOM_OUT and HorPlus FOV telemetry
   available for the combined regression session.

The runtime toggle must not change camera output or feature availability.

## Status

Implemented and statically/harness validated. Runtime behavior of the toggle
remains not validated until the next planned combined game session.
