# Runtime Evidence Regression Corpus — Foundation

Date: 2026-09-20  
Scope: Batch 1 deterministic replay foundation. No production behavior or game
runtime was changed.

## Evidence sources audited

- Existing pure deterministic harnesses under `tests/` for HorPlus, Gameplay
  Baseline, restoration, cinematic FOV and Dialogue Candidate behavior.
- `GAMEPLAY_ASPECT_RESTORATION_PRODUCTION_CUTOVER.md` and its restoration
  decision evidence.
- `GAMEPLAY_BASELINE_BATCH2B_ENTER_CUTOVER.md` and the recorded cinematic
  baseline/output values.
- `CINEMATIC_FOV_MODE_MATCHGAMEPLAY_IMPLEMENTATION.md` for the two cinematic
  reference-input semantics.
- `DIALOGUE_CANDIDATE_CONTEXT_REPAIR.md` for coherent/contradictory Candidate
  context behavior.
- Existing runtime trajectory reports and supplied logs. Only stable numeric
  inputs and behavioral invariants were retained; timestamps, publication
  sequence IDs and pointer identity were not made expected outputs.

Raw game logs were not copied into tests. The repository does not contain a
canonical raw-log corpus, so this batch records only evidence already sufficient
to exercise existing pure production helpers.

## Added corpus

Machine-readable C++ fixtures were added under:

```text
tests/fixtures/runtime/gameplay/horplus_evidence.hpp
tests/fixtures/runtime/transitions/restoration_evidence.hpp
tests/fixtures/runtime/cinematics/cinematic_evidence.hpp
tests/fixtures/runtime/dialogue/candidate_evidence.hpp
```

The replay harness is:

```text
tests/regression/runtime_evidence_replay_harness.cpp
```

It calls existing production helpers and stores; it does not emulate UE, hooks,
native callbacks or a camera runtime.

## Automated scenarios and invariants

### Gameplay HorPlus

- Replays recorded native gameplay FOV `112.623` at aspect `3.55556` and checks
  the recorded HorPlus result approximately `143.132`.
- Checks the same native input/output pair through both existing HorPlus APIs.
- Sweeps arbitrary wider aspects rather than a 21:9/32:9 whitelist and requires
  finite outputs.
- Checks native-aspect identity/pass-through.
- Checks unsupported flags and NaN input fail closed.

### Gameplay restoration

- Replays retained aspect publication, invalidation, republish and restoration
  decision using the coherent restoration store.
- Checks the exact retained target is restored after the round trip.

### Cinematic

- Replays the recorded `90 @ 32:9 -> 126.87` NativeHorPlus endpoint.
- Replays the recorded GameplayHorPlus baseline input `112.623 -> ~143.132`.
- Checks that the two modes differ because only the reference/baseline input
  differs, while the common cinematic transform is used.
- The fixture does not treat stored transformed gameplay FOV as the cinematic
  native baseline.

### Dialogue Candidate hardening

- Replays coherent recorded source/target context and valid descent to promotion.
- Replays source change and target contradiction to cancellation without a
  phantom Active state.
- Pointer values are opaque continuity tokens only; exact addresses are not
  asserted as semantic facts.

The replay harness contains 16 deterministic assertion points, including the
arbitrary-aspect sweep and fail-closed cases.

## Integration and validation

`test.cmd` now builds and runs `runtime_evidence_replay_harness.exe` after the
existing harnesses. Existing harnesses were not rewritten.

Results:

- focused replay harness: PASS;
- full `test.cmd`: PASS;
- production `build.cmd`: PASS;
- `git diff --check`: PASS apart from normal line-ending warnings;
- known external Zydis C4201 warnings only;
- game launch: not performed.

## What remains runtime-only

The corpus does not prove:

- executable-specific resolver uniqueness or hook installation;
- actual native UE callback ordering and lifetime;
- visual framing in Gameplay/Cinematic/Dialogue/ZOOM transitions;
- real save/load, aspect changes, ADS/binocular and recovery integration;
- performance or logging volume in the game.

Those remain real-runtime checks because they cannot be established by replaying
pure mathematical/state evidence.

## Next highest-value additions

1. Add a compact recorded Gameplay `80/100/110` baseline sequence once its
   source/provenance is formalized, checking baseline retention across ADS.
2. Add a compact normal Gameplay -> Cinematic -> EXIT fixture once the
   episode-level reference provenance is fixed, including cached transformed
   ENTER exclusion.
3. Add a recovery trajectory fixture around native target convergence when the
   recovery terminal contract is finalized.
4. Add source/target continuity fixtures for additional Dialogue lifecycle
   boundaries without asserting unstable addresses.

## Final verdict

```yaml
runtime_evidence_corpus: ESTABLISHED
recorded_scenarios_automated: 4
deterministic_invariants_added: 16
arbitrary_aspect_coverage: PASS
negative_fail_closed_coverage: PASS
test_cmd_integration: PASS
tests: PASS
build: PASS
runtime: NOT_PERFORMED
```

No production behavior, hooks, release artifacts or game state were changed.
