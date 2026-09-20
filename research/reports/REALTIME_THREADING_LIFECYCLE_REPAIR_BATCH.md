# Realtime / Threading / Lifecycle Repair Batch

## Scope and current-tree revalidation

This report records the bounded repair after rechecking the supplied R1–R7
findings against the current tree after A2–A6. No game launch was performed.
Performance hypotheses were not changed.

## R1 — cinematic lifecycle selection snapshot

Implemented a small `cinematics::CinematicSelectionSnapshot` and capture
helper. At cinematic ENTER, the current aspect policy and FOV mode are captured
once with a generation. Active cinematic callbacks use the captured aspect
policy; ENTER uses the captured FOV mode. F9/F12 continue changing the runtime
selection for the next lifecycle and do not mutate the active snapshot. The
snapshot is invalidated at EXIT and on rollback.

`tests/cinematics/cinematic_selection_harness.cpp` covers coherent capture,
latest next-lifecycle selection and active-snapshot immutability.

Status: `FIXED` at source/deterministic-contract level. Native callback ordering
and visual next-cinematic behavior remain runtime checks.

## R2 — hook activation/publication/stopping

Gameplay hook creation now starts disabled. Its callback gate opens only after
physical enable succeeds and gameplay availability is published. Cinematic
hooks already used StartDisabled; their gate now opens only after the complete
cinematic commit, including observation-only commit. Closed/stopping gates
return without production intervention. The replaced cinematic aspect store
uses the native aspect/flags fallback while its gate is closed.

Status: `FIXED` for the confirmed Gameplay/Cinematic domains. Native callback
thread affinity and OS-level interleavings remain runtime evidence gaps.

## R3 — controlled shutdown

`ResetAllRuntimeResources()` now closes stopping/callback gates first and checks
the `StopWorkers(true)` result before resetting hooks/resources. If a required
join does not complete, teardown is deferred and ownership is retained. The
process-termination/loader-lock path remains the minimal stop notification.

Status: `PARTIAL`: deterministic self-join/failed-wait ownership is represented
by the existing WorkerLifecycle result and the reset gate, but OS-level failure
injection and a full controlled-shutdown integration test are not established.

## R4 — SafetyHook transaction coherence

The current vendored transaction already propagates protection/trap failure and
cleans trap records on failure. No additional local edit was made in this
batch because the remaining post-write OS failure path cannot be deterministically
proved by the repository harnesses.

Status: `PARTIAL`; OS-level uninjected protection/rollback evidence remains a
known gap. No wholesale dependency update was attempted.

## R5 — worker startup failure

The existing S7 repair retains the suspended handle through termination and
wait before closing it on the normal `ResumeThread` failure path. The current
source does not provide an injectable Win32 seam for proving failed
`TerminateThread`/wait outcomes without a platform abstraction rewrite.

Status: `PARTIAL`; no speculative abstraction or unbounded new teardown path
was added.

## R6 — diagnostic TLS reset

The Dialogue discovery worker no longer resets callback-thread TLS by assigning
to its own `thread_local` copy. It increments a shared reset generation; the
next discovery callback observes the generation and resets its owner-thread
snapshot before publication. This is diagnostic-only and absent from the
production profile.

Status: `FIXED`.

## R7 — diagnostic snapshot coherence

The current snapshot builder remains a change-driven diagnostic observer with
atomic inputs and semantic publication sequencing. Source review did not prove
that a mixed-generation snapshot produces a production decision or observed
defect. No production synchronization was added.

Status: `NO_CHANGE`; stronger coherence claims require runtime publication
evidence.

## Cross-regression and validation

- Existing camera, Gameplay mode, HorPlus, cinematic, Dialogue, recovery,
  safety and runtime-evidence harnesses remain passing.
- New cinematic selection harness: `PASS`.
- `test.cmd`: `PASS`.
- `build.cmd`: `PASS`.
- `build-diagnostic.cmd`: `PASS`.
- `git diff --check`: `PASS` apart from normal CRLF conversion warnings.
- Runtime: `NOT_PERFORMED`.

The builds retain the known external Zydis C4201 warnings.

## Manifest

```yaml
cinematic_selection_snapshot:
  status: FIXED
hook_activation_publication:
  status: FIXED
controlled_shutdown:
  status: PARTIAL
safetyhook_transaction_coherence:
  status: PARTIAL
worker_start_failure:
  status: PARTIAL
diagnostic_tls_reset:
  status: FIXED
diagnostic_snapshot_coherence:
  status: NO_CHANGE
performance_hot_path:
  status: DEFERRED_TO_PERFORMANCE_AUDIT
logging_frequency:
  status: DEFERRED_TO_PERFORMANCE_AUDIT
test_cmd: PASS
production_build: PASS
diagnostic_build: PASS
diff_check: PASS
runtime: NOT_PERFORMED
```

## Runtime checks accumulated

The next combined session should exercise F9 immediately before/after ENTER,
F9/F12 during CinematicActive, EXIT followed by a new ENTER, observation-only
cinematic lifecycle, shutdown/resource ordering where observable, and one
diagnostic discovery reset followed by a callback. These are recorded in
`GLOBAL_HORPLUS_RUNTIME_MATRIX.md` and are not claimed as offline proofs.
