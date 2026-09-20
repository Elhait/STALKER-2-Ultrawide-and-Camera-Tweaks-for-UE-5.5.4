# MatchGameplay Diagnostic Prediction — Task Plan

## Objective

Add a diagnostics-gated, read-only prediction of the proposed MatchGameplay
transfer without applying it to the game.

## Established evidence and current state

- `R=90` is not an established universal contract.
- Exact cinematic ENTER native input `E` must remain separate from semantic
  reference `R`.
- The existing gameplay writer telemetry already provides native and HorPlus
  values from the same observation.
- The current runtime can show a pre-ENTER gameplay pair, but it is not yet a
  production-confirmed baseline classifier.

## Approved scope

- Add diagnostic-only state for the last valid pre-ENTER gameplay observation.
- Capture the exact native cinematic ENTER observation separately.
- Log candidate native/HorPlus values for cinematic writer samples using `E` as
  the diagnostic reference candidate.
- Gate all records on `[Diagnostics] Enabled=true`.

## Explicit non-goals

- Do not apply the candidate to `XMM0` or any game memory.
- Do not change HorPlus, Cinematics, Dialogue, ZOOM, CameraState or recovery.
- Do not call the candidate `ConfiguredGameplayFov` or production baseline.
- Do not treat `E` as proven semantic `R` in production.
- No game launch, Ghidra analysis or new resolver.

## Expected files

- `src/plugin/runtime.cpp`
- `build.cmd`
- `research/reports/STABLE_GAMEPLAY_BASELINE_DIAGNOSTIC.md`
- new diagnostic research report.

## Validation

- Existing harnesses and full `test.cmd`.
- Canonical `build.cmd`.
- `git diff --check` and scoped Git review.
- No runtime in this batch.

## Risks and safe failure

- If the pre-ENTER pair is unavailable or any input is non-finite, log
  `candidateAvailable=false` and do not invent a value.
- Candidate output must be diagnostic-only and must not overwrite the writer
  register or production cache.
- Keep exact ENTER observation distinct from candidate semantic reference.

## Stop conditions

- Stop if implementation requires production FOV changes or a new hook.
- Stop after static/build validation; runtime prediction is deferred to the
  next combined session.

## Final review

Verify the candidate is computed from one episode's captured `E`, the pre-ENTER
gameplay pair and current cinematic aspect, with no production authority.
