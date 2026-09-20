# v0.6.0 INI Description Update Task Plan

## Objective

Improve the user-facing comments in the current `STALKER2CameraTweaks.ini`
template and release asset without changing configuration keys, defaults or
runtime behavior.

## Approved scope

- Make boolean `true`/`false` choices explicit and easy to scan.
- Explain the current `Native` versus forced `16:9` cinematic behavior.
- Add per-dialogue-policy FOV examples for 90° and 110° gameplay FOV.
- Clarify hotkey defaults and next-state semantics.
- Keep version `v0.6.0` in the current template.

## Non-goals

- No config parsing or policy changes.
- No new keys, defaults, hotkeys or features.
- No changes to README/release version metadata in this batch.
- No game launch or runtime regression.

## Files

- `src/config/config_repository.cpp`
- `src/config/config_template.cpp`
- `tests/config/config_persistence_harness.cpp` (expected-description assertion only)
- `release-assets/STALKER2CameraTweaks.ini`

## Validation and stop condition

- Compare the generated template text with the release INI.
- Confirm keys/defaults are unchanged.
- Run `git diff --check`.
- Run the production build and existing harnesses because the embedded template is
  compiled source; update only the affected description assertion if required.
- Stop after the documentation/template review.
