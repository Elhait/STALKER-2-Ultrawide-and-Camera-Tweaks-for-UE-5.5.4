# V1 Default INI Description Task Plan

## Objective

Update the canonical INI defaults and user-facing descriptions to represent the
validated v1 seamless HorPlus path.

## Established evidence and current state

- The current source defaults still select `AspectRecalculation` and
  `NativeHorPlus`.
- The intended v1 default is `HorPlus` gameplay with `Auto` cinematic aspect
  and `GameplayHorPlus` cinematic FOV.
- `NativeHorPlus` remains an available alternative that is independent of the
  gameplay FOV setting.
- Custom-windowed limitations apply specifically to native
  `AspectRecalculation` transitions; arbitrary runtime aspects remain supported
  by HorPlus paths.

## Approved scope

- Update default values and generated INI descriptions in the configuration
  source/template.
- Keep the explanation user-facing and factual.
- Update repository-facing configuration examples only where needed to match
  the new defaults.

## Explicit non-goals

- No camera algorithm changes.
- No new hooks or state.
- No changes to hotkey behavior.
- No changes to diagnostics, Dialogue, resolver logic, or runtime ownership.
- No game launch or Git commit.

## Expected files or areas

- `src/config/feature_config.hpp`
- `src/config/config_repository.cpp`
- `src/config/config_template.cpp`
- `README.md`
- `NEXUS_DESCRIPTION.md` only if its default INI example is stale.

## Implementation batches

1. Update defaults and generated/template INI descriptions.
2. Align published configuration examples if required.

## Validation

- Run `test.cmd`.
- Run `build.cmd`.
- Run `build-diagnostic.cmd` if configuration source is shared by both builds.
- Run `git diff --check`.
- Perform read-only Git status/diff review against this plan.

## Risks and safe failure

- Default behavior changes for newly generated configurations. Existing user
  INI values remain explicit and are not overwritten by the parser.
- If validation fails, retain the previous defaults and report the failure.

## Stop conditions and phase gates

- Stop after the scoped files are updated and validation/review complete.
- Do not expand into camera architecture cleanup or release packaging.

## Expected final Git review

- Confirm only the planned configuration/documentation paths changed.
- Confirm no unrelated source or generated artifact changes are included.
