# Security / Safety Repair Batch

## Scope

This report covers bounded S1–S7 repairs from the independent Security/Safety
audit. No game launch, commit, release, new UE hook, or camera behavior
redesign was performed.

## S1 — Native callback / worker exception containment

```yaml
status: PARTIAL
production_change: >-
  Production mid-hook registrations now use a noexcept catch-all adapter;
  logging is effectively non-throwing; cinematic aspect-store RIP completion
  is performed before auxiliary logging/state work; WorkerLifecycle now owns a
  noexcept thread thunk.
deterministic_evidence: test_cmd PASS; existing lifecycle harness PASS
runtime_validation_required: YES
```

The adapter contains C++ exceptions, not arbitrary Windows access violations.
Per-callback semantic fault injection is not currently available, so runtime
validation remains required for native pass-through contracts.

## S2 — SafetyHook transaction repair

```yaml
status: PARTIAL
upstream_provenance: >-
  Vendored amalgamated source is identified as cursey/safetyhook under Boost
  Software License 1.0, but no exact upstream release/commit is recorded.
backport_scope: >-
  Bounded local repair: trap records are removed after each transaction;
  VirtualQuery/VirtualProtect failures now fail the transaction and propagate
  to hook enable/disable. No unrelated dependency update was made.
deterministic_evidence: test_cmd PASS; no OS-level protection failure injection
```

Exact upstream comparison and deterministic OS failure injection remain gaps;
this finding is not claimed fully closed.

## S3 — Strictly span-bounded resolver validation

```yaml
status: PARTIAL
bounded_read_contract: >-
  ExecutableSpan is discovered from the PE section and is required by decoder,
  byte-window, rel32-call and rel32-target validation. Cinematic and gameplay
  resolver validation now passes the .text span; fixed look-ahead rejects when
  it exceeds the span.
deterministic_evidence: validator_span_bounds=PASS; test_cmd PASS
```

Normal and boundary byte-window cases are covered. A complete matrix of
truncated Zydis instructions and every resolver-specific near-end case remains
additional deterministic test work.

## S4 — Cinematic installation pre-commit gate

```yaml
status: FIXED
commit_gate_contract: >-
  Cinematic aspect/FOV hooks are created StartDisabled. The transaction enables
  them only after both components install successfully. Observation-only A1
  installation enables only the FOV lifecycle pair. Rollback disables/resets
  partial hooks.
deterministic_evidence: commit_failure=PASS; cinematic initialization harness PASS
```

## S5 — Durable config staging contract

```yaml
status: FIXED
persistence_contract: >-
  PersistConfigValue now verifies write, explicit flush, explicit close, and
  only then performs MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH.
deterministic_evidence: config persistence harness PASS
```

## S6 — Runtime viewport plausibility

```yaml
status: FIXED
plausibility_contract: >-
  Auto client viewport resolution accepts arbitrary ratios when both client
  dimensions are usable, and rejects dimensions below the generic 16-pixel
  minimum so minimize/resize degeneracy falls through to display/native
  fallback. No aspect-ratio whitelist was added.
arbitrary_aspect_preserved: YES
deterministic_evidence: viewport_plausibility=PASS; existing aspect tests PASS
```

## S7 — WorkerLifecycle ResumeThread failure

```yaml
status: PARTIAL
lifecycle_contract: >-
  ResumeThread failure no longer drops the suspended thread handle: ownership
  is retained long enough to terminate, join, and close the thread, and the
  worker is not published as started.
deterministic_evidence: normal lifecycle harness PASS; failure injection unavailable
```

## Cross-finding validation

- A1 Dialogue capability, camera/dialogue, Gameplay, and cinematic harnesses: PASS.
- Production and diagnostic separation: both builds PASS.
- `test.cmd`: PASS.
- `build.cmd`: PASS.
- `build-diagnostic.cmd`: PASS.
- `git diff --check`: PASS apart from normal Git line-ending warnings.
- Runtime/game launch: NOT_PERFORMED.

Known build warnings remain the external Zydis C4201 warnings; no new compiler
errors were introduced.

## Final verdict

```yaml
S1: PARTIAL
S2: PARTIAL
S3: PARTIAL
S4: FIXED
S5: FIXED
S6: FIXED
S7: PARTIAL
test_cmd: PASS
production_build: PASS
diagnostic_build: PASS
diff_check: PASS
runtime: NOT_PERFORMED
```
