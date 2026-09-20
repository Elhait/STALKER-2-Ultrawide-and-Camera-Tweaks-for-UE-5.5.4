# Wideboy ADS owner integration task plan

## Objective

Implement the bounded ADS lifecycle aggregator and Dialogue negative-exclusion
seam as a candidate build, using the runtime-confirmed Wideboy 2.0.5 paired
transition samples.

## Established evidence and current state

- Steam 2.0.5 Wideboy IN/OUT anchors are unique and share one native function.
- Runtime covered rifle, shotgun and pistol configurations with magnified,
  reflex and no-optic aiming, plus rapid ADS and cinematic recovery control.
- The diagnostic observed 18 paired IN/OUT runs with no cinematic false ADS
  records.
- Callback samples are blend samples, not one-shot input events.
- The first IN sample can contain `RAX+0x4C=0` and `RAX+0x50=0`; therefore
  validity must not require a complementary sum of one.

## Approved scope

- Add a small pure `AdsLifecycle` state machine.
- Add unit/harness coverage for transitions, reversal, completion and
  conservative invalid-sample handling.
- Integrate the state as a negative Dialogue exclusion in the candidate build.
- Keep the Wideboy resolver fail-closed and read-only.
- Build a separate candidate/diagnostic ASI only.

## Explicit non-goals

- No positive Dialogue discriminator or target-70 classifier.
- No removal of the generic Dialogue heuristic in this batch.
- No Dialogue FOV math, hotkey snapshot, HorPlus, Cinematics or
  AspectRecalculation changes.
- No production stable ASI replacement and no game launch by the agent.

## Expected files or areas

- `src/dialogue/ads_lifecycle.hpp/.cpp`
- `src/plugin/runtime.cpp`
- `src/hooks/signatures/signature_definitions.hpp`
- `tests/dialogue/ads_lifecycle_harness.cpp`
- build wrapper and the existing ADS research report

## Batches and validation

### Batch 1 — Pure state machine

Implement the four states and conservative invalid-sample semantics. Run the
Dialogue harness and existing `test.cmd`.

### Batch 2 — Candidate integration

Connect validated Wideboy observations to the state and reject Dialogue while
state is `Entering`, `Active` or `Exiting`. Build a separate ASI.

### Final review

Run `git diff --check`, inspect the exact diff/status against this plan and
report build/runtime status separately. Runtime remains user-operated.

## Risks and safe failure

- Resolver/hash/operand mismatch leaves native behavior untouched.
- Invalid samples while ownership is active preserve exclusion; they never
  reopen Dialogue classification.
- Invalid samples while inactive do not assert ADS ownership.
- The observer writes no camera/FOV/aspect/flags/register state.

## Stop conditions

- Stop on harness failure, compile failure, resolver ambiguity or unexpected
  changes outside the approved areas.
- Do not promote the candidate to the stable ASI before runtime validation.
