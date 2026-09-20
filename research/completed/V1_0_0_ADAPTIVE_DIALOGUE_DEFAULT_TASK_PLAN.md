# v1.0.0 Adaptive Dialogue Default Task Plan

## Objective

Change the user-facing default dialogue zoom policy from `Reduced` to `Adaptive` so the default preserves the native optical zoom strength relative to the current Gameplay FOV.

## Established evidence and current state

- `Adaptive` and `Reduced` are existing production policies with deterministic math and runtime validation.
- `Adaptive` preserves native optical zoom strength; `Reduced` applies half-strength.
- Gameplay `HorPlus` preserves changing native Gameplay FOV values, making `Adaptive` the more direct native-experience default.
- No dialogue algorithm, lifecycle, recovery or hotkey ordering change is requested.

## Approved scope

- Change the default configuration value and all user-facing default examples.
- Update deterministic configuration expectations.
- Keep explicit `Reduced` support and hotkey cycle semantics unchanged.

## Explicit non-goals

- No change to Adaptive/Reduced formulas.
- No Dialogue lifecycle, recovery, Candidate or policy snapshot redesign.
- No gameplay/cinematic behavior change.
- No new hooks or runtime session.

## Expected files or areas

- `src/config/feature_config.hpp`.
- `src/config/config_template.cpp` and `src/config/config_repository.cpp`.
- Release INI, README/Nexus/GitHub release text and relevant config harnesses.

## Implementation batches

1. Update default values and generated/release configuration text.
2. Update deterministic expectations and user-facing documentation.

## Validation

- Relevant configuration/dialogue harnesses.
- `test.cmd`.
- `git diff --check`.
- Static review confirming `Reduced` remains selectable and the cycle remains unchanged.

## Risks and rollback/safe-failure behavior

- Existing user INI values remain user-controlled; only newly generated/default configuration changes.
- Invalid values continue to use existing parser fallback behavior.
- If tests reveal an unintended lifecycle change, stop and revert only this bounded default change.

## Stop conditions and phase gates

- Stop after deterministic validation and documentation synchronization.
- Do not launch the game or change production dialogue math.

## Expected final Git review

- Confirm only default/configuration documentation and relevant test expectations changed.
- Report runtime as not performed for this default-only batch.
