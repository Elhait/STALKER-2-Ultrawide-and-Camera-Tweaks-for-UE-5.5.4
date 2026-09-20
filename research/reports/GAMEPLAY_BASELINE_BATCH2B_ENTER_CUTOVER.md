# GameplayBaseline → Cinematic ENTER Production Cutover — Batch 2B

Date: 2026-09-19

## Scope

This batch migrated only the Cinematic ENTER / GameplayHorPlus target-baseline
selection to the coherent `GameplayBaselineStore` snapshot.

No runtime game launch was performed.

## Production dataflow

Before this batch, Cinematic ENTER read the retained gameplay native FOV and
observed aspect as separate legacy atomics. The values could be observed from
different updates.

The new path performs one mutex-protected snapshot read:

```cpp
const auto gameplayBaseline = g_gameplayBaselineStore.Read();
```

The snapshot is validated once and then used by the pure target-selection
helper. The helper returns either:

- `GameplayBaseline.nativeFov.value` for a usable GameplayHorPlus baseline;
- the authored ENTER FOV as the safe fallback.

The optional `GameplayBaseline.horPlusFov` is diagnostic evidence only. It is
never used as the cinematic target baseline.

## Mode semantics

### NativeHorPlus

NativeHorPlus ignores GameplayBaseline completely. It keeps the authored
cinematic ENTER value as the target baseline and preserves the existing unified
cinematic transform.

### GameplayHorPlus

GameplayHorPlus uses the retained native Gameplay baseline only when the
baseline is usable:

- snapshot is valid;
- native FOV is valid, finite and in the established range;
- native input space/provenance are native Gameplay writer evidence;
- aspect is valid;
- observation sequence is valid.

When usable, the target is `GameplayBaseline.nativeFov.value`.

### Fallback

If GameplayHorPlus has no usable baseline, the target falls back to the
authored ENTER FOV. The fallback uses the same unified transform and therefore
has NativeHorPlus behavior. It does not use legacy distributed state, a default
value, the optional transformed evidence, or hardcoded 90.

## Cinematic aspect policy

The stored GameplayBaseline aspect is not used as the effective cinematic
aspect. Cinematic ENTER continues to use the existing `ResolveCinematicAspect()`
result and the existing transform/math contract.

This keeps Gameplay baseline provenance separate from cinematic aspect policy.

## Retained legacy responsibilities

Legacy globals remain in place. This batch did not migrate or delete:

- Dialogue invalidation responsibilities;
- gameplay mode transition aspect restoration;
- other legacy retained-state users;
- hooks, resolvers, ZOOM/ADS, CameraState or unrelated FOV paths.

The only removed legacy reads are the two distributed reads used by the
Cinematic ENTER target-selection decision.

## Diagnostics

Added `CINEMATIC_BASELINE_SELECTION` telemetry records:

- selected source;
- requested mode;
- baseline validity;
- native and optional HorPlus evidence;
- gameplay aspect and observation sequence;
- authored ENTER value;
- selected target baseline;
- effective cinematic aspect;
- transformed result.

The diagnostic values are logged after the coherent snapshot has been copied,
outside the store lock.

## Harness coverage

Added deterministic coverage for:

- NativeHorPlus ignoring a valid GameplayBaseline;
- invalid-baseline authored fallback;
- transformed Gameplay baseline native target;
- native-only pass-through baseline;
- AspectRecalculation-like native-only baseline;
- malformed native baseline fail-safe behavior;
- cinematic aspect independence across 16:9, 21:9 and 32:9;
- stored Gameplay horPlus evidence not being reused as the final target/output;
- preservation of existing native-aspect behavior.

The existing gameplay and zoom harness link inputs were updated for the shared
cinematic helper dependency.

## Validation

- Full `test.cmd`: PASS.
- Cinematic FOV cutover harness: PASS.
- GameplayBaseline harness: PASS.
- Zoom transition HorPlus harness: PASS.
- `build.cmd`: PASS.
- `git diff --check`: PASS; only normal line-ending warnings were reported.
- Game launch/runtime validation: NOT PERFORMED by task scope.

## Future combined runtime matrix

The next runtime session should verify:

1. Establish a valid GameplayHorPlus baseline at 32:9.
2. Enter a cinematic with forced 21:9 and confirm
   `CINEMATIC_BASELINE_SELECTION=GameplayBaselineNative`.
3. Verify target selection uses baseline native FOV while effective cinematic
   aspect remains 21:9.
4. Compare the same cinematic under NativeHorPlus.
5. Use F12 between cinematics to compare both modes without changing an active
   cinematic.
6. Switch gameplay to AspectRecalculation, establish a valid native-only
   baseline, and verify GameplayHorPlus uses it without requiring transformed
   evidence.
7. Exercise invalid/unavailable baseline fallback if reproducible.
8. Confirm Dialogue, ZOOM, cinematic EXIT/recovery and legacy aspect restoration
   remain unchanged.

Runtime acceptance should compare the baseline source, native target, effective
cinematic aspect and final transformed output, not only visual appearance.

## Status

```yaml
cinematic_enter_cutover: PASS_STATIC_HARNESS
native_horplus_regression: PASS_STATIC_HARNESS
gameplay_horplus_transformed_baseline: PASS_STATIC_HARNESS
gameplay_horplus_pass_through_baseline: PASS_STATIC_HARNESS
gameplay_horplus_aspect_recalculation_baseline: PASS_STATIC_HARNESS
cinematic_aspect_independence: PASS_STATIC_HARNESS
legacy_dialogue_responsibility: UNCHANGED
legacy_aspect_restoration_responsibility: UNCHANGED
static_harness_validation: PASS
build: PASS
runtime_validation: NOT_PERFORMED
```

