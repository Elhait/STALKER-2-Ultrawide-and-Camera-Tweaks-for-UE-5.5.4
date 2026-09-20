# HorPlus Zoom Transition Integration — Task Plan

## Objective

Integrate the validated native `ZOOM_IN`/`ZOOM_OUT` transition callbacks into the Gameplay HorPlus path so every valid native zoom sample is transformed with the existing HorPlus math.

## Established evidence and current state

- The validated Wideboy signature pair produces complementary native zoom transition samples.
- Runtime evidence confirms the pair for ADS and controller camera pull, including a trajectory `90 → 34.5502 → 90`.
- The pair must not be interpreted as ADS ownership, Dialogue ownership or Cinematic ownership.
- Existing `TryTransformHorPlus()` and the gameplay writer path are validated and must remain the single transformation implementation.
- Dialogue and Cinematics are independent subsystems and are outside this batch.

## Approved scope

- Rename the production-facing diagnostic/integration terminology from `WIDEBOY_ADS` to neutral `ZOOM_IN`/`ZOOM_OUT`.
- Define the bounded integration seam between the native zoom callbacks and Gameplay HorPlus.
- Apply HorPlus to each valid native zoom sample without change-only suppression.
- Preserve the native transition trajectory and avoid feeding transformed output back as input.
- Add static/unit coverage for single-transform and sample-by-sample behavior.

## Explicit non-goals

- No Dialogue classifier, lifecycle, policy or exclusion changes.
- No Cinematic hook, coordinator or recovery changes.
- No new global FOV router.
- No ADS detection or ADS-specific state semantics.
- No change to `AspectRecalculation`.
- No new aspect mathematics.
- No runtime game launch in this batch unless separately approved after build/harness validation.

## Expected files or areas

- `src/plugin/runtime.cpp`
- `src/hooks/signatures/signature_definitions.hpp`
- `src/gameplay/horplus_gameplay.*` only if a pure helper is required
- `tests/gameplay/` and/or a dedicated zoom-transition harness
- `build.cmd` or a dedicated build wrapper only if required for the bounded candidate
- this plan and a research report/task log entry after review

## Batches and validation

### Batch 1 — Static integration design

- Confirm the existing Wideboy callback registers and sample values.
- Define the exact native FOV source at each callback.
- Define the non-feedback invariant: only native input is transformed; transformed output is never stored as the next native input.
- Define fail-closed behavior for unreadable state, invalid FOV/aspect/flags, wrong mode, non-gameplay coordinator, and duplicate hook installation.

Validation: source inspection and diff-free design review.

### Batch 2 — Minimal implementation

- Introduce neutral `ZOOM_IN`/`ZOOM_OUT` names and narrow production integration.
- Keep Dialogue/Cinematics code paths untouched.
- Ensure the existing gameplay writer remains correct for baseline gameplay and that zoom callbacks do not create a second transformation.

Validation: compile, `git diff --check`, gameplay HorPlus harness, and zoom-transition unit/harness coverage.

### Batch 3 — Post-change review

- Inspect Git status, affected diff and recent history.
- Compare changed paths with this plan.
- Record completed, remaining, deferred, blocked and not-runtime-validated items.
- Archive this plan only after the implementation and validation are complete; otherwise keep it active.

## Risks and safe-failure behavior

- The native zoom signatures are executable-version-specific; resolver identity and uniqueness checks remain mandatory.
- If either zoom signature is ambiguous or instruction validation fails, do not install the zoom integration and retain native behavior.
- If the callback cannot read a valid native FOV/aspect/flags, leave the sample untouched.
- If the baseline gameplay writer and zoom callback can both transform the same sample, stop implementation and resolve ownership before building.
- No rollback operation is required beyond disabling the new bounded integration or retaining the existing diagnostic-only path; stable unrelated modes remain unchanged.

## Stop conditions and phase gates

- Stop before source edits if the callback's native FOV source cannot be established statically.
- Stop before build if double-transform avoidance cannot be proven from the source path.
- Stop before runtime if unit/harness validation fails or if Dialogue/Cinematics files are changed unintentionally.

## Expected final Git review

- `git status --short`
- relevant `git diff --stat` and `git diff`
- confirm only approved paths changed
- confirm no stable ASI was overwritten
- report build/harness evidence separately from runtime evidence
