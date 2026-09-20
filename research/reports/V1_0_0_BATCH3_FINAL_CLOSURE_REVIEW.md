# v1.0.0 Batch 3 — Final Safety Closure Review

## Scope

Compare the approved Batch 3 safety candidates from the v1.0.0 task plan with
the completed evidence and final classifications. This review does not reopen
closed batches or introduce new audit work.

## Candidate disposition

| Initial candidate | Final result | Production change |
| --- | --- | --- |
| Hotkey/monitor worker shutdown | Confirmed lifecycle defect; owned stop event, retained handles, interruptible waits and controlled join implemented and harness-validated | Yes, Batch 3.1 |
| Initialization rollback / feature failure isolation | Feature-local degradation and shared-fatal cleanup implemented; independent features continue safely | Yes, Batch 3.2 |
| Native pass-through after cinematic failed write | Intentional fail-closed behavior; native pass-through was not justified | None, Batch 3.3 |
| Atomic INI replacement / direct-write fallback | Confirmed data-loss defect; destructive live-file truncation removed and failure-injection validated | Yes, Batch 3.4 |
| Hook installation rollback | Current local and shared cleanup paths cover reviewed failures; orphaned production hook not found | None, Batch 3.5 |
| Loader-lock cleanup | `DllMain` process termination bypass and signal-only normal detach path; no blocking teardown remains in detach call path | Yes, Batch 3.1 |
| Generic helper safety | No separate reachable defect was established after the ownership split and checked helper boundaries | None; no open candidate |

## Final invariants

- Feature-local failure does not automatically disable independent Gameplay,
  Cinematics or Dialogue features.
- `DISABLED` and `FAILED` remain distinct observable states.
- Shared-fatal cleanup stops workers before hook/state teardown.
- A persistence failure cannot intentionally truncate the live INI before a
  successful replacement.
- Cinematic failed writes remain fail-closed and do not fall through to an
  unvalidated native store.
- `HookSet` remains a passive ownership container; transactionality is not
  implied without evidence of a cleanup gap.
- Normal DLL unload support is not claimed; `DllMain` is not a blocking
  controlled-shutdown owner.

## Validation evidence

- Worker lifecycle harness: PASS.
- FeatureStatus vocabulary harness: PASS. It verifies `DISABLED`, `FAILED`
  and `AVAILABLE` status semantics; feature-local rollback and runtime
  graceful-degradation behavior remain supported by the corresponding source
  review and final in-game regression.
- Config persistence harness: normal save, staging failure preservation and
  replacement failure preservation all PASS.
- Production `build.cmd`: PASS after the Batch 3.4 implementation.
- `git diff --check`: PASS for the reviewed worktree.
- Full in-game regression remains Batch 4 and is not claimed here.

## Closure decision

```text
Batch 3 safety candidates in approved plan    COMPLETE
Unresolved confirmed safety defect             NONE
Open production safety ambiguity               NONE
New synthetic probes justified                 NO
Production changes after this review           NONE
Next phase                                     Batch 4 regression validation
```

Batch 3 is closed. Further safety work requires new contradictory evidence or
a separately approved maintenance task; do not create additional audit work
solely to extend the phase.
