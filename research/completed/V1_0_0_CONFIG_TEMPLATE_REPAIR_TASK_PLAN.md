# v1.0.0 Config Template Repair Task Plan

## Objective

Make managed INI synchronization restore missing sections, keys and canonical
managed comments idempotently while preserving valid or invalid user values and
unknown user content.

## Established evidence and current state

The current synchronization repairs some managed comments and Hotkeys keys but
does not consistently restore missing Gameplay/Cinematics/Dialogue sections or
keys, and partial managed-comment damage is not fully repaired. The generated
template and release asset already contain the desired canonical descriptions.

## Approved scope

- Repair missing managed sections, keys and canonical comments.
- Preserve existing values exactly, including invalid values.
- Preserve unknown keys, comments and other section content.
- Make repeated synchronization idempotent.
- Extend the existing config harness with observable file-content checks.

## Explicit non-goals

- No config parsing or runtime fallback policy changes.
- No rewriting of existing values, including invalid values.
- No new keys, defaults or features.
- No broad ConfigRepository redesign.
- No game launch or in-game regression.

## Expected files and areas

- `src/config/config_template.cpp`
- `tests/config/config_persistence_harness.cpp`
- `backlog/TASKLOG.md`

## Batches and validation

1. Replace managed-template repair with bounded section/key/comment repair.
2. Add harness cases for missing structure, preservation and idempotence.
3. Run `build.cmd`, `test.cmd` and `git diff --check`.
4. Perform read-only Git review against this plan.

## Risks and safe failure

Unknown user content must remain untouched. Existing values must remain
unchanged. A staging/write/replace failure must preserve the live INI through
the existing non-destructive commit path.

## Stop conditions

Stop if preserving unknown content requires a broader parser redesign, if any
runtime value semantics change, or if validation exposes a regression outside
template repair.

## Final review

Confirm the changed paths match this plan, document completed/deferred items in
`backlog/TASKLOG.md`, and archive this plan under `research/completed/` only
after validation passes.
