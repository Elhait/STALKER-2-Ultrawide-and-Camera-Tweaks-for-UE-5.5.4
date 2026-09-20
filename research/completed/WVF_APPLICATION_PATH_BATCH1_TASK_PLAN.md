# Weapon Viewmodel FOV — WVF Application Path Batch 1

## Objective

Trace the bounded runtime execution order of the reference Weapon Viewmodel
FOV infrastructure from `ClientRestart`/world initialization to WVF Blueprint
execution and actor initialization. Establish at least one concrete runtime
edge without repeating already closed visual behavior tests.

## Established evidence and current state

- `Weapon Viewmodel FOV - 0` consistently preserves correct post-cinematic
  weapon framing; vanilla does not.
- Vanilla ADS repairs the current runtime weapon state, but that repair is lost
  across `ClientRestart` and the next post-cinematic lifecycle.
- At the corrected WVF F9/S3 observation point, one live `WVF_C` and one live
  `WVF_Actor_C` were found. `WVF_Actor_C` was owned by
  `S2Dev_Event_Watcher_C`.
- At the symmetric vanilla F9/S3 point, no live `WVF_C` or `WVF_Actor_C`
  matches were found.
- `WVF_C` static evidence includes `OnWorldBeginPlay`,
  `ExecuteUbergraph_WVF` and `DeferredActorSpawnFromClass`. Static names do
  not by themselves prove runtime execution order.
- `AnimScriptInstance`, `AimingData`, camera/PCM fields, F_99_P curves and
  exposed WVF actor FOV fields are not the accepted application seam.

## Approved scope

- Add one read-only UE4SS Lua execution-trace probe under `research/ue4ss/`.
- Register exact hooks for:
  - `/Script/Engine.PlayerController:ClientRestart`;
  - `/Weapon_Viewmodel_FOV/WVF.WVF_C:OnWorldBeginPlay`;
  - `/Weapon_Viewmodel_FOV/WVF.WVF_C:ExecuteUbergraph_WVF`;
  - `/Weapon_Viewmodel_FOV/WVF_Actor.WVF_Actor_C:ReceiveBeginPlay`;
  - `/Weapon_Viewmodel_FOV/WVF_Actor.WVF_Actor_C:ExecuteUbergraph_WVF_Actor`.
- Log timestamped callback name, safe self FullName/Class and safe Owner/Outer
  where available. Log argument values only as bounded `tostring` output.
- Keep the existing F7–F11 capture contract unchanged. This probe adds no
  research hotkey and does not replace F9 discovery.

## Explicit non-goals

- No repeated visual A/B, O-profile, S3/S5, AnimScriptInstance or identity-only
  tests.
- No polling, timer, broad UObject enumeration or property writes.
- No inference that registration equals execution.
- No Ghidra/CUE4Parse work in this batch unless the runtime hooks fail to
  expose any usable execution edge.
- No production ASI/source/release changes, builds or packaging.

## Expected files or areas

- `research/ue4ss/WVFApplicationTrace/Scripts/main.lua`
- This task plan.
- User-side copy into the disposable UE4SS `Mods` directory for runtime use.

## Batches and validation

### Batch 1 — Bounded execution probe

Create the probe, inspect it for exact hook paths, safe callback handling and
absence of writes/polling. Do not launch the game in this batch.

Initial runtime attempt exposed a tooling limitation: `ClientRestart` registered
successfully, but the four WVF Blueprint hooks were attempted before their
Blueprint `UFunction`s were present and were rejected by UE4SS. This is a probe
bootstrap failure, not evidence that the WVF functions do not execute.

The probe must therefore use `NotifyOnNewObject` for the exact WVF classes,
log their creation, and register the WVF hooks only after the corresponding
class/object has appeared. No polling or arbitrary delay is allowed.

### Batch 2 — User runtime trace

With `Weapon Viewmodel FOV - 0` enabled, run one normal lifecycle and one save
reload only if needed to capture a meaningful callback order. Preserve the
existing F7–F11 hotkeys and use F9 only for the already defined S3 observation
point.

The first WVF_0 attempt at approximately `16:04:00` produced two useful
`ClientRestart` callback records (`16:05:30` and `16:06:26`) but no WVF
execution records because all four early WVF hook registrations failed with
`no UFunction with the specified name was found`. It is classified as
`PARTIAL / TOOLING BOOTSTRAP FAILED`, not as a runtime negative result.

The corrected late-hook attempt then succeeded. It observed `WVF_C` creation
in `MainMenuMap` and `WorldMap`, registered the WVF UFunction hooks after the
classes appeared, and observed the following repeated runtime structure:

```text
WVF_C created
→ WVF_C::ExecuteUbergraph_WVF
→ WVF_C::OnWorldBeginPlay
→ ClientRestart
→ WVF_Actor_C created
→ WVF_Actor_C::ExecuteUbergraph_WVF_Actor
→ WVF_Actor_C::ReceiveBeginPlay
→ WVF_C::ExecuteUbergraph_WVF
→ repeated WVF_Actor_C ExecuteUbergraph calls
```

This establishes a concrete lifecycle/execution edge. The callback wrapper
still exposed `self` and the first Blueprint argument as `RemoteUnrealParam`,
so FullName/Owner and numeric `EntryPoint` were not yet decoded. The next
probe revision uses the already observed UE4SS `RemoteUnrealParam:get()` unwrap
pattern and logs a decoded numeric `EntryPoint` when available.

The next run decoded the first Blueprint arguments successfully and survived
without a new UE4SS crash. Observed records include:

```text
16:23:23  MainMenuMap
  WVF_C::ExecuteUbergraph_WVF  EntryPoint=1423
  WVF_C::OnWorldBeginPlay      arg1=UWorld

16:24:17  WorldMap
  WVF_C::ExecuteUbergraph_WVF  EntryPoint=1423
  WVF_C::OnWorldBeginPlay      arg1=UWorld
  WVF_C::ExecuteUbergraph_WVF  EntryPoint=15

16:24:19  ClientRestart

16:24:21  WVF_Actor_C created
  WVF_Actor_C::ExecuteUbergraph_WVF_Actor  EntryPoint=3903
  Owner = S2Dev_Event_Watcher_C
```

This is a stronger runtime edge than the previous address-only argument dump:
the WVF Blueprint dispatcher entry points are now observable. The earlier
access-violation run is classified as an unsafe unwrap attempt and contributes
no WVF behavior evidence.

## Success criteria

- A timestamped, reproducible execution edge is observed, such as:

```text
ClientRestart
  → WVF_C::OnWorldBeginPlay
  → WVF_C::ExecuteUbergraph_WVF
  → WVF_Actor_C::ReceiveBeginPlay
```

- Or a concrete WVF callback is tied to an external object/function relevant
  to the weapon lifecycle.

## Not enough

- Different UObject IDs after reload;
- class names found only by static inspection;
- another visual confirmation already established;
- a callback registration line without a callback execution line;
- a speculative call chain without runtime evidence.

The observed entry-point numbers are runtime anchors, not semantics yet. They
must be mapped against the cooked Kismet structure before assigning them to
profile selection, actor spawning, weapon refresh or any other behavior.

The repeated `ExecuteUbergraph_*` callbacks must not be called a tick or a
specific event until their entry-point argument is decoded. `On Event Watcher`
remains secondary; its reflected `Aim` flag was already rejected as the ADS
anchor.

## Risks and rollback / safe-failure behavior

- The probe is read-only and must never write UObject properties.
- Every reflected access is guarded; unsupported callback `self` values are
  logged as unavailable rather than dereferenced unsafely.
- If a hook callback causes an exception or crash, disable only this research
  mod and preserve the exact log/error. Do not modify production files.
- Do not use broad process cleanup or destructive filesystem operations.

## Stop conditions and phase gates

- Stop when one concrete runtime execution edge is established, then inspect
  only that edge in the next batch.
- The runtime execution edge is now established. Do not launch another game
  run for this batch; continue with targeted static mapping of entry points
  `1423`, `15` and `3903`.
- Stop and defer to targeted Kismet recovery if the hooks register but never
  produce usable execution evidence.
- Do not implement an ASI probe until runtime ownership and visible effect are
  both demonstrated.

## Expected final Git review

After the probe is added, perform a read-only review of status and the exact
research paths. Report completed, remaining, deferred and not-runtime-validated
items. Do not stage or commit.
