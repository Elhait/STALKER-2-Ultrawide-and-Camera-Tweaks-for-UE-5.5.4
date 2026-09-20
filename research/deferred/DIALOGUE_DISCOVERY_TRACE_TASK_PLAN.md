# Dialogue discovery trace task plan

## Objective

Build a separate diagnostic ASI that starts a bounded ten-second F12 session
and records change-only telemetry around the existing validated
DialogueBoundary/FOV-blend hook. The trace is for discovery only and must not
change production Dialogue, Gameplay, Cinematics or HorPlus behavior.

## Established evidence and current state

- `FUN_140D08CE0` is a generic native FOV-blend path.
- `RSI` is its native blend context; `+0x28`, `+0x2C`, and `+0x30` are native
  blend values, with `+0x2C` established as a target/end FOV rather than a
  Dialogue flag.
- UE4SS `APC::IsInStaticDialog()` supplied ground-truth edges for the tested
  static dialogue path, but standalone ASI access remains deferred.
- Existing Dialogue classifier and existing diagnostic hook must remain
  unchanged by this discovery build.

## Approved scope

- Add a diagnostic compile-time path only.
- F12 rising edge starts one ten-second trace; repeated F12 during a session is
  ignored; expiry writes one bounded END summary.
- Use only the existing validated DialogueBoundary hook and the existing
  worker lifecycle.
- Safely observe known registers/state plus `RSI+0x00..0x80` in 4-byte units.
- Log only changed snapshots, with monotonic elapsed time and counters.
- Produce `STALKER2CameraTweaks_DialogueDiscovery.asi` separately.

## Explicit non-goals

- No production classifier repair or new Dialogue ownership predicate.
- No UE4SS integration, reflection bridge, new hook, resolver or native call.
- No writes to game memory and no changes to XMM registers or camera state.
- No related-object walk or call to `FUN_140D08FE8`.
- No changes to HorPlus, AspectRecalculation, Cinematics, config semantics or
  hotkey meanings.
- No game launch.

## Expected files/areas

- `src/plugin/runtime.cpp`: diagnostic-only trace state, F12 worker and hook
  telemetry.
- `build-dialogue-discovery.cmd`: separate diagnostic build wrapper.
- `research/reports/`: factual build/design report after validation.

## Batches and validation

1. Add the bounded diagnostic implementation and isolated build wrapper.
   Validate preprocessing scope, F12 start/expiry logic, safe-read-only access,
   and absence of production-path edits outside `#ifdef`.
2. Build the diagnostic ASI only. Check output identity, SHA-256 and file path.
3. Run `git diff --check` and a read-only Git review against this plan.

No runtime test is performed by this task; the user owns the later F12 run.

## Risks and safe failure

- Failed reads skip the affected snapshot/field and keep the trace running.
- Invalid/null context or invalid logger/session state fails closed.
- The session is inactive by default and automatically expires after ten
  seconds. Repeated F12 cannot extend it.
- The diagnostic target is separate from the production ASI and must never
  replace it.

## Stop conditions and phase gates

- Stop if the existing DialogueBoundary hook cannot be reused without a new
  resolver or hook.
- Stop if safe bounded memory reads cannot be implemented without broad pointer
  walks.
- Stop after diagnostic build, static checks and Git review; do not launch the
  game or infer a Dialogue discriminator from telemetry.

## Final review requirements

Report the diagnostic ASI path/SHA-256, actually instrumented fields, fields
omitted for safety, build result, diff-check result, production untouched
status, and the fact that runtime validation remains pending.
