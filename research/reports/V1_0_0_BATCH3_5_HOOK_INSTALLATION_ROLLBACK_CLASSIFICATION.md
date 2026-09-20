# v1.0.0 Batch 3.5 — Hook Installation and Rollback Classification

## Scope

Audit the current post-Batch-3.2 initialization and hook-installation control
flow for partial installation, orphaned hooks and incorrect rollback ownership.
This is classification only; no production behavior is changed.

## Current ownership and failure paths

### Dialogue boundary

`InstallDialogueBoundary` resolves one candidate, validates the expected
instruction shape, creates/enables the hook and stores it in the runtime-owned
`HookSet`. Any failure returns `false`; its initialization scope resets the
dialogue hook and continues with native dialogue behavior.

### Cinematic aspect store

The resolver validates the store before installation. `InstallCinematicAspect`
records the store and original bytes, creates the store hook and marks the
patch active only after hook creation succeeds. Its local failure scope calls
`RestoreCinematicAspect`, which resets the store hook and clears the patched
state.

### Cinematic FOV callbacks

The two callsites are resolved before either hook is installed. The first and
second hook objects are assigned to separate `HookSet` members. If either
creation fails, the local failure scope resets both enter and exit hooks and
continues with native cinematic FOV behavior.

### Gameplay camera hook / cinematic observer

The validated writer/observer address is resolved before `g_hook` creation.
Failure resets `g_hook`, clears the resolved address and, when gameplay was
requested, marks gameplay unavailable without tearing down independent feature
hooks.

### Shared-fatal path

The outer initialization handlers call `ResetAllRuntimeResources`. That path
stops and joins owned workers before resetting diagnostic hooks, gameplay,
dialogue and cinematic hooks, then restores the cinematic aspect state and
clears gameplay availability.

## Classification

```text
Feature-local partial installation     CONFIRMED HANDLED
Shared-fatal cleanup                   CONFIRMED HANDLED
Orphaned production hook found         NO
Incomplete rollback defect             NOT CONFIRMED
All-or-nothing HookSet requirement     NOT JUSTIFIED
HookSet as an ownership container      INTENTIONAL ARCHITECTURAL DEBT
```

The current code does not establish a confirmed rollback defect. Each
production feature has a bounded local cleanup path, and the shared outer
failure path resets the complete runtime-owned hook set. Independent feature
continuation is preserved rather than converted into global all-or-nothing
installation.

The `HookSet` type remains a passive ownership container. It does not itself
provide transactionality, automatic rollback or destruction policy. That is an
architectural limitation, but the current coordinator control flow explicitly
owns those semantics and currently covers the observed failure paths.

## Decision

```text
Production changes             NONE
Transactional HookSet rewrite  DEFERRED / NOT JUSTIFIED
Installation order             UNCHANGED
Feature-local degradation       PRESERVED
Shared-fatal cleanup            PRESERVED
```

Do not introduce global rollback merely because the type is named `HookSet`.
Any future change to hook transactionality requires new evidence showing a
reachable cleanup gap or an observable unsafe continuation.

## Validation and limits

- Reviewed `plugin::Initialize`, all current production feature-local catches,
  `ResetAllRuntimeResources` and the `hooks::HookSet` ownership boundary.
- This was a static control-flow classification; no synthetic hook-failure
  injection was added because no concrete defect was identified to target.
- Existing lifecycle, feature-status and config-persistence harnesses are
  unrelated and were not repeated for this classification.
- Build and in-game regression are unchanged by this audit and are not claimed
  as new evidence for Batch 3.5.

## Stop condition

Close this candidate without production changes. Reopen only if a future
runtime or controlled failure test demonstrates an orphaned hook, invalid
continuation or cleanup ordering defect that the current ownership paths do
not handle.
