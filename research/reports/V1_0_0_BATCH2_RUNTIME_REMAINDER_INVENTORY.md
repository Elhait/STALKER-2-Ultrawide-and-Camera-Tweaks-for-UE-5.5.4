# v1.0.0 Batch 2 — Remaining `plugin::Runtime` inventory

Date: 2026-09-14  
Scope: read-only classification after config, hooks, gameplay and cinematic/dialogue domain extractions.

## Current size

`src/plugin/runtime.cpp` is currently 1,887 lines, down from the original
2,647-line production translation unit. The reduction came from moving
configuration, hook infrastructure, gameplay/cinematic calculations and
dialogue calculations into their domains.

## Classification

### A — Legitimate plugin lifecycle

- `Initialize`
- `SetModuleHandle`
- `InitializeThread`
- `Shutdown`
- logger/config subsystem composition

These functions are composition-root responsibilities. Thread creation and
shutdown behavior remain unchanged and are Batch 3 safety-sensitive material.

### B — Legitimate cross-domain orchestration

- `ReplayManualTransition`
- `TraceCinematicEnter`
- `TraceCinematicExit`
- `TryAtomicExitHandoff`
- `ApplyGameplayAspectFixAtomic`
- coordinator state transitions between gameplay, cinematic EXIT and recovery

This is the strongest candidate for the final intentional `plugin::Runtime`
boundary. It coordinates validated interactions rather than owning one
feature's calculations.

### C — Gameplay implementation still coupled to orchestration

- `ReplayManualTransitionOriginal`
- `TryDeferredGameplayReplay`
- `WriteFlagsOnly`
- remaining gameplay telemetry and transition snapshots

These are possible future gameplay extractions, but they depend on cinematic
state, coordinator state, diagnostics and Batch 3 atomicity semantics. Do not
move them mechanically until the cross-domain API is explicit.

### D — Cinematic implementation still coupled to integration

- `ReadRuntimeAspect`
- `ReadClientViewportAspect`
- `ResolveAutoAspect`
- `ApplyCinematicAspectStore` hook callback wrapper
- `InstallCinematicAspect`
- `RestoreCinematicAspect`

Policy, resolver and application primitives already belong to `cinematics/`.
The remaining functions combine viewport/platform access, hook context,
original-byte lifecycle and coordinator interaction.

### E — Dialogue implementation still coupled to integration

- `ResetDialogueRuntimeState`
- `TraceDialogueBoundary`
- `InstallDialogueBoundary`
- dialogue mutex and phase transition orchestration

Dialogue FOV math and phase vocabulary already belong to `dialogue/`. The
remaining code is stateful callback integration and should not be split merely
to reduce `runtime.cpp` line count.

### F — Platform/Win32 infrastructure still embedded

- `ComputeSha256`
- `IsWritable`
- `SafeRead`
- `FindCurrentProcessWindowForAspect`
- `ReadClientViewportAspect`
- `ResolutionMonitorLoop`
- `HotkeyLoop`

These are the clearest remaining candidates for `platform/win32/`, but their
extraction should be a separate bounded infrastructure batch because several
are used by safety-sensitive callbacks and diagnostic paths.

### G — Hook integration glue

- `ValidateEnterBoundary`
- `ValidateExitBoundary`
- `ValidateIndexedExitBoundary`
- `ResolveCinematicFovCallsites`
- `InstallCinematicAspect`
- `InstallDialogueBoundary`
- callback registration and reset calls

Generic discovery and instruction primitives already belong to `hooks/`.
These remaining functions are feature-specific integration glue and should
move only when their callback/lifecycle contracts are explicit.

### H — Batch 3 safety-sensitive code

- deferred gameplay replay arm/cancel paths;
- atomic EXIT handoff state and compare-exchange paths;
- hook installation failure handling;
- original-byte restoration;
- `CreateThread`/shutdown behavior;
- failed-write continuation behavior;
- partial initialization and reset ordering.

These are intentionally not Batch 2 extraction targets unless a move is purely
mechanical and proven behavior-preserving.

## Decision

The remaining `runtime.cpp` is no longer a generic production monolith. Most
of its unresolved code is either:

1. legitimate plugin composition/lifecycle;
2. deliberate gameplay↔cinematics↔dialogue orchestration; or
3. safety-sensitive integration glue reserved for Batch 3.

The next Batch 2 candidates are therefore bounded infrastructure work, not
continued mechanical feature splitting:

- platform/Win32 ownership extraction, if it can be done without callback
  semantic changes;
- explicit cross-domain coordinator API design after review of the remaining
  state machine;
- no further extraction solely to make `runtime.cpp` smaller.

## Non-goals

- no rollback or transactionality fixes;
- no thread/shutdown redesign;
- no failed-write behavior changes;
- no new gameplay, cinematic, dialogue or Weapon Viewmodel FOV features;
- no runtime regression claim from this inventory.
