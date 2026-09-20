# Unified Cinematic FOV Baseline Implementation

Date: 2026-09-19

## Result

NativeHorPlus and GameplayHorPlus now share one cinematic FOV transformation
pipeline. The runtime selects only the target baseline:

- `NativeHorPlus`: target baseline equals the cinematic reference FOV;
- `GameplayHorPlus`: target baseline is the retained valid Gameplay native FOV;
- invalid Gameplay baseline: target baseline falls back to the cinematic
  reference FOV.

The common pipeline performs the tangent-space baseline transfer followed by
the single HorPlus aspect conversion. Existing public helper functions remain
as thin delegating wrappers for compatibility and tests.

## Validation

- Full `test.cmd`: PASS.
- `HorPlus gameplay harness`: PASS, including common NativeHorPlus identity and
  GameplayHorPlus wrapper-equivalence assertions.
- `build.cmd`: PASS with existing external Zydis C4201 warnings.
- `git diff --check`: PASS; only normal line-ending conversion warnings.
- No game launch.

## Runtime status

Not runtime-validated after this refactor. The previously validated runtime
contracts remain the acceptance targets:

```text
NativeHorPlus:   authored 90 -> 126.87 at 32:9
GameplayHorPlus: authored 90, G=112.623 -> 143.132 at 32:9
invalid G:       same result as NativeHorPlus
```

## Scope review

Changed only the cinematic FOV math seam, its ENTER baseline selection and
deterministic math coverage. Retained baseline propagation, cached transformed
provenance, Dialogue, ZOOM, AspectRecalculation, CameraState and resolver
behavior were not changed.
