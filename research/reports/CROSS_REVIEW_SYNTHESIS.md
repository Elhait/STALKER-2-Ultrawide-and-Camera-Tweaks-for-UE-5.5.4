# Cross-Review Synthesis — Six Audit Directions

Date: 2026-09-20  
Scope: current production tree after the bounded Architecture, Security/Safety,
Realtime/Threading/Lifecycle, Persistence/Runtime State/Recovery, Performance,
and Tests/Harnesses/Validation repair batches.

## Executive verdict

No confirmed current production defect was found by the combined read-only
review. The repairs are mutually compatible in the current tree: they do not
reintroduce the earlier ownership, publication, persistence, recovery,
fail-closed, logging, or deterministic-runner failures.

The previous `GLOBAL_HORPLUS_RUNTIME_PASS` remains useful historical behavior
and fixture evidence, but it is not proof for the current artifact. The
capability installation, callback gates, cinematic selection publication,
shutdown handling, cinematic fallback, and Dialogue context-invalidation
dataflows changed after that evidence was collected. Those paths require one
new combined runtime regression with the exact current ASI and game identity.

The current tree is therefore:

```text
Production repair state       statically coherent
Deterministic validation      31 / 31 / 31, recorded PASS
Current production defects    none confirmed
Runtime regression            required for changed integration paths
OS fault injection            incomplete, accepted as a bounded evidence gap
Performance                   measurement required; no unmeasured defect claim
```

**Pre-runtime verdict: `READY_FOR_COMBINED_RUNTIME_VALIDATION`.**

This synthesis did not build, execute harnesses, launch the game, optimize
code, change hooks, or modify production/test sources. Recorded validation
results below come from the repair reports and were checked against current
registration/source structure, not rerun by this review.

## Canonical finding ledger

The ledger deliberately uses one row per independent root cause. Historical
manifestations from more than one audit are listed in the same row where they
share that cause.

| ID | Original finding | Domain; severity at discovery | Subsequent repair(s) | Current-tree status | Remaining evidence requirement |
| --- | --- | --- | --- | --- | --- |
| C01 | Non-Native Dialogue implicitly depended on cinematic lifecycle and Gameplay recovery observation while feature status represented only the Dialogue boundary. | Architecture; high | `ARCHITECTURE_A1_DIALOGUE_CAPABILITY_REPAIR.md`: explicit capability rule, observation-only cinematic lifecycle installation, hotkey rejection when capability is unavailable. | `RUNTIME_VALIDATION_REQUIRED` | Validate the full configuration and observation-only startup paths, plus fail-closed behavior when Gameplay recovery observation is unavailable. |
| C02 | Cross-domain presentation state and Hor+ projection were named as Gameplay/Cinematic ownership, obscuring their real boundary. | Architecture; medium | `camera::CoordinatorState` and `camera::HorPlus` introduced; production and harness consumers migrated. | `FIXED_DETERMINISTICALLY` | No additional runtime proof beyond the combined behavior regression. |
| C03 | Runtime owner wording overstated ownership and could mislead a new maintainer about `RuntimeState` and `g_*` aliases. | Architecture; medium | `docs/ARCHITECTURE.md` and source comments now describe the translation-unit-private primary integration/lifetime owner and field aliases. | `FIXED_STATICALLY` | None. |
| C04 | Further coordinator/runtime decomposition, transactional `HookSet`, DI, typed signatures, or a build-system migration were proposed without a concrete current defect. | Architecture; low/maintainability | Independent quality review retained the current integration boundary and feature-local cleanup contracts. | `NOT_APPLICABLE_AFTER_CURRENT_REPAIRS` | Reopen only on a concrete ownership, update, or testability failure. |
| C05 | Native callback affinity was treated more strongly than source evidence supports. | Architecture + Realtime; medium | Audit recorded `THREAD_AFFINITY_NOT_ESTABLISHED`; atomics/gates cover declared cross-thread observations and `WorkerLifecycle` remains externally serialized. | `ACCEPTED_RISK` | Runtime may increase confidence in observed callback threads, but one session cannot establish a universal UE guarantee. Documentation wording still needs reconciliation after validation; see Evidence/documentation gaps. |
| C06 | Dialogue `Candidate/Active` could be mistaken for game-owned Dialogue truth. | Architecture + Recovery; high if treated as truth | Source/target/trajectory hardening, explicit classifier provenance, capability gating, and recovery/exclusion guards. | `ACCEPTED_RISK` | Combined regression must check no phantom ownership during ADS, binocular, cinematic recovery, or camera recreation. The heuristic itself is intentional. |
| C07 | Native callbacks and worker entry points allowed C++ exceptions to cross ABI/thread boundaries; logging could throw inside callbacks. | Security/Safety; high | `SafeMidHookEntry`, `WorkerLifecycle::ThreadStartThunk`, and non-throwing auxiliary logging; cinematic RIP continuation is completed before auxiliary work. | `RUNTIME_VALIDATION_REQUIRED` | Exercise ordinary callbacks and shutdown on the current artifact. Access violations/SEH remain outside the C++ catch-all contract. |
| C08 | Vendored SafetyHook transaction failure could leave stale trap state or ignore protection failures. Realtime R4 described the same root cause. | Security/Safety + Realtime; high | Trap removal and `VirtualQuery`/`VirtualProtect` failure propagation were repaired; hook commits use disabled creation and explicit enable/rollback. | `VALIDATION_GAP` | Rare OS protection/rollback failures are not deterministically injectable. Exact upstream SafetyHook commit/release provenance is also not recorded. |
| C09 | Resolver decode, look-ahead, byte windows, and rel32 targets were not uniformly bounded by the executable section. | Security/Safety; high | `ExecutableSpan` propagated through validators/resolvers; truncated decode, rel32, byte-window, camera-writer, and cinematic-aspect fixtures added. | `FIXED_DETERMINISTICALLY` | Actual executable uniqueness and installation remain part of startup/runtime evidence. Fixed CameraWriter bytes make a separate public invalid-operand fixture unreachable; signature-mismatch refusal covers the reachable path. |
| C10 | `ReadMemory()` could copy from committed but non-readable protection such as `PAGE_EXECUTE`. | Architecture quality + Security; high | Explicit readable-protection predicate plus execute-only/noaccess/guard/boundary tests. | `FIXED_DETERMINISTICALLY` | None beyond normal platform/runtime use. |
| C11 | Cinematic hooks could become observable before the complete component set was committed. Realtime R2 was the publication manifestation of the same transaction issue. | Security/Safety + Realtime; high | Hooks are created `StartDisabled`; complete and observation-only commits enable the intended set; callback gates publish only after enable; rollback closes gates and resets hooks. | `RUNTIME_VALIDATION_REQUIRED` | Verify startup status, observation-only mode, and native/pass-through behavior on the current executable. |
| C12 | Config staging/creation could report success before durable flush/close or damage the last-known-good INI on failure. | Security/Safety + Persistence; high | Explicit write/flush/close before write-through replacement; first creation closes before success; staging/replacement failure harnesses. | `FIXED_DETERMINISTICALLY` | Power-loss and filesystem-driver behavior remain normal OS limitations, not a current code defect. |
| C13 | Auto viewport acquisition could accept transient degenerate geometry. | Security/Safety; medium | Generic usable-dimension predicate and native/display fallback; arbitrary aspect support retained. | `RUNTIME_VALIDATION_REQUIRED` | Live resize/minimize/aspect changes and subsequent Auto cinematic behavior. |
| C14 | `ResumeThread` failure dropped ownership of a suspended worker. Realtime R5 was the same worker-start failure class. | Security/Safety + Realtime; high | Handle retained through terminate/wait/close; failed worker is not published; thread thunk contains exceptions. | `VALIDATION_GAP` | `ResumeThread`, `TerminateThread`, and wait failure injection is unavailable without a platform abstraction. |
| C15 | Cinematic aspect/FOV selections could mix generations when F9/F12 changed around ENTER; Persistence P1 later found duplicate ENTER publication ordering in the same lifecycle. | Realtime + Persistence; high | Immutable `CinematicSelectionSnapshot`, generation, accepted-before-publish ordering, duplicate-ENTER refusal, active snapshot authority, EXIT/rollback invalidation. | `RUNTIME_VALIDATION_REQUIRED` | F9/F12 immediately before/after ENTER, during active cinematic, EXIT, and next ENTER on the current artifact. |
| C16 | Controlled shutdown could reset hooks/resources even when workers were not joined. | Realtime; high | Stopping and callback gates close first; failed join defers teardown and retains ownership; loader-lock detach remains signal-only. | `VALIDATION_GAP` | Normal process exit can validate the supported detach path. Failed waits/self-join and a complete externally initiated controlled shutdown are OS/integration gaps. |
| C17 | Diagnostic TLS reset wrote the worker thread's TLS rather than the callback thread's snapshot. | Realtime; medium, diagnostic-only | Shared reset generation consumed by the next callback-thread publication. | `FIXED_STATICALLY` | Optional diagnostic-only check when that special profile is next used; not a production runtime gate. |
| C18 | Diagnostic `CameraStateSnapshot` could combine independently loaded observations; no production consumer or concrete bad decision was demonstrated. | Realtime; low/diagnostic | No speculative synchronization change; semantic publication remains change-driven. | `ACCEPTED_RISK` | Reopen only if diagnostic evidence shows a false semantic transition. |
| C19 | Closed/stopping cinematic aspect callback wrote flags although the replaced native instruction owns only the aspect field. | Persistence/Runtime State; high | Closed/stopping/invalid-selection path now performs aspect-only native fallback and does not require/write `+0x259`. | `RUNTIME_VALIDATION_REQUIRED` | Observe closed/stopping fallback where practical; confirm no flags mutation or visual/lifecycle regression. |
| C20 | Dialogue context invalidation was applied in AspectRecalculation but not symmetrically before HorPlus mode behavior. | Persistence/Recovery; high | Shared `EvaluateGameplayContextChange` runs on native/current writer input in both Gameplay paths before mode-specific transformation. | `RUNTIME_VALIDATION_REQUIRED` | Source recreation and material FOV change in both modes; no stale Candidate/Active/Recovery state. |
| C21 | Loader used last-valid duplicate config values, while persistence updated only one occurrence and could report a value that reload would not select. | Persistence; high | Persistence updates every matching managed occurrence; loader semantics remain last-valid; duplicate round-trip harness added. | `FIXED_DETERMINISTICALLY` | One ordinary hotkey persistence/restart check may be included in runtime regression, but duplicate semantics do not need manual repetition. |
| C22 | CinematicExiting, post-cinematic exclusion, and unreadable/invalid recovery targets had unresolved terminal/liveness evidence. | Persistence/Recovery; medium | Existing recovery state machines and fail-closed behavior retained; no timeout or speculative invalidation added. | `RUNTIME_VALIDATION_REQUIRED` | Cinematic EXIT through native convergence, subsequent Dialogue, and save/load/camera recreation. |
| C23 | Informational callback logging forced flushes and diagnostic memory probes/formatting could execute on hot paths. | Performance; high for spikes, medium for sustained cost | Error-only automatic flush; explicit lifecycle flushes; early diagnostic gates before probes, locks, comparisons, and formatting. | `PERFORMANCE_MEASUREMENT_REQUIRED` | Compare the clean production artifact against vanilla and both Gameplay modes. Static repair removes the confirmed hazards but does not quantify total overhead. |
| C24 | High-rate research instrumentation was compiled into the same artifact used for production/performance comparison. | Performance; high measurement contamination | Separate production and diagnostic build profiles and output names; supported high-rate macros absent from production. | `FIXED_STATICALLY` | Benchmark only `STALKER2CameraTweaks.asi` with diagnostics disabled. |
| C25 | GameplayBaseline/store mutexes, SafetyHook context preservation, Hor+ math, `/O1`, and repeated viewport queries were plausible costs without measured attribution. | Performance; low/medium hypothesis | No speculative optimization; production hot paths retained after removal of confirmed logging/probe work. | `PERFORMANCE_MEASUREMENT_REQUIRED` | Investigate only if repeatable benchmark deltas exceed control variance and correlate with a path. GameplayHorPlus and NativeHorPlus cinematic steady-state paths still converge after ENTER-time selection. |
| C26 | Unified runner compiled but did not execute one harness and could mask one compile failure. | Tests/Validation; high evidence-integrity | Immediate checks after every compile/run; cinematic selection execution restored; pre-build registration audit added. | `FIXED_DETERMINISTICALLY` | Current source inventory was independently matched to 31 compile and 31 execute entries. |
| C27 | Cinematic aspect and Dialogue FOV production helpers had no deterministic coverage. | Tests/Validation; high | New resolver/store/policy and recorded endpoint/trajectory/invalid-input harnesses. | `FIXED_DETERMINISTICALLY` | Native callback integration remains runtime-only. |
| C28 | Instruction/rel32, camera write/resolver, and `IsWritable` tests covered only a subset of declared safety contracts. | Tests/Validation; high | Expanded validator, atomic write/refusal, PE resolver, and Windows protection/range fixtures. | `FIXED_DETERMINISTICALLY` | The unreachable standalone CameraWriter operand-mismatch case remains a documented bounded limitation, not a missing production seam. |
| C29 | Runtime-evidence fixtures lacked machine-readable provenance. | Tests/Validation; medium | Scenario/source metadata added; unknown hashes/version are represented as `UNKNOWN`. | `VALIDATION_GAP` | Current combined regression must record exact ASI/game hashes. Historical fixture hashes can remain unknown rather than fabricated. |

## Cross-audit deduplication

The following historical findings are one canonical root cause rather than
separate unresolved problems:

```text
Hook transaction safety
  Security S2 protection/trap failure
  Realtime R4 transaction coherence
  Realtime R2 pre-publication callback exposure
    -> C08 for rare OS transaction failure
    -> C11 for production installation/publication ordering

Worker lifecycle failure
  Security S7 ResumeThread failure
  Realtime R5 startup cleanup
  Realtime R3 teardown after failed join
    -> C14 for startup ownership
    -> C16 for shutdown ownership

Cinematic selection coherence
  Realtime R1 mixed F9/F12 generations
  Persistence P1 duplicate ENTER/active authority
    -> C15

Dialogue stale context
  Architecture A1 missing lifecycle dependencies
  Persistence P3 asymmetric Gameplay-mode invalidation
  Persistence recovery-liveness questions
    -> C01 for capability availability
    -> C20 for gameplay context invalidation
    -> C22 for terminal runtime evidence

Resolver and memory safety
  Security S3 bounded decode/look-ahead
  Architecture-quality ReadMemory protection
  Tests partial validator/resolver/writability coverage
    -> C09, C10, and C28; independent failure modes, shared validation layer

Performance instrumentation
  Performance logging/flush hazard
  diagnostic probe work
  production/diagnostic artifact contamination
    -> C23 for reachable runtime work
    -> C24 for build-profile isolation
```

## Repair interaction analysis

### Ownership and publication

- `RuntimeState` remains the integration/lifetime owner; `g_*` references are
  field aliases, not new owners.
- Gameplay and cinematic hooks are created disabled. Their gates open only
  after physical enable/commit and close before teardown.
- The active cinematic policy/mode is stored before the release publication of
  `cinematicSelectionValid`; aspect consumers require the valid snapshot.
- Hotkeys change selected next-lifecycle values. Active cinematic consumers
  read the captured selection, and duplicate ENTER cannot replace it on the
  serialized callback contract.

No contradictory second owner or bypass of the lifecycle snapshot was found.
Concurrent duplicate ENTER callbacks remain part of C05's unestablished
native-affinity assumption; there is no current evidence that this concurrency
occurs, so it is not promoted to a production defect.

### Fail-closed and shutdown interaction

- Closing `stopping`, Gameplay, and cinematic gates precedes worker join and
  hook destruction.
- A failed required join leaves hook/resource ownership intact rather than
  destroying resources under a live worker.
- The cinematic replaced-store callback advances its validated instruction
  boundary first. A closed gate applies only the original native aspect field;
  an unwritable target skips the store and continues, matching the documented
  fail-closed contract.
- SafetyHook enable/disable failure propagates to feature transaction rollback.

No repair weakens the earlier fail-closed behavior. Rare Windows protection,
terminate, and wait failures remain evidence gaps, not demonstrated normal-path
defects.

### State, recovery, and performance interaction

- Dialogue context invalidation now occurs before mode-specific Gameplay
  handling and uses native writer input, so Hor+ transformed output cannot
  manufacture a context jump.
- GameplayBaseline/restoration and cinematic selection retain separate
  authority. The selection repair does not alter baseline provenance or EXIT
  recovery math.
- Logging containment returns before diagnostic reads, Dialogue mutex access,
  and formatting when diagnostics are disabled. It does not bypass state
  publication, context invalidation, or recovery decisions.
- Production/diagnostic build separation changes instrumentation presence, not
  hook locations or camera policy.

No interaction was found that invalidates deterministic tests or changes the
established recovery semantics. Old runtime evidence is non-representative of
the repaired integration paths only because those paths changed afterward,
not because the historical evidence was invalid.

## Current production defects

**Count: 0.**

No current-tree path was found that demonstrates incorrect production camera
behavior, unsafe ownership release, stale-state publication, config loss, or a
known performance regression under the documented operating contract.

## Remaining validation gaps

### Runtime validation requirements

1. Current-artifact resolver installation, capability status, and exact
   ASI/game identity.
2. Cinematic snapshot behavior under F9/F12 timing and duplicate native ENTER.
3. Gameplay context invalidation in both modes after material FOV/source change.
4. Cinematic EXIT, native recovery convergence, post-cinematic Dialogue
   exclusion, and subsequent Dialogue lifecycle.
5. Observation-only cinematic lifecycle for non-Native Dialogue and fail-closed
   behavior when Gameplay observation is unavailable.
6. Live Auto resize/minimize/aspect behavior, save/load, and camera recreation.

### OS/platform validation gaps

- SafetyHook `VirtualQuery`/`VirtualProtect` and protection-restore failure
  injection, including exact upstream comparison.
- `ResumeThread`/`TerminateThread`/wait failure injection and a failed-join
  controlled-shutdown integration path.
- C++ exception containment does not claim recovery from arbitrary access
  violations or corrupted native context.

These do not require a pre-runtime abstraction/DI batch.

### Evidence/documentation gaps

- `docs/SAFETY_INVARIANTS.md` says callbacks are *expected* on a runtime owner
  thread, while `docs/SUPPORTED_BUILD_MANIFEST.md` says ordinary telemetry is
  thread-confined and the architecture audit records affinity as not
  established. The durable wording should distinguish an operating assumption
  from a proven engine guarantee after the runtime regression.
- `docs/SUPPORTED_BUILD_MANIFEST.md` records the earlier Steam 2.0.5 identity
  `E7B...`, while `GLOBAL_HORPLUS_RUNTIME_PASS.md` records `61BC...` for its
  later session. The combined regression must identify the actual current
  executable before the manifest is finalized.
- Runtime fixture source references are now explicit, but some original game,
  mod, and build hashes remain honestly `UNKNOWN`.
- `tests/runner/test_cmd_audit.ps1` proves that runner registrations have
  matching compile/run/check entries. It derives its “sources” set from
  `test.cmd`, not a filesystem enumeration. This review independently counted
  31 current `*_harness.cpp` files and matched them to 31 compile and 31 run
  entries; future filesystem drift is not automatically discovered by the
  helper.

None of these gaps changes production behavior or justifies delaying the
combined runtime run.

## Accepted risks / intentional limitations

- Process-resident ASI lifetime; hot/manual unload is unsupported.
- Externally serialized `WorkerLifecycle`; it is not a general concurrent
  thread manager.
- Native callback thread affinity is an operating assumption, not a proven UE
  guarantee.
- Dialogue classification remains a bounded observation heuristic with
  fail-closed guards, not native Dialogue ground truth.
- SHA-256 identifies evidence/support scope; it is not an allowlist gate.
- An unwritable replaced cinematic store is skipped after the validated
  instruction boundary rather than attempting an unsafe native/custom write.
- Exact vendored SafetyHook upstream revision is not recorded.
- Weapon/Viewmodel FOV is not a defect of this ASI: the mechanism and working
  external repair are known, but a safe patch-resilient standalone ASI access
  path is not established and the feature is intentionally absent.

## Deterministic validation status

Current registration/source inspection establishes:

```yaml
harness_sources: 31
harnesses_compiled_by_runner: 31
harnesses_executed_by_runner: 31
immediate_compile_checks: 31
immediate_run_checks: 31
registration_sets_equal: true
```

The latest repair report records runner self-audit, `test.cmd`, production
build, diagnostic build, and diff check as `PASS`. This synthesis did not rerun
them.

Deterministically represented repair contracts include:

- Dialogue capability truth table;
- camera/shared-state naming and transition helpers;
- cinematic transaction failure/rollback helper;
- selection capture, immutability, and duplicate-ENTER refusal;
- camera context invalidation shared by both production paths;
- config flush/replacement and duplicate-occurrence convergence;
- readable/writable Windows protection contracts;
- resolver cardinality/span/rel32/truncation refusal;
- cinematic aspect application/resolution;
- Dialogue FOV endpoints, trajectories, and invalid inputs;
- camera write all-or-nothing behavior;
- camera baseline/restoration and runtime-evidence replay.

Deterministic tests intentionally do not claim UE callback ordering, visual
framing, real hook installation, save/load recreation, OS fault injection,
or performance.

## Combined runtime validation matrix

Use the exact current production ASI for behavioral acceptance. A diagnostic
artifact may be used in a separate evidence pass where the required telemetry
does not exist in production, but its result must not replace production visual
and interaction confirmation. Each session must record game SHA, ASI SHA,
resolution/aspect, INI, artifact profile, save/location, and evidence path.

| ID | Purpose | Configuration | Exact user/runtime action | Expected observable result | Required log/telemetry evidence | Current contract validated | Source audit/finding |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RV-01 | Current identity, resolver set, and full capability publication | Production ASI; Gameplay enabled; AspectRecalculation initially; Cinematics Auto + NativeHorPlus; Dialogue Reduced; hotkeys enabled; diagnostics disabled for primary pass | Clean launch to gameplay, remain idle briefly, then exit normally after the other scenarios | No crash; requested production hooks install; Gameplay/Cinematics/Dialogue/Hotkeys report available; ordinary gameplay framing is correct; process exit is clean | Startup identity; resolver cardinality/validation lines; initialization summary; artifact/config record; absence of fatal/reset errors | C01, C07, C09, C11, C16; current support identity | Architecture A1; Security S1/S3/S4; Realtime R2/R3 |
| RV-02 | Both Gameplay modes, restoration, native context invalidation, and live aspect continuity | Continue RV-01; representative ultrawide/custom aspect; F11 enabled | Idle gameplay → ADS → binocular equip/internal zoom/unequip → F11 AspectRecalculation→HorPlus → repeat ADS → change live aspect/resolution → repeat → F11 HorPlus→AspectRecalculation → back to HorPlus; include one material player-FOV change | No FOV feedback/double transform; ADS/binocular framing remains stable; restoration returns to current aspect; current native source/FOV change clears stale Dialogue hypothesis equally in both modes; no old transformed baseline | Mode-transition/restoration records, current aspect/flags/write result, native/transformed FOV observations where diagnostic evidence is needed, no unexpected Dialogue Active/owner transition | C13, C20, C23/C25 characterization; restoration and baseline invariants | Persistence P3; Performance audit; Global HorPlus matrix |
| RV-03 | Immutable cinematic selection and next-lifecycle hotkeys | Production ASI with hotkeys; repeatable cinematic; start Auto + NativeHorPlus | Press F9/F12 immediately before first ENTER; after ENTER change F9/F12 again; during active cinematic change once more; complete EXIT; start a second cinematic without restart | First cinematic uses one coherent captured aspect/FOV mode; active framing does not change retroactively; latest selection applies to the second cinematic; duplicate ENTER samples do not overwrite the accepted generation | ENTER/EXIT records, selected and active policy/mode, selection generation if available, baseline source, effective aspect and transformed FOV, screenshots/video for both cinematics | C15 and C19 normal committed path; no mixed generation | Realtime R1; Persistence P1/P2 |
| RV-04 | Cinematic EXIT, recovery liveness, Dialogue exclusion, and recreation | Continue RV-03; Dialogue Reduced; run once in each Gameplay mode if RV-02 did not already cover both | Complete cinematic EXIT → allow native recovery to settle → immediately trigger a real Dialogue → complete it → save/load or death/load/camera recreation → trigger another Dialogue and one ADS transition | Native EXIT remains game-owned; coordinator reaches Gameplay; post-cinematic exclusion releases only after valid recovery; no phantom Dialogue during recovery; both Dialogues recover; recreation establishes a new valid native context without stale baseline | EXIT target/coordinator/handoff records, recovery convergence/exclusion release, Dialogue Candidate→Active→Exiting→Rearm/Inactive sequence, source/context-change evidence, save/load identity marker | C06, C20, C22; selection invalidation at EXIT | Architecture A6; Persistence recovery findings; Global HorPlus matrix |
| RV-05 | Observation-only lifecycle and capability fail-closed matrix | Two short clean launches: A) Gameplay enabled, Cinematics Native + NativeHorPlus, Dialogue Reduced, hotkeys off. B) Gameplay disabled, same cinematic/dialogue config. | A: launch, run one cinematic and one Dialogue. B: launch and attempt one Dialogue; do not modify code or hooks | A: cinematic presentation remains native while lifecycle observation supports non-Native Dialogue/recovery. B: non-Native Dialogue reports unavailable and native behavior is retained; unrelated feature status remains truthful | Initialization status/capability lines; observation-only installation/commit line for A; explicit capability rejection for B; visual native cinematic and native fail-closed Dialogue confirmation | C01 and C11, feature-local degradation | Architecture A1; Security S4; Realtime R2 |
| RV-06 | Config persistence and next-lifecycle semantics on the real file | Production ASI; hotkeys enabled; preserve a copy of the starting INI | Change one Gameplay mode and one cinematic aspect/FOV/Dialogue selection via hotkeys; finish any active lifecycle; exit cleanly; inspect INI; relaunch once | Current active lifecycle remains unchanged where promised; next lifecycle uses new selection; persisted values reload as effective values; no truncation or `.tmp` residue | Hotkey persistence lines, before/after INI, startup configuration after relaunch, exact file path | C12, C15, C21 | Security S5; Persistence P1/P4 |

Runtime completion requires all applicable rows to be `PASS`. A blocked save,
cinematic, or external game condition is recorded as `BLOCKED`, not inferred as
success. Do not use the historical ASI hash as evidence for these repaired
paths.

## Performance benchmark gate

### Artifacts and controls

Use exactly three conditions:

1. `VANILLA`: ASI absent.
2. `ASPECT`: current production `STALKER2CameraTweaks.asi`, diagnostics off,
   `Gameplay.Mode=AspectRecalculation`.
3. `HORPLUS`: the same ASI and INI except `Gameplay.Mode=HorPlus`.

Do not use `STALKER2CameraTweaksDiagnostic.asi`. Record the production ASI and
game hashes. Disable unrelated overlays/mod changes where practical; hold game,
driver, graphics settings, resolution, save, route, weather/time, and capture
tool constant. Pre-warm shader/cache-sensitive content before measured runs.

### Representative scenarios

- Stable gameplay camera at a reproducible location.
- Reproducible traversal/combat or camera-motion route.
- ADS and binocular sequence with the same timings.
- A repeatable cinematic may be measured separately for spikes, but must not be
  mixed into steady-gameplay aggregates.

### Metrics

- frame-time distribution: median, p95, p99, p99.9/max with raw trace retained;
- average FPS and 1%/0.1% lows as derived summaries;
- stutter/outlier count using a threshold derived from the vanilla run's own
  distribution and capture-tool convention, not an invented project limit;
- CPU/GPU busy/utilization or equivalent evidence sufficient to distinguish a
  CPU-side hook cost from a GPU/graphics-mod limit;
- run duration, frame count, route/scenario, and log file size/line count;
- callback/application counts only in a separate diagnostic characterization
  run if necessary. Do not compare diagnostic frametimes to production.

### Repeat strategy

- One unmeasured warm-up per condition/scenario.
- At least five measured repetitions per condition for short deterministic
  routes; use more when native variance is high.
- Rotate or randomize condition order (`VANILLA → ASPECT → HORPLUS`, then a
  different order) to reduce thermal/cache/order bias.
- Compare medians and dispersion across repetitions and retain raw frame traces.

### Optimization investigation trigger

Open a performance optimization task only when a condition shows a repeatable
delta outside the control run's observed run-to-run variance, reproduced in
more than one run order or scenario. Attribution must then distinguish:

- common ASI overhead (`ASPECT` and `HORPLUS` both differ from vanilla);
- AspectRecalculation-specific work;
- HorPlus-specific work;
- isolated transition spikes versus sustained steady-state cost;
- mod CPU cost versus GPU/native game variance.

No fixed FPS, millisecond, or percentage threshold is established before this
evidence exists.

## Pre-runtime work verdict

`READY_FOR_COMBINED_RUNTIME_VALIDATION`

There is no current production defect or missing deterministic contract test
that justifies another pre-runtime repair batch. Remaining OS failures are not
injectable without disproportionate abstraction, callback affinity and
Dialogue classification are accepted bounded assumptions, and performance is
a measurement gate rather than a defect.

The evidence/documentation inconsistencies should be reconciled using the new
runtime identity and results after the combined session; they do not change the
artifact that must now be tested.

## Final manifest

```yaml
current_production_defects:
  count: 0
  items: []

runtime_validation_required:
  count: 6
  items:
    - current artifact identity, resolver installation, capability publication, and clean supported detach
    - dual Gameplay-mode behavior, restoration, context invalidation, ADS/binocular, and live aspect continuity
    - immutable cinematic selection across F9/F12 timing, duplicate ENTER, EXIT, and next ENTER
    - cinematic EXIT recovery, post-cinematic exclusion, Dialogue coexistence, and camera recreation
    - observation-only cinematic lifecycle and non-Native Dialogue fail-closed capability matrix
    - real-file hotkey persistence, next-lifecycle semantics, and reload

os_platform_validation_gaps:
  count: 3
  items:
    - SafetyHook VirtualQuery/VirtualProtect/protection-restore failure injection
    - Worker ResumeThread/TerminateThread/wait and failed-join injection
    - arbitrary native access violations are outside C++ exception containment

performance_measurement_required:
  count: 1
  items:
    - clean production VANILLA vs AspectRecalculation vs HorPlus benchmark with repeated raw frametime evidence

accepted_risks:
  count: 8
  items:
    - process-resident lifetime; hot/manual unload unsupported
    - externally serialized WorkerLifecycle rather than general concurrent thread safety
    - native callback affinity is an operating assumption, not an established engine guarantee
    - bounded Dialogue observation heuristic rather than native Dialogue ground truth
    - SHA identity evidence rather than an allowlist gate
    - unwritable replaced cinematic store is skipped after validated control-flow continuation
    - exact vendored SafetyHook upstream revision is not recorded
    - Weapon/Viewmodel FOV intentionally excluded without a safe standalone ASI access path

deterministic_runner:
  sources: 31
  compiled: 31
  executed: 31
  status: PASS

pre_runtime_verdict: READY_FOR_COMBINED_RUNTIME_VALIDATION
```
