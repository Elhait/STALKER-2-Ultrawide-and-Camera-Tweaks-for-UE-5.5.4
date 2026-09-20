# Performance P2 — Production and Diagnostic Build Profile Separation

Date: 2026-09-19

## Scope

This batch separates production and diagnostic compile-time instrumentation
while keeping one source tree and the same common build inputs.

## Build profiles

### Production

`build.cmd` now produces the stable `STALKER2CameraTweaks.asi` artifact.
The production profile does not define the high-rate research instrumentation:

- `ZOOM_TRANSITION_DIAGNOSTIC`;
- `HORPLUS_FOV_STATE_DIAGNOSTIC`;
- `CAMERA_STATE_SNAPSHOT_DIAGNOSTIC`;
- `HORPLUS_CINEMATIC_WRITER_TRACE_DIAGNOSTIC`;
- `MATCHGAMEPLAY_DIAGNOSTIC`.

The runtime source and common compiler/linker settings remain shared.

### Diagnostic

`build-diagnostic.cmd` selects the diagnostic profile and produces the separate
`STALKER2CameraTweaksDiagnostic.asi` artifact.

It enables the supported high-rate research instrumentation and preserves the
existing `[Diagnostics] Enabled` runtime gate. The diagnostic artifact is for
evidence collection and is not suitable for FPS/frametime comparison.

## Safety and compatibility

- No runtime source behavior was changed.
- No hook, resolver, coordinator, GameplayBaseline, HorPlus, cinematic or
  Dialogue logic was changed.
- The production artifact keeps the stable release filename.
- The diagnostic artifact has an explicit different filename to prevent
  accidental installation over the production ASI.
- Both profiles use the same source list and common compiler/linker settings.

## Validation

- Full `test.cmd`: PASS.
- Production `build.cmd`: PASS.
- Diagnostic `build-diagnostic.cmd`: PASS.
- Production artifact exists: `STALKER2CameraTweaks.asi` (1,152,512 bytes).
- Diagnostic artifact exists: `STALKER2CameraTweaksDiagnostic.asi`
  (1,179,648 bytes).
- `git diff --check`: PASS; only normal line-ending warnings reported.
- Game launch and performance benchmark: NOT PERFORMED.

## Status

`P2 production/diagnostic build separation: FIXED STATICALLY / BUILDS VALIDATED`.

P3 remains deferred: measurement-driven optimization of stores, SafetyHook
context preservation, compiler optimization level and duplicate viewport
queries.
