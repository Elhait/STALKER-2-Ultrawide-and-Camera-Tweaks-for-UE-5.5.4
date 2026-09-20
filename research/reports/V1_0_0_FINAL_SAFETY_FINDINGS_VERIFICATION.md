# v1.0.0 Final Independent Safety Findings Verification

## Scope

Read-only verification of the original independent P0/P1/P2 findings against
the current production tree after Batch 2 and Batch 3. This audit also reviews
reachable pre-cinematic work for the reported `Gameplay.Enabled=false` stutter
observation.

No production source, build configuration or runtime configuration was changed.
No game or research probe was run.

## Findings

### P0 — worker lifecycle

Current production workers use `plugin::WorkerLifecycle` with an owned stop
event and retained worker handles. `HotkeyLoop` waits interruptibly for 50 ms;
the diagnostic resolution loop is behind a non-production diagnostic define and
uses a 250 ms wait. The production build has no tight diagnostic polling loop.
`StopAndJoin` signals the event, joins workers, refuses self-join and releases
handles. The lifecycle harness covers normal, partial-start and
stop-before-start cases; the single-owner contract deliberately does not claim
concurrent self-join support.

Disposition: `FIXED + VERIFIED` for production workers.

The bootstrap `InitializeThread` handle is still closed by `DllMain`, but this
is separate from the owned runtime worker set. Normal DLL unload remains
unclaimed.

### P0 — initialization rollback

Feature-local initialization catches reset only the owned feature resources.
The outer `std::exception` and unknown-exception paths both call
`ResetAllRuntimeResources`, which stops workers and resets runtime-owned hooks
and state. No global transactional `HookSet` rewrite is required by the
current ownership model.

Disposition: `FIXED + VERIFIED` by current control flow and existing feature
status/lifecycle harness evidence.

### P1 — cinematic failed-write continuation

`ApplyCinematicAspectStore` validates writability, skips the custom store when
the target is invalid/unwritable and advances over the replaced native store
instruction. This matches the previously accepted intentional fail-closed
semantics.

Disposition: `INTENTIONAL BEHAVIOR + VERIFIED`.

### P1 — config persistence

`PersistConfigValue` remains non-destructive: staging is written first, the
input is closed before replacement, failed replacement preserves the live INI,
and the temporary file is cleaned up.

However, `config::SynchronizeManagedConfigTemplate` was found to write the
live INI with `std::ios::trunc` after constructing replacement content. This
was a separate persistence path that escaped the Batch 3.4 fix.

The bounded fix now writes a staging file, explicitly closes the source and
staging streams, validates flush/close state, and uses non-destructive
`MoveFileExW` replacement. Failed staging or replacement preserves the live
INI and reports the failure through the existing runtime logger.

Disposition: `FIXED + VERIFIED` by the template persistence harness.

### P1 — loader-lock cleanup

`DllMain(DLL_PROCESS_DETACH)` calls only `NotifyProcessDetach`. Process
termination returns immediately; normal detach only signals the worker stop
event. Hook reset and joining remain outside the detach callback.

Disposition: `FIXED + VERIFIED`; normal DLL unload is not claimed.

### P2 — helper and Win32 memory safety

`src/helper.hpp` still exists, is included by production `runtime.cpp`, and
provides the scanner implementation used by `hooks::signature_scanner.cpp`.
Its legacy `Memory::Write` and `PatchBytes` helpers use unchecked
`VirtualProtect`, C-style casts and no `#pragma once`; their symbols are not
reachable from the current stable runtime call graph. The current reachable
write paths use validated domain writers instead.

Disposition: `ARCHITECTURAL DEBT`; no current stable-runtime write defect
confirmed. The legacy helper should not be treated as production-quality API,
but this audit does not expand into a helper rewrite.

## Stutter-specific review

No repeated resolver scan, hook-install retry, config/file I/O loop or
production diagnostic resolution loop is reachable after initialization. The
hotkey worker sleeps on an interruptible 50 ms wait and performs two key-state
reads per iteration.

The Gameplay-disabled observer path is different:

```text
camera-writer hit
→ ReplayManualTransition
→ Gameplay=false branch
→ SafeRead(aspect)
→ VirtualQuery + memcpy
→ atomic g_lastObservedAspect store
```

This callback is attached to a camera-writer hot path and can run repeatedly
even before any cinematic. The work is read-only, but `VirtualQuery` on every
hit is a plausible source of the reported stutter.

Disposition: `PLAUSIBLE CANDIDATE`; no causal runtime proof yet. Production fix
not started.

## Final audit status

```text
Worker lifecycle                 FIXED + VERIFIED
Initialization rollback           FIXED + VERIFIED
Cinematic failed write            INTENTIONAL BEHAVIOR + VERIFIED
PersistConfigValue                FIXED + VERIFIED
Template synchronization write    FIXED + VERIFIED
Loader-lock cleanup               FIXED + VERIFIED
Legacy helper safety              ARCHITECTURAL DEBT
Observer stutter path             BOUNDED CHANGE IMPLEMENTED + TARGETED PASS
```

The template-synchronization finding is fixed and validated. The observer
review is documented separately in
`research/reports/V1_0_0_OBSERVER_HOT_PATH_DESIGN_AUDIT.md`. Its preferred
bounded change avoids installing the continuous observer when Gameplay is
disabled and Cinematics uses Auto. Source/static validation, harnesses, build
and one targeted user-run passed. Gameplay replay suppression after EXIT was
expected because Gameplay was disabled. Batch 4 release closure remains
stopped.
