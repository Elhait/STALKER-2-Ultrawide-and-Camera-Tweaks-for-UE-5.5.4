# v1.0.0 Batch 3.2 — Local Failure and Shared-Fatal Design

## Scope

This design defines the mechanics for the already-established dependency
policy. It does not change production code, hook ordering or feature behavior.

## Stable invariants

1. Local rollback may release only resources owned by the failed feature.
2. `FAILED` and `DISABLED` remain distinguishable in state, logging and control
   flow.
3. The outer catch is a last-resort shared-fatal boundary, not the normal path
   for feature initialization failure.
4. A failed feature must not invalidate an independently valid feature.
5. A feature-local failure must leave that feature in one explicit unavailable
   or native-fallback state before initialization continues.

## Local failure path

Each optional feature initialization must have a bounded local scope:

```text
feature init begins
    ↓
resolver/install/setup fails
    ↓
release only that feature's acquired hooks/state
    ↓
mark feature FAILED/unavailable
    ↓
log feature + operation + reason
    ↓
continue independent initialization
```

Expected ownership boundaries:

- Gameplay failure resets only `g_hook` and gameplay-owned transient state.
- Cinematic aspect failure resets only the aspect-store hook and restores only
  cinematic aspect state acquired by that feature.
- Cinematic FOV failure resets only ENTER/EXIT hooks and cinematic state.
- Dialogue failure resets only the dialogue boundary hook and returns dialogue
  behavior to native mode.
- Hotkey worker failure disables hotkeys without removing production hooks.
- Diagnostic worker failure disables diagnostics without changing production
  behavior.

The implementation must preserve `DISABLED` as a separate state. A feature
disabled by configuration does not enter a failure path and should not emit a
failure diagnostic.

## Shared-fatal path

The shared-fatal path is reserved for failures such as:

- shared Runtime ownership cannot be initialized or is corrupted;
- worker lifecycle state is inconsistent and safe continuation cannot be
  established;
- an unknown exception leaves the acquired-resource set uncertain;
- a prerequisite used by all remaining active runtime operations is invalid.

Its contract is:

```text
shared invariant breaks
    ↓
stop further initialization
    ↓
stop workers
    ↓
release acquired shared/runtime resources in reverse ownership order
    ↓
enter defined non-running/fail-closed state
    ↓
log fatal reason
```

The shared-fatal path must not be used merely because a requested independent
feature failed to resolve.

## Initialization structure required by the design

The current single outer try/catch should be split conceptually into:

```text
shared runtime setup
    → shared-fatal handling

feature-local setup blocks
    → feature-local rollback + FAILED/native fallback

worker setup
    → worker-local handling unless lifecycle ownership itself is invalid
```

The exact helper/API shape remains an implementation decision. Tests must assert
observable outcomes, not catch-block identity or helper names.

## Catch semantics

`std::exception` and non-standard exceptions must not produce different
observable state when they represent the same failure class. An unknown
exception in a feature-local boundary is a local failure only if the boundary
can prove that ownership is contained; otherwise it enters shared-fatal
handling.

The existing outer catch asymmetry is therefore not fixed by copying cleanup.
It is resolved by routing failures through the correct local or shared boundary.

## Validation contract

Future tests should express behavior:

- Gameplay setup failure leaves Cinematics/Dialogue operational.
- Cinematic setup failure leaves Gameplay/Dialogue operational.
- Dialogue setup failure leaves Gameplay/Cinematics operational.
- Hotkey/diagnostic setup failure leaves production hooks operational.
- Shared ownership failure leaves no worker or hook active.
- `DISABLED` produces a valid bypass, not a failure state.

No production changes were made by this design step.
