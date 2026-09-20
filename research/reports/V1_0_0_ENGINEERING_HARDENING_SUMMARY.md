# v1.0.0 Engineering Hardening Summary

## Purpose

This is factual context for the final independent v1.0 review. It is not an
approval request and is not a substitute for reviewing the current production
tree. A reviewer should verify every relevant claim against source, build
configuration, tests and recorded evidence.

## Scope boundary

v0.6.0 established the user-facing feature scope. v1.0.0 work has preserved
that scope while completing production structure, safety hardening, evidence
accuracy and repository hygiene.

The following remain outside v1.0 scope: new features, Weapon/Viewmodel FOV,
subtitle centering, new experimental hooks and deferred reverse-engineering.

## Architecture and ownership

- Production code was separated into plugin, config, hooks, gameplay,
  cinematics, dialogue and Win32 platform ownership boundaries.
- The translation-unit-private `RuntimeState` is the explicit lifetime owner
  for module identity, configuration, workers, installed hooks, feature
  availability, transition state, dialogue state and telemetry.
- Domain modules own reusable policy, calculation and resolution primitives;
  `RuntimeState` owns runtime integration and cross-domain coordination.
- A separate coordinator class was considered but not introduced because it
  would receive runtime-owned hooks, lifecycle and state through callbacks
  without gaining an independent contract.

## Supported ASI lifetime

- Supported model: load the ASI and keep it resident until process termination.
- Normal dynamic `FreeLibrary`/manual unload is explicitly not claimed.
- Controlled shutdown signals workers, joins them, then resets hooks/restores
  state outside loader lock.
- `DLL_PROCESS_DETACH` performs no complex teardown on process termination;
  normal detach only signals stop.

## Build and repository hygiene

- `build.cmd` discovers Visual Studio through `vswhere`, requires the MSVC
  17.14+ x64 C++ toolchain and uses explicit `/std:c++latest` language mode.
- Source and dependency paths are repository-relative; no author-local Visual
  Studio path is required.
- Stable production behavior no longer depends on test-named atomicity defines.
- Production object output is explicitly directed to `build-artifacts/obj`;
  production builds do not emit `.obj` files into the repository root.
- Generated root objects, `build-artifacts/obj` and CUE4Parse `bin/obj` output
  were removed from the repository index and working tree. `.gitignore` covers
  regenerated outputs.
- Nine intentionally preserved historical release archives remain tracked.

## Safety changes and evidence

| Area | Final result | Evidence |
| --- | --- | --- |
| Worker lifecycle | Stop signal, retained worker handles, join-before-hook teardown, defensive self-join refusal and loader-lock-safe detach path | Production lifecycle harness, static detach review |
| Feature initialization | Feature-local failures degrade locally; shared-fatal path cleans owned runtime resources | Source review, FeatureStatus vocabulary harness, final runtime regression pending |
| Cinematic failed write | Intentional fail-closed behavior: custom/native store skipped and RIP advances after validated instruction | Static control-flow review; behavior preserved |
| Config persistence | Non-destructive staging and replacement; last-known-good INI survives staging/replacement failure | Production config-persistence harness |
| Template synchronization | Same non-destructive commit semantics as persistence | Production config-persistence harness |
| Gameplay-disabled observer | Unnecessary camera-writer observer removed when Gameplay is disabled | Build, focused harnesses and targeted user-run log evidence |
| ReadMemory | Requires committed, in-range, non-guard, non-noaccess and explicitly readable page protection | Platform-memory harness using real Win32 page protections |

## Harness scope and limits

- `WorkerLifecycle` harness executes the real lifecycle implementation for
  startup, controlled stop, repeated stop, partial-start cleanup and
  stop-before-start. The primitive is documented as externally serialized;
  self-join is a defensive misuse guard, not a supported concurrent contract.
- `ConfigPersistence` harness executes real persistence/template code for
  normal save/synchronization plus staging and replacement failure preservation.
- `FeatureStatus` harness verifies only `DISABLED`/`FAILED`/`AVAILABLE`
  vocabulary semantics. It does not execute hook installation, initialization
  rollback, shared-fatal cleanup or game runtime orchestration.
- The platform-memory harness verifies readable, execute-only, noaccess and
  guard-page behavior through `VirtualAlloc`/`VirtualProtect`.
- Harness success and a successful build are not game-runtime proof.

## Resolver and update discipline

- Production resolution uses centralized signatures, unique-match handling,
  structural instruction/operand validation and fail-closed refusal.
- The supported-build update workflow requires executable identity, `.text`
  layout, known anchors, resolver/decode checks, callback/offset review,
  startup log evidence and in-game regression before a support claim changes.
- A typed descriptor framework and broader multi-version fixtures were
  considered but deferred: no current patch-maintenance ambiguity demonstrates
  their additional complexity is needed for v1.0.

## Deliberately not changed

- Transactional `HookSet`: existing feature-local and shared-fatal cleanup had
  no demonstrated reachable gap; a transaction would duplicate coordinator
  responsibility.
- WorkerLifecycle RAII/full rewrite: current explicit process-resident contract
  and harness coverage do not demonstrate a defect; destructor-driven cleanup
  could imply unsupported unload behavior.
- Further `runtime.cpp` decomposition: no remaining block showed a natural
  independent owner without introducing a forwarding coordinator.
- DI framework/fake hook layer: no currently untestable invariant justified a
  broader test-only abstraction.
- CMake/MSBuild migration: portable documented `build.cmd` closes the stated
  reproducibility requirement.
- PE/memory subsystem rewrite: only the bounded readable-page contract was
  justified; no broad rewrite was introduced.
- Legacy `helper.hpp` relocation: production no longer consumes it, while 54
  archived research files do; relocation is deferred to preserve research
  usability unless a future research build surface is standardized.

## Current validation state

Completed evidence:

- production `build.cmd`: PASS;
- WorkerLifecycle harness: PASS;
- FeatureStatus vocabulary harness: PASS;
- ConfigPersistence harness: PASS;
- platform-memory harness: PASS;
- `git diff --check`: PASS.

Not yet established after the final architecture/quality changes:

- full in-game regression of Gameplay, Cinematics, Dialogue, Auto aspect,
  post-cinematic handoff and normal startup on the supported Steam 2.0.5 build.

## Required reviewer posture

The final independent reviewer should treat this document as context only.
They should independently inspect the current code, verify the stated limits,
identify any concrete contradiction or missing v1.0-quality requirement, and
distinguish a demonstrated improvement from an alternative architectural taste.

No architecture freeze has been declared. The next step is the final
independent review; only after its disposition may architecture freeze and the
final Batch 4 in-game regression proceed.
