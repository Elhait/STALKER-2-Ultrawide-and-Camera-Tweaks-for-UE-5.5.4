# MatchGameplay Mathematics — Offline Research Task Plan

## Objective

Determine an evidence-backed mathematical contract for aligning authored
cinematic FOV with the current gameplay native/HorPlus baseline while preserving
cinematic authored variations.

## Established evidence and current state

- At gameplay FOV setting 90, cinematic HorPlus endpoint and gameplay HorPlus
  endpoint coincided at approximately 126.87.
- At setting 112, stable native gameplay endpoint was 112.623 and its HorPlus
  result was 143.132, while authored cinematic 90 became 126.87.
- Runtime showed a visible EXIT discontinuity for the latter case.
- Native recovery interpolated toward the gameplay native endpoint; the issue is
  framing endpoint mismatch, not a confirmed double transform.
- Existing gameplay and cinematic HorPlus math already exists and must be
  treated as the source of truth for this research.

## Approved scope

- Read-only source/formula inspection.
- Offline mathematical comparison using existing runtime values and trajectories.
- Define candidate MatchGameplay contracts and reject unsafe ones.
- Record required runtime evidence for a later diagnostic-only validation.

## Explicit non-goals

- No production code or formula changes.
- No new hooks, telemetry or configuration parameters.
- No Ghidra/EXE analysis.
- No game launch.
- No replacement of authored cinematic FOV with a constant gameplay FOV.
- No assumption that numeric FOV differences are linear screen-space changes.

## Expected files or areas

- `src/gameplay/horplus_gameplay.*`
- `src/cinematics/cinematic_fov.*`
- existing MatchGameplay and cinematic research reports.
- new offline research report under `research/reports`.

## Research batches

1. Inventory current tangent-space HorPlus formulas and reference/aspect
   semantics.
2. Model the observed 90 and 112 endpoints in native and HorPlus spaces.
3. Compare candidate contracts: direct replacement, additive FOV offset,
   multiplicative ratio and tangent-space framing transfer.
4. Preserve authored cinematic variation as an explicit invariant.
5. Classify each candidate as viable, rejected or requiring runtime evidence.

## Validation

- Check derivations against the existing HorPlus implementation.
- Check candidate formulas against the observed 90/112 endpoint cases.
- Run no build or game validation unless the research reveals a source inconsistency.
- Run `git diff --check` for the report/plan changes.

## Risks and safe-failure behavior

- A visually plausible additive or degree-based correction may violate tangent-space
  projection semantics; reject it unless derived from the existing formula.
- A contract that forces every cinematic sample to one gameplay endpoint destroys
  authored cinematic variation; reject it.
- If the evidence cannot distinguish candidate formulas, leave the result as
  runtime-required and do not implement.

## Stop conditions and phase gates

- Stop after the offline report and candidate classification.
- Do not promote a candidate to production without a separate diagnostic/runtime
  validation task.

## Final review

Verify that only the plan and research report changed, no production behavior was
modified, and the report clearly separates established math, hypotheses and
runtime-required questions.
