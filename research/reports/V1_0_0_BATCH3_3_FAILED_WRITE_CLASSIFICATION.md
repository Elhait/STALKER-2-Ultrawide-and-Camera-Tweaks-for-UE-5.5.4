# v1.0.0 Batch 3.3 — Cinematic Failed-Write Classification

## Scope

This audit classifies the cinematic aspect-store failure path after the Batch 2
extraction and Batch 3.1–3.2 changes. No production behavior is changed.

## Current control flow

```text
cinematic aspect-store hook
    ↓
read target object from RAX
    ↓
resolve configured/runtime aspect
    ↓
validate target + offset + writable memory
    ↓
write custom aspect when writable
    ↓
always advance RIP by the original 10-byte MOV encoding
```

When the target is null, overflows the offset calculation, fails writability
validation or otherwise cannot accept the custom write:

```text
custom write = refused
native MOV   = skipped
execution    = resumes after the original store
```

The current log explicitly reports:
`Cinematic aspect store refused: target object was not writable; native store skipped.`

## Classification

### `INTENTIONAL BEHAVIOR` — current evidence

The hook performs a guarded write and deliberately avoids executing the original
memory store when its target cannot be validated as writable. Resuming after the
instruction preserves the surrounding engine control flow without repeating an
unsafe write through the original instruction.

This is a fail-closed memory-safety policy, not proof of a visual or gameplay
regression.

### Not a confirmed defect

The current evidence does not establish that native pass-through is required or
safe for an invalid/unwritable target. Executing the original `MOV` could repeat
the same invalid write or fault. Therefore the old audit concern “failed write
must pass through to native” is rejected as an automatic fix.

### Remaining boundary

If future evidence establishes that the target is valid, the native store would
be safe, and skipping it changes a required engine state, this classification
must be reopened with that evidence. No such evidence is currently present.

## Validation limits

- Static control-flow and bounded memory-validation review completed.
- No runtime fault injection or game behavior change performed.
- No `ctx.rip` or native pass-through change is justified.

## Non-goals

- no hook continuation change;
- no SafetyHook transactionality change;
- no cinematic policy change;
- no configuration persistence work.
