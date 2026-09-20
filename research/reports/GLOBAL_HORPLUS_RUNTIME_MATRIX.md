# Global HorPlus — Combined Runtime Matrix

Purpose: record the single combined runtime gate and its current result. Do not
repeat exhaustive FOV/aspect sweeps; those are now covered by `test.cmd`.

Current result: `GLOBAL_HORPLUS_RUNTIME_PASS` on 2026-09-20.

## Preconditions

- Use the current canonical production ASI and matching game executable.
- Record resolution/aspect, Gameplay mode, Cinematic FovMode and configuration.
- Enable reusable diagnostics only where needed for provenance and lifecycle
  confirmation.
- No claim is valid without the resulting runtime log and executable identity.

## Scenarios

1. **Gameplay HorPlus**
   - Idle gameplay at the selected noncanonical/custom aspect.
   - ADS and binocular transitions, including return to ordinary gameplay.
   - Confirm visible framing and native/transformed observation provenance.

2. **F11 mode round trip**
   - `HorPlus -> AspectRecalculation -> HorPlus` without reload.
   - Confirm restoration write, current flags and successful return to the same
     effective aspect.
   - Repeat one ADS transition after each mode.
   - Include one live aspect change before and after the round trip.

3. **Cinematic NativeHorPlus**
   - Gameplay idle -> cinematic ENTER -> dynamic cinematic -> EXIT/recovery.
   - Confirm authored cinematic framing, cached ENTER bypass and no double
     transform.

4. **Cinematic GameplayHorPlus**
   - Same or equivalent cinematic with retained GameplayBaseline.
   - Confirm baseline source is native Gameplay FOV, cinematic aspect policy is
     unchanged, and EXIT returns to gameplay framing.

5. **Dynamic aspect continuity**
   - Exercise a compact sequence such as `16:9 -> 2.4 -> 3:1 -> 32:9` and
     return through a different custom aspect to `16:9`.
   - Confirm the next native writer sample uses the current aspect and the
     retained native Gameplay baseline is not replaced by old transformed
     evidence.
   - Check Auto follows the current display/client aspect and forced cinematic
     policies remain their configured policy targets.

6. **F12 cinematic mode switch**
   - Switch `Cinematics.FovMode` between NativeHorPlus and GameplayHorPlus at
     the supported hotkey boundary.
   - Confirm the next cinematic uses the selected mode and the active cinematic
     is not retroactively misclassified.

7. **Dialogue coexistence**
   - One real Dialogue before or between gameplay/cinematic transitions.
   - Confirm Dialogue policy/lifecycle remains independent and no phantom
     Dialogue state appears during cinematic or generic zoom.

8. **Save/load and camera recreation**
   - Include one save/load or direct camera recreation where practical.
   - Confirm retained GameplayBaseline is not manufactured from cached transformed
     cinematic input and recovery remains visually correct.

## Acceptance evidence

```text
Gameplay native -> exactly one Gameplay HorPlus result
ADS/binocular -> no Dialogue/Cinematic ownership contamination
F11 round trip -> restoration target and writeSuccess confirmed
NativeHorPlus -> authored cinematic framing preserved
GameplayHorPlus -> native Gameplay baseline selected
F12 -> next-cinematic mode selection confirmed
ENTER/EXIT -> no cached double transform or stale baseline
Dialogue -> independent lifecycle and policy snapshot
save/load/recreation -> no invented baseline/provenance
visual -> human confirmation only; not replaced by logs
```

## Explicitly offline-covered and not repeated manually

- broad FOV sweeps;
- arbitrary aspects through at least `4:1`, including noncanonical values;
- flags `0x4`/`0x5` and unsupported flags;
- NaN/Inf/invalid values;
- baseline contamination/pass-through stale-state negatives;
- NativeHorPlus identity and GameplayHorPlus math equivalence;
- Candidate source/target contradiction decisions.

The following are also deterministic and no longer need exhaustive manual
repetition: FOV sweeps across changing aspect inputs, Gameplay mode transition
contracts, and stale transformed evidence replacement in GameplayBaseline.

## Runtime result

The combined session passed Gameplay HorPlus, NativeHorPlus,
GameplayHorPlus, F11/F12 switching, Auto/forced cinematic aspect, ADS/binocular,
Dialogue coexistence, save/load/camera recreation and legacy-aspect cleanup
regression. See `GLOBAL_HORPLUS_RUNTIME_PASS.md` for executable identity and
the evidence qualification.

## Still runtime-only for future regressions

- actual resolver/hook installation and callback ordering;
- visual framing for ADS/binocular and both cinematic FOV modes;
- Auto versus forced cinematic aspect behavior at actual display resolution;
- F11/F12 live switching without reload;
- save/load or camera recreation behavior;
- Dialogue coexistence and post-cinematic recovery;
- the pending legacy-aspect cleanup regression.

Runtime status for this matrix: `PASS` for the recorded 2026-09-20 session.

## Realtime / lifecycle repair additions for next combined session

The following scenarios are accumulated for runtime validation; deterministic
selection/gate behavior is covered offline, but native callback ordering and
OS lifecycle behavior remain runtime-only:

- F9 immediately before cinematic ENTER: one coherent aspect policy and FOV
  mode for the complete cinematic.
- F9 immediately after ENTER, F9/F12 during CinematicActive: current
  cinematic remains internally coherent; changed selection applies to the next
  cinematic.
- EXIT followed by selection changes and a new ENTER: no stale selection
  generation or stale active snapshot.
- Initialization/rollback and observation-only cinematic lifecycle: callbacks
  remain native/pass-through until publication is complete.
- Shutdown while callbacks/workers are active: stopping gates close before
  teardown and no callback uses released resources.
- Dialogue discovery reset followed by a native callback: diagnostic TLS starts
  from the new reset generation.

These additions do not change the recorded `PASS` result above; they are
future runtime checks for the lifecycle repair batch.

## Persistence / runtime-state repair additions

The following remain runtime-only after the deterministic P1–P4 repair:

- duplicate cinematic ENTER does not overwrite the accepted active selection;
- F9/F12 changes during an active cinematic apply only to the next lifecycle;
- closed/stopping cinematic aspect fallback writes the native aspect field
  without changing the flags field, including when flags are unavailable;
- Dialogue source/FOV context invalidation behaves consistently in both
  AspectRecalculation and HorPlus gameplay paths;
- CinematicExiting terminal behavior, post-cinematic exclusion completion and
  unreadable/invalid recovery-target liveness.

These additions do not change the recorded runtime PASS; no game launch was
performed for the persistence/runtime-state repair batch.
