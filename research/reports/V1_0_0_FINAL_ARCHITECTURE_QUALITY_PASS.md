# v1.0.0 Final Architecture Quality Pass

## Scope

This is one bounded, read-only review after A1–A5 closure. It evaluates the
ten agreed architecture-quality questions; it does not reopen the completed
safety audit, seek new defects, build, run harnesses, launch the game or modify
production code.

No proposal is treated as a problem by default. A v1.0 change is justified
only where current-code evidence shows a concrete benefit for ownership,
safety, lifecycle correctness, testability, update resilience, maintainability
or public production-code clarity.

## 1. Transactional `HookSet` / hook ownership

**Current design:** `hooks::HookSet` owns hook objects. `RuntimeState` owns
feature-local failure cleanup, shared-fatal cleanup and shutdown ordering.

**Evidence:** Each feature installation boundary resets only its own hook on
failure. `ResetAllRuntimeResources()` stops workers before resetting the
remaining hooks and restoring cinematic state. Batch 3.5 found no orphaned
production hook or reachable cleanup gap.

**Benefit/risk:** A transaction object would duplicate coordinator knowledge
and obscure the feature-local degradation contract without eliminating a
confirmed weakness.

**Classification:** **CURRENT DESIGN PREFERRED.**

## 2. `WorkerLifecycle` design quality

**Current design:** `WorkerLifecycle` explicitly owns a stop event and up to
three worker handles. `SignalStop()` and `StopAndJoin()` are intentionally
separate; self-join retains the handle and returns failure. The supported ASI
lifetime is process-resident, not destructor-driven normal unload.

**Evidence:** The real production primitive is exercised by the lifecycle
harness for startup, stop, repeated stop, partial startup failure and
stop-before-start. The owner-thread contract is explicit; self-join refusal
remains a defensive production guard but is not presented as concurrent
thread-safety evidence.

**Benefit/risk:** RAII/destructor cleanup or a larger state-machine rewrite
would risk implying unsupported unload semantics. An `INFINITE` join is limited
to explicit controlled shutdown outside loader lock, which is the accepted
contract.

**Classification:** **CURRENT DESIGN PREFERRED.** Full lifecycle rewrite is
**OVERENGINEERING / REJECT** for v1.0.

## 3. Further `runtime.cpp` decomposition

**Current design:** domain calculations, policies, resolvers and state types
live in their feature modules. `RuntimeState` retains composition, worker and
hook lifetime, feature status, telemetry and cross-domain transitions.

**Evidence:** The remaining transition functions coordinate gameplay,
cinematic and dialogue state with hook contexts and runtime lifetime. No
remaining block has a natural independent owner without forwarding most of the
runtime back through callbacks.

**Benefit/risk:** A coordinator type or additional forwarding layer would add
indirection without reducing real ownership coupling.

**Classification:** **CURRENT DESIGN PREFERRED.**

## 4. DI/fakes and orchestration tests

**Current design:** the worker and persistence harnesses execute their real
production implementations. Feature-status coverage is deliberately narrow;
game-hook orchestration remains a runtime regression concern.

**Evidence:** No specific invariant requires a fake scanner, fake hook API or
filesystem abstraction to be tested today. Existing source/static review and
the final game regression cover the integration boundary those fakes would
otherwise model.

**Benefit/risk:** A DI framework would broaden public APIs and increase the
number of test-only seams before a concrete missing invariant is identified.

**Classification:** DI framework: **OVERENGINEERING / REJECT**. Additional
fault-injection orchestration testing: **NICE IMPROVEMENT — DEFER**.

## 5. CMake/MSBuild versus `build.cmd`

**Current design:** `build.cmd` discovers Visual Studio through `vswhere`,
selects MSVC 17.14+ with x64 tools, uses explicit language mode and repository-
relative paths, and builds one unambiguous production ASI target.

**Evidence:** This closes the reproducibility blocker without a machine-
specific path. No current target split, warning-policy or IDE integration
problem blocks the documented build workflow.

**Benefit/risk:** A build-system migration would be a presentation preference,
not a demonstrated improvement to this project before v1.0.

**Classification:** **CURRENT DESIGN PREFERRED.**

## 6. Signature/resolver architecture

**Current design:** signatures are centralized; feature modules own their
specific resolution and structural validation; generic scanner and decoder
helpers are isolated; update provenance is documented.

**Evidence:** The resolver contract already includes unique-match handling,
instruction shape, operand/register checks and fail-closed behavior. A typed
`SignatureDescriptor` would still need feature-specific validators and ABI
rules, so it would not remove the meaningful code.

**Benefit/risk:** Typed descriptors and multi-version fixtures could improve
future patch maintenance, but no current update ambiguity demonstrates that
their complexity is justified.

**Classification:** typed descriptor/fixture expansion:
**NICE IMPROVEMENT — DEFER**. Current production resolver structure:
**CURRENT DESIGN PREFERRED.**

## 7. PE/memory validation

**Current design:** write paths validate committed writable ranges; scanner
and resolvers validate known executable-image candidates; `ReadMemory()`
checks non-null committed, non-guard and non-noaccess memory plus full region
range before copying.

**Concrete evidence:** `platform::win32::ReadMemory()` does not require a
readable protection class before `memcpy`. Its name and use as `SafeRead()`
promise a stronger contract than its current protection check. A committed
`PAGE_EXECUTE` region, for example, is neither `PAGE_NOACCESS` nor guarded but
is not a documented readable page.

**Benefit/risk:** This is a small, centralized hardening change: reject
non-readable committed regions before copying. It strengthens every reachable
`SafeRead()` caller without changing hook ownership, resolver policy or the
memory subsystem architecture.

**Classification:** **JUSTIFIED FOR v1.0.**

**Minimal action:** add an explicit readable-protection predicate to
`ReadMemory()` and a bounded platform-memory test that proves readable pages
succeed and execute-only/non-readable pages are refused. Do not rewrite PE
parsing, add a new memory framework or change write semantics.

## 8. Legacy `helper.hpp`

**Current design:** production no longer includes the legacy helper; the build
compiles only `src/` production modules and external dependencies. Fifty-four
archived probes/traces still include `helper.hpp` as shared research support.

**Evidence:** Moving or deleting the header would require a broad research
include migration but would not improve the production include graph, which is
already clean. Its unsafe APIs are not reachable from stable production code.

**Benefit/risk:** Relocation would be cosmetic unless a future research build
surface is standardized. It could reduce archival reproducibility of existing
probes.

**Classification:** **NICE IMPROVEMENT — DEFER.**

## 9. Cinematic failed-write fail-closed contract

**Current design:** on an unwritable target, the callback logs refusal, skips
both the custom and native store, then advances past the validated ten-byte
instruction.

**Evidence:** `ApplyCinematicAspectStore()` includes a local comment explaining
why advancing `RIP` is safe for that replacement, and
`docs/SAFETY_INVARIANTS.md` records the same fail-closed behavior.

**Benefit/risk:** The accepted nontrivial contract is already visible both at
the integration point and in production documentation.

**Classification:** **CURRENT DESIGN PREFERRED.**

## 10. Harness/evidence claims

**Current design:** WorkerLifecycle directly exercises production lifecycle
code; ConfigPersistence directly exercises production staging/replacement
paths; FeatureStatus exercises the status vocabulary only.

**Concrete evidence:** `tests/feature_status/feature_status_harness.cpp`
constructs enum values directly. It does not execute feature initialization,
hook installation, local rollback or shared-fatal cleanup. The historical
Batch 3 closure wording calls it a “Feature-status/graceful-degradation
harness”, which overstates what this executable proves.

**Benefit/risk:** Correcting the claim improves evidence integrity without
adding an artificial hook simulation or altering implementation behavior.

**Classification:** **JUSTIFIED FOR v1.0.**

**Minimal action:** amend the current closure/evidence wording so
FeatureStatus is described as status-vocabulary coverage; retain source review
and final in-game regression as evidence for feature-local degradation and
runtime orchestration. Do not add a fake hook framework merely to make the
label broader.

## Minimal justified v1.0 worklist

1. Strengthen `platform::win32::ReadMemory()` to require readable committed
   page protection, with a bounded platform-memory test.
2. Correct FeatureStatus harness/closure evidence wording to match its actual
   enum-level coverage.

The first item requires a new narrow implementation plan and validation. The
second is a documentation/evidence correction and must not be presented as a
new runtime test.

## Deferred, rejected and current-preferred decisions

- Transactional `HookSet`: current design preferred.
- WorkerLifecycle RAII/full rewrite: current design preferred / full rewrite
  rejected.
- Further runtime decomposition: current design preferred.
- DI framework: rejected; broader orchestration fault injection deferred.
- CMake/MSBuild migration: current design preferred.
- Typed signature descriptors and fixture expansion: deferred.
- `helper.hpp` research relocation: deferred.
- Cinematic failed-write documentation: current design preferred.

## Result

**QUALITY PASS — BOUNDED v1.0 CHANGES JUSTIFIED.**

Only the two listed actions may be planned next. Architecture freeze remains
pending their bounded implementation/validation and a subsequent final review.
