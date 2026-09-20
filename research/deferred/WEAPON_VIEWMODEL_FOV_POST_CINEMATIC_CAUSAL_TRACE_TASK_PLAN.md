# Post-Cinematic Weapon/Viewmodel FOV Causal Trace Task Plan

## Objective

Identify the downstream state or consumer through which native ADS refreshes the
weapon/viewmodel presentation after cinematic EXIT, without changing the stable
ASI or introducing a hard-coded weapon FOV.

Status: Current-image static revalidation complete; no runtime candidate promoted

## Current accepted state

### Confirmed runtime negatives

- Camera `+0x234` transition during the visual correction: not observed.
- Camera `+0x262` transition during the visual correction: not observed.
- Camera replacement during the captured ADS window: not observed.
- Mesh assignment during the captured ADS window: not observed.
- `+0x265` primitive setter during the captured ADS window: not observed.
- Setter-driven `MarkRenderStateDirty` activity during the captured ADS window: not observed.

### Known positive event

- Native ADS changes world FOV during the correction window, approximately
  `90.6557 → 83.6`.
- The weapon/viewmodel becomes visually correct during the native ADS path.

### Active frontier

- ADS IN/OUT resolve uniquely in matching 2.0.4 and share
  `FUN_1406ABB9C` (entry RVA `0x6ABB9C`).
- That function owns a compact ADS transition state around `RAX+0x4C…+0x64`
  and calls several direct update/dispatch targets.
- The downstream first-person/viewmodel consumer and ownership relationship
  remain unresolved.

## Approved scope

- Reconcile the current static ADS anchors and setter address terminology.
- Rank only direct callees of `FUN_1406A81BA` using local callsite context and
  bounded semantic indicators.
- Analyze local ownership/data-flow for the ADS state fields `+0x4C…+0x64`.
- Produce a short candidate ranking with explicit evidence and rejection reasons.
- If a candidate is sufficiently specific, propose—but do not create—a bounded
  read-only runtime presence/context trace.

## Explicit non-goals

- No writes to camera, weapon, primitive or render state.
- No weapon FOV slider, hard-coded weapon FOV or FOV formula.
- No new runtime tracer in this phase.
- No broad renderer search or raw executable-wide offset scan.
- No changes to the stable gameplay/cinematic implementation.
- No task-log entry unless this theory is rejected by evidence or a later implementation is completed and tested.

## Expected files or areas

- Read-only Ghidra scripts and reports under `02-Research/Ghidra`.
- This plan file and the research task log.

## Batches

### Batch 1 — observer implementation and resolver safety

- Reuse only the validated current camera-writer signature family where possible.
- Add read-only snapshots of the five approved fields.
- Gate every read on a validated camera pointer and finite/sane scalar values.
- Keep the observer independent of WIDEBOY and production writes.

Validation: source review, unique resolver/decode checks, successful isolated build and load log. No causal claim from load alone.

Batch 1 status — implementation/build PASS, runtime pending:

- Added `src/weapon_viewmodel_fov_state_observer_204.cpp`.
- Added `build-artifacts/test-scripts/build-weapon-viewmodel-fov-state-observer-204.cmd`.
- The observer resolves the current 2.0.4 camera-writer signature uniquely and reads only `+0x230`, `+0x234`, `+0x254` and `+0x262` from the writer's camera object.
- The isolated build completed successfully and produced `build-artifacts/test-asi/STALKER2WeaponViewmodelFovStateObserver204.asi`.
- No runtime causal conclusion has been made; Batch 2 still requires separate ADS and menu runs.

### Batch 2 — separate ADS and menu runtime runs

- Capture the same fields immediately before and after native ADS correction.
- Capture the same fields immediately before and after native menu correction in a separate run.
- Record executable SHA-256, ASI SHA-256, resolution/aspect and event ordering.

Validation: user-supplied runtime logs and visual reproduction at 21:9 or 32:9. Do not combine ADS and menu runs into one causal dataset.

Batch 2 result — scalar branch runtime-negative / primitive branch promoted:

- In the supplied two-run log, `camera +0x234` remained `90` and `camera +0x262` remained `0x1` across the observed writer snapshots.
- A camera pointer change was observed between test segments, but it was not temporally marked as an ADS/menu correction and is not causal evidence by itself.
- The observer also showed that `+0x230` changes continuously during the native world-FOV recovery, so those changes are not a useful weapon/viewmodel correction marker.
- Direct `+0x234/+0x262` state-transition hypothesis is therefore not supported by this runtime dataset.
- The next runtime branch is ADS-only primitive/reacquisition observation. The separate menu branch is deferred until ADS produces a concrete state transition.

### Batch 3 — discriminator classification

- Classify the result as `+0x234 writer`, shared refresh/ownership, or downstream presentation state.
- Promote a next bounded branch only if the values change at the correction event and the direction matches the visual correction.

Validation: compare the observed state transitions against the predeclared decision tree; otherwise close the candidate as non-causal.

### Batch 4 — bounded weapon/viewmodel primitive observation

- Use the WIDEBOY-derived current path only to locate the actual weapon/viewmodel primitive; do not port its slider, writes or render-state calls.
- Observe primitive pointer, `primitive +0x265`, camera pointer, `camera +0x234` and `camera +0x262` immediately before and after ADS correction.
- Keep ADS IN/OUT as the primary event boundary. Do not add menu tracing until ADS establishes a concrete native refresh/reacquisition contract.
- Promote `MarkRenderStateDirty` only if primitive identity and `+0x265` remain unchanged while the visual correction is tied to a validated native refresh event.

Validation: one isolated 2.0.4 build, one ADS-focused runtime run, visual confirmation and timestamp-correlated log. No production implementation in this batch.

Batch 4 status — implementation/build PASS, runtime pending:

- Added `src/weapon_viewmodel_primitive_ads_observer_204.cpp`.
- Added `build-artifacts/test-scripts/build-weapon-viewmodel-primitive-ads-observer-204.cmd`.
- The observer resolves the WIDEBOY-derived ADS IN/OUT and mesh-assignment signatures plus the existing camera-writer signature; every resolver requires exactly one executable match.
- It records live assignment candidates (`object`, `[object+0x20]`, both `+0x265` values), ADS IN/OUT register markers and camera controls.
- It contains no camera/primitive writes, no setter call and no `MarkRenderStateDirty` call.
- The isolated build completed successfully and produced `build-artifacts/test-asi/STALKER2WeaponViewmodelPrimitiveAdsObserver204.asi`.
- The first runtime attempt exposed excessive assignment/blend logging rather than a game failure; logging was rate-limited to one ADS IN/OUT marker per blend window, a bounded assignment-discovery budget and compact control summaries. The observer was rebuilt successfully after this diagnostic-only correction.
- The compact runtime log confirmed all four 2.0.4 resolvers and showed `camera +0x234=90` and `camera +0x262=0x1` at ADS markers, but did not identify the active primitive at that boundary. ADS register probes for `RAX/RCX/RSI` were therefore added as a read-only refinement; no causal classification is made from the previous run.
- The refined runtime log confirmed the same camera scalar values at ADS IN/OUT. `RAX` and `RCX` did not form valid primitive/parent pairs; `RSI` was stable with `+0x265=0x2` and its parent `+0x265=0x0`, with no transition across the observed ADS markers. This rejects the direct scalar and direct ADS-register primitive-transition hypotheses for this dataset, but does not yet identify the actual rendered weapon primitive.

### Batch 5 — MarkRenderStateDirty ownership audit

- Search the current 2.0.4 program for the WIDEBOY-derived `MarkRenderStateDirty` signature and the `FirstPersonPrimitiveType` (`+0x265`) setter signature.
- Confirm uniqueness, current addresses/RVAs, direct setter-to-refresh relationship and the setter's `this` ownership.
- Keep the audit static and read-only; do not add a runtime hook or modify the stable ASI in this batch.

Validation: headless Ghidra scan, targeted decompilation of the current setter, its direct caller and the resolved refresh function. A generic refresh function with many callers is not sufficient weapon-specific causal proof.

Batch 5 result — stale-image static anchor record (revalidated below):

- Added `02-Research/Ghidra/ghidra-scripts/AuditMarkRenderStateDirty204.java`.
- Added `02-Research/Ghidra/ghidra-scripts/DecompilePrimitiveRefreshPath204.java`.
- The original addresses in this batch (`0x140072660` / `0x14565FF00`) came
  from the stale image and are not current 2.0.4 setter coordinates.
- Current-image revalidation found the setter at `0x145665FA6` (RVA
  `0x5665FA6`) and retained the refresh target at `0x140072660` (RVA
  `0x72660`).
- The setter has one direct call site at `0x1454AC6AC`, inside `FUN_1454AC658`; that caller computes the new byte from `param_2` state and invokes `FUN_14565FF00(param_1, local_19)`. This establishes the current `this` flow for the setter, but not that the object is the rendered weapon/viewmodel primitive during ADS.
- The resolved refresh function has a large generic caller set (`515` references in the current analysis), so it is not safe to treat the MRSD implementation itself as a weapon-specific hook target without runtime correlation.
- Static result: MRSD/setter linkage is confirmed on the matching image;
  weapon-specific causality remains unproven. The prior setter runtime trace
  already produced no correlated event, so no new runtime tracer is justified.

### Batch 9 — matching-image weapon revalidation — completed

- Passed the canonical 2.0.4 identity gate.
- Reconciled the current primitive setter `RVA 0x5665FA6` as
  `FUN_145665FA6`, with one xref from `FUN_1454B26FE`; the setter compares and
  writes `+0x265`, then reaches the render-state refresh target.
- Revalidated ADS ownership as `FUN_1406ABB9C`, with 13 direct callees.
- Re-ranked current direct callees. The strongest candidate,
  `FUN_1424BE5AE`, is a generic interpolation helper with 128 callers; no
  candidate has sufficient weapon/viewmodel ownership or causal evidence.
- Fresh static `+0x230/+0x234/+0x262` intersection evidence remains
  non-specific and does not identify a projection owner.
- Report: `02-Research/Ghidra/reports/weapon-viewmodel-fov-revalidation-2026-09-02.md`.

Result: primitive/MRSD branch is closed as non-causal for the captured ADS
correction; ADS downstream ownership remains unresolved. No runtime tracer or
production change is justified.

## Decision tree

- `+0x234` changes from the stale value to the stable value at ADS/menu: promote a bounded native writer/refresh trace.
- `+0x234` and `+0x262` remain unchanged while the image corrects: defer to downstream primitive/render-state analysis.
- Camera pointer changes at correction: promote ownership/reacquisition analysis.
- ADS and menu reach different terminal states: keep their mechanisms separate.
- No meaningful approved-field change: close this candidate.
- Primitive pointer or `+0x265` changes at ADS correction: promote ownership/reacquisition analysis.
- Primitive pointer and `+0x265` remain unchanged, but a validated native refresh event coincides with correction: promote a bounded render-state refresh trace.
- The current 2.0.4 setter and MRSD signatures do not match uniquely: stop and do not trace or implement.
- The setter `this` is not shown to be the active weapon/viewmodel object at ADS: do not promote a production fix; use one bounded setter-focused runtime correlation only if explicitly approved.

### Batch 6 — unique primitive-setter runtime correlation

- Add one isolated read-only hook for the unique current 2.0.4 `FirstPersonPrimitiveType` setter.
- Log only real setter invocations, including `this`, old `+0x265`, incoming value, whether the value changes, caller RVA, ADS phase and camera control fields.
- Keep the hook independent of `MarkRenderStateDirty`; do not add render-state writes or production behavior.
- Run one bounded post-cinematic → ADS scenario. Promote ownership analysis only if a relevant object and state change coincide with the visual correction.

Validation: unique signature/decode checks, isolated build/load, then one timestamp-correlated user runtime log. No causal claim from setter activity alone; `this` relevance and visual timing are required.

Batch 6 status — implementation/build PASS, runtime pending:

- Added `src/weapon_viewmodel_primitive_setter_trace_204.cpp`.
- Added `build-artifacts/test-scripts/build-weapon-viewmodel-primitive-setter-trace-204.cmd`.
- The tracer resolves the unique current 2.0.4 primitive setter, ADS IN/OUT markers and camera-writer signature; it refuses safely if any required signature is ambiguous or missing.
- It logs bounded real setter invocations with object, parent, old/incoming `+0x265`, `changed`, caller RVA, ADS phase and camera controls. It performs no primitive, camera or render-state writes.
- Isolated build PASS. Artifact: `build-artifacts/test-asi/STALKER2WeaponViewmodelPrimitiveSetterTrace204.asi`.
- Runtime causality is pending one user-supplied post-cinematic → ADS run.

Batch 7 status — implementation/build PASS, runtime pending:

- Added `src/weapon_viewmodel_ads_differential_trace_204.cpp`.
- Added `build-artifacts/test-scripts/build-weapon-viewmodel-ads-differential-trace-204.cmd`.
- The tracer opens a bounded window at ADS IN and closes it at ADS OUT.
- It records baseline/window-close camera snapshots, camera changes during the window, new mesh-assignment objects and any primitive-setter events, with window IDs and phase markers.
- It uses no polling loop, no timers, no memory writes and no production hooks.
- The isolated build completed successfully and produced `build-artifacts/test-asi/STALKER2WeaponViewmodelAdsDifferentialTrace204.asi`.
- Runtime differential classification is pending one post-cinematic → ADS → ADS OUT run.

Batch 7 diagnostic correction after first runtime attempt:

- The first log showed repeated ADS anchor executions being interpreted as 26 separate windows. This is invalid window gating, not causal evidence.
- Updated `src/weapon_viewmodel_ads_differential_trace_204.cpp` so only the first IN edge opens a window and only the first OUT edge closes it; repeated executions within the same transition are ignored.
- Rebuilt the isolated diagnostic successfully. Runtime classification remains pending a clean single-window capture.

Batch 7 runtime result — known ADS-window paths negative:

- The corrected capture produced one valid window (`id=1`) from ADS IN through ADS OUT.
- No primitive-setter event and no mesh-assignment event occurred inside the window.
- Camera `FirstPersonFOV` stayed `90`, `FirstPersonState` stayed `0x1`, and aspect stayed `3.55556`.
- The only observed transition was native world FOV, approximately `90.6557 → 83.6122`, while the camera object remained unchanged. This is an ADS world-camera transition, not evidence of weapon/viewmodel FOV state correction.
- The known `+0x265` setter/mesh-assignment branch is therefore negative for this captured correction path. The next bounded branch is static analysis of `+0x234` reads and first-person/viewmodel projection consumers.

Batch 6 runtime result — setter branch negative for the captured ADS window:

- The supplied 2.0.4 log shows unique resolver installation for primitive setter, ADS IN/OUT and camera writer.
- No `Primitive setter:` event was emitted during the captured ADS marker sequence, so the tested ADS correction path did not invoke the resolved `+0x265` setter.
- The log contains no explicit cinematic ENTER/EXIT marker, so this closes only the observed setter-during-ADS hypothesis; it does not by itself identify the downstream correction mechanism.
- The diagnostic's repeated ADS marker output was a logging defect, not a runtime hook failure. Marker logging was rate-limited to one event per direction per 500 ms for the next build.
- No production implementation is justified by this branch. The next research path is the downstream first-person/viewmodel projection or refresh consumer, subject to a new bounded plan/approval.

### Batch 7 — bounded ADS differential capture

- Use the validated ADS IN/OUT anchors as the only window gates.
- Capture a compact pre-ADS baseline and compare it with state changes observed between ADS IN and ADS OUT.
- Record changes on the known camera control slice, known mesh/primitive assignment objects and the unique primitive setter when they occur.
- Classify one-shot ADS events, changed values, no-op setter calls, object changes and repeated events without tracing the entire process or renderer.
- Keep the diagnostic read-only and do not add timers, polling loops or production hooks.

Validation: isolated build/load first, then one post-cinematic → ADS → ADS OUT runtime log. Promote only events that are temporally correlated with the visual correction and tied to a relevant object.

### Batch 8 — bounded first-person FOV consumer audit

- Scan the current 2.0.4 Ghidra program for decoded reads of camera offsets `+0x230`, `+0x234` and `+0x262`.
- Rank functions that combine `+0x230` and `+0x234`, then inspect first-person-state intersections and derived projection/FOV writes.
- Reject obvious generic serialization, reflection, constructor/default and editor/debug paths; do not broaden into an all-renderer scan.
- Do not build or run a runtime artifact until a small candidate set is justified statically.

Validation: read-only Ghidra scan and targeted decompilation, with explicit candidate/rejection reasons.

Batch 8 result — no bounded consumer candidate:

- Added `02-Research/Ghidra/ghidra-scripts/AuditCameraFirstPersonFovConsumers204.java`.
- The displacement triage found 516 functions with at least two target offsets and 9 functions with all three offsets.
- All 9 all-three functions were large (approximately 3,970–53,905 bytes) and were not suitable as bounded projection/FOV hook candidates; targeted review classified the available slices as generic state/packing contexts rather than a compact consumer contract.
- No runtime tracer was created or promoted from this scan. Raw offset intersection is insufficient ownership/causal evidence.
- Report: `02-Research/Ghidra/reports/camera-first-person-fov-consumer-audit-204.md`.
- The next branch, if pursued, must narrow from the ADS path/data-flow or a stronger semantic anchor rather than broadening the renderer scan.

### Batch 9 — bounded ADS call-chain/data-flow audit

- Resolve the current 2.0.4 ADS IN and ADS OUT signatures in the existing Ghidra program.
- Inspect the containing functions and only a bounded instruction/basic-block neighborhood around each validated ADS hook site.
- Enumerate direct call targets, object dereferences, state writes and FOV/projection-like math in that local slice.
- Do not reject a candidate solely because its containing function is large; classify local blocks and callsites instead.
- Do not build a runtime tracer until one or more local candidates have a defensible camera/viewmodel or refresh contract.

Validation: read-only Ghidra script execution and a concise report of local ADS call/data-flow candidates and rejection reasons.

Batch 9 initial result — ADS local slice completed against the selected Ghidra image:

- Added `02-Research/Ghidra/ghidra-scripts/AuditAdsLocalCallChain204.java`.
- The selected Ghidra image's ADS IN and ADS OUT signatures each resolved uniquely at
  `0x1406A849C` and `0x1406A8639`, both inside `FUN_1406A81BA`.
- The bounded slice shows shared ADS transition logic: clamped float math,
  writes to `RAX+0x4C/+0x50/+0x54/+0x58/+0x60/+0x64`, state updates at
  `RAX+0x5C`, and several direct update/dispatch calls.
- The local slice showed shared ADS transition logic and direct update/dispatch
  candidates, but no compact ownership-validated first-person projection or
  render-refresh hook.
- The direct-callee ranking/decompilation classified the strongest local
  candidates as registration, generic interpolation or generic helper paths.
- Read-only headless execution completed with no Ghidra project modifications.
- Report: `02-Research/Ghidra/reports/ads-local-callchain-audit-204.md`.
- Stable gameplay/cinematic ASI code was not changed.

Batch 9 identity reconciliation correction:

- The initial ADS local slice, direct-callee ranking and bounded decompilation
  were run against a stale Ghidra image. Although its nominal executable path
  matched the game path, its initialized `.text` block was `130,803,712` bytes,
  while the current 2.0.4 executable has a `.text` raw section of `130,818,560`
  bytes.
- The static ADS RVAs `0x6A849C` and `0x565FF00` therefore do not match the
  current runtime resolver RVAs `0x6ABE7E` and `0x5665FA6`. This is an image
  mismatch, not a function-entry versus hook-instruction offset.
- The static ranking is invalidated for current 2.0.4 hook selection. No
  current-build candidate was promoted and no runtime tracer was created.
- A matching-image Ghidra analysis is required before repeating the ranking.
- Identity probe: `02-Research/Ghidra/ghidra-scripts/PrintProgramIdentity204.java`.
- Ranking: `02-Research/Ghidra/ghidra-scripts/RankAdsDirectCallees204.java`.
- Bounded decompilation: `02-Research/Ghidra/ghidra-scripts/DecompileRankedAdsCallees204.java`.

## Risks and safe failure

- Invalid pointer reads could crash the game; all reads must be validated and fail closed.
- Observer overhead must be bounded and logging rate-limited to state transitions, not every frame.
- The observer must never alter the camera or weapon state.
- Disable the diagnostic immediately if resolver ambiguity, invalid reads or instability appears.

## Stop conditions and phase gates

- Stop before runtime testing if the observer cannot resolve and validate one camera-writer anchor.
- Stop after Batch 2 if the approved fields do not change meaningfully at the correction event.
- Do not implement a setter/MRSD hook from static linkage alone; runtime object identity and ADS correlation are required.
- Do not update the stable ASI from this plan.

## Expected final Git review

Review changed paths, resolver/build evidence and runtime-log limits against this plan. Record the result in `backlog/TASKLOG.md` only if the causal theory is rejected or a follow-up implementation is completed and tested.
