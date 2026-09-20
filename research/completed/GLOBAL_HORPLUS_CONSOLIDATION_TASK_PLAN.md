# Global HorPlus Consolidation — Task Plan

## Objective

Consolidate and harden the existing Global HorPlus Gameplay and Cinematic
paths, expand deterministic regression coverage, remove only proven-obsolete
HorPlus diagnostics/state, and produce the consolidation and next-runtime
matrix reports.

## Established evidence and current state

- Gameplay HorPlus, NativeHorPlus, GameplayHorPlus and unified cinematic
  transform helpers already exist.
- GameplayBaseline, CameraFovObservation and GameplayAspectRestorationState are
  established production stores.
- Cached transformed ENTER has an existing numeric guard and diagnostic
  provenance evidence, but explicit input-space provenance remains to audit.
- The runtime evidence corpus foundation is integrated into `test.cmd` and
  currently passes.
- No runtime launch is authorized for this batch.

## Approved scope

1. Repository-wide production FOV/HorPlus dataflow audit.
2. Bounded cached-ENTER provenance repair only if current evidence supports it.
3. Gameplay and Cinematic arbitrary-aspect/FOV/flags/invalid-input test
   expansion using existing pure helpers.
4. Cross-state and negative deterministic scenarios using existing stores and
   fixtures; no UE simulation.
5. Remove only HorPlus-specific state/diagnostics proven obsolete by dataflow.
6. Create `GLOBAL_HORPLUS_CONSOLIDATION.md` and
   `GLOBAL_HORPLUS_RUNTIME_MATRIX.md`.

## Explicit non-goals

- No Dialogue or ZOOM ownership redesign.
- No new UE hooks unless a concrete, unavoidable production defect is proven.
- No unrelated architecture rewrite, release work, commit or game launch.
- No conversion of UNKNOWN evidence into BLOCKED.

## Expected files/areas

- Existing Gameplay/Cinematic pure helpers and tests.
- Runtime evidence fixtures/replay harness where additional recorded evidence
  is justified.
- `test.cmd` for new harness coverage.
- Two required reports and this plan.

## Batches

1. Audit current production callsites, stores, guards and diagnostics.
2. Expand deterministic Gameplay/Cinematic and negative coverage.
3. Implement only locally proven guard/consolidation repairs, if any.
4. Run focused/full tests, production build and diff review.
5. Write consolidation report and compact runtime matrix; stop before runtime.

## Risks and safe failure

- Preserve NativeHorPlus behavior as the control contract.
- Keep GameplayBaseline native FOV distinct from optional transformed evidence.
- Invalid/unknown provenance must pass through or fall back using existing safe
  behavior; never guess ownership.
- Any repair that requires new runtime evidence is deferred and recorded in the
  runtime matrix rather than implemented speculatively.

## Validation

- Existing and new deterministic harnesses through `test.cmd`.
- Production `build.cmd`.
- `git diff --check` and read-only Git review against this plan.
- Runtime explicitly `NOT_PERFORMED`.

## Stop conditions

- Stop a sub-batch if it requires a new hook or changes Dialogue/ZOOM ownership.
- Stop if NativeHorPlus equivalence cannot be proven deterministically.
- Stop after reports and validation; do not launch the game.

## Final review

Confirm changed paths match this plan, distinguish completed/gap/deferred/runtime
items, archive this plan to `research/completed/`, and preserve all pre-existing
user changes.
