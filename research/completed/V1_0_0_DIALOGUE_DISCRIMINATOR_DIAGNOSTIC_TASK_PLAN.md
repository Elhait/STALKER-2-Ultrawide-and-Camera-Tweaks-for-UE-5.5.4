# V1.0.0 Dialogue Discriminator Diagnostic Task Plan

## Objective

Prepare a diagnostic-only ASI build that records the existing dialogue-boundary
callback context for ordinary gameplay FOV changes, cinematic EXIT recovery and
real dialogue, so a positive dialogue-ownership discriminator can be identified.

## Established evidence and current state

- `TraceDialogueBoundary` is installed at the validated `DialogueBoundary`
  virtual-call site.
- The callback currently classifies valid descending FOV samples while the
  coordinator is `Gameplay` as a dialogue lifecycle.
- This explains the confirmed false-positive, but no positive real-dialogue
  discriminator has been established.
- The existing production behavior and hook contract are otherwise retained.

## Approved scope

- Add compile-time diagnostic logging inside `TraceDialogueBoundary`.
- Record callback sequence, stack return address, relevant preserved register
  values, FOV input/output, coordinator, dialogue policy and phase.
- Record a bounded marker for each existing normal return path.
- Build the diagnostic candidate and run static diff checks.

## Explicit non-goals

- Do not change dialogue classification, transformation or state transitions.
- Do not add a cooldown, timer, polling, guard, new hook or new resolver.
- Do not dereference new game objects or probe unvalidated offsets.
- Do not change Cinematics, Gameplay, HorPlus, AspectRecalculation or Dialogue
  configuration semantics.
- Do not run the game or claim runtime validation.

## Expected files or areas

- `src/plugin/runtime.cpp`: diagnostic-only callback instrumentation.
- `build.cmd`: only if required to enable the diagnostic compile definition.
- This task plan and the final bounded task-log entry.

## Implementation batches

### Batch 1 — callback-context instrumentation

Add a compile-time diagnostic block that logs the existing callback context
before production logic and emits markers at the existing return paths. Use
only register values and the already available stack return address.

Validation: inspect the diff for unchanged production predicates and run
`git diff --check`.

### Batch 2 — diagnostic build and review

Build the diagnostic candidate, inspect the resulting changed paths and verify
that no production behavior was changed outside the diagnostic block.

Validation: `build.cmd`, `git diff --check`, read-only Git review.

## Risks and rollback / safe-failure behavior

- Logging can add callback overhead; it is compile-time diagnostic-only and is
  not part of the release path.
- Reading the stack return address uses the existing `SafeRead` helper; failure
  is logged as unavailable and does not alter control flow.
- The diagnostic block can be removed without changing the production callback.
- No Git state-changing or destructive operation is permitted.

## Stop conditions and phase gates

- Stop if instrumentation requires an unvalidated object field, new hook or
  production predicate change.
- Stop after the diagnostic build and static review; runtime execution is owned
  by the user in a separate step.

## Expected final Git review

Confirm only the approved runtime diagnostic area and any necessary diagnostic
build definition changed; preserve all unrelated user work. Report completed,
remaining, deferred, blocked and not-runtime-validated items separately.
