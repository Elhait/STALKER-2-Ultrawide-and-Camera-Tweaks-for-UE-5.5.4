# v1.0.0 Build Object Output Location — Task Plan

## Objective

Keep generated C/C++ object files out of the repository root by directing
`build.cmd` to `build-artifacts/obj` and removing the current root-level
generated `.obj` files.

## Established evidence

- `build.cmd` compiles the production ASI successfully but currently lets `cl`
  place object files in the repository root.
- `.gitignore` already excludes `*.obj` and `build-artifacts/`.
- Root-level `.obj` files are generated outputs, not source or release assets.

## Approved scope

- Update only `build.cmd` to create and use `build-artifacts/obj`.
- Remove only current root-level `.obj` files after exact inventory.

## Non-goals

- No source, hook, resolver, configuration, test or runtime behavior change.
- No removal of `.asi`, release archives, research assets or non-root outputs.

## Validation

1. Production build succeeds.
2. Generated `.obj` files appear under `build-artifacts/obj`.
3. No root-level `.obj` files remain after build.
4. `git diff --check` passes.

## Risks and stop conditions

- Stop if `/Fo` causes an object-name collision or changes linker behavior.
- Stop if a root `.obj` has ambiguous provenance; do not delete it.
- Do not run the game.

## Final review

Review changed paths, output locations and ignored status. Archive this plan
after the bounded batch completes.
