# v1.0.0 Independent Review Finding-by-Finding Triage

## Scope and evidence boundary

This is a bounded read-only triage of the independent architecture/code-quality
review against the current checkout. No production source, tests, build
scripts, repository artifacts or runtime behavior were changed or executed by
this triage. The classifications below distinguish actual current evidence
from proposed designs.

## Triage table

| Finding | Independent-review claim | Current evidence | Reachability / impact | Classification | Blocks v1.0? | Smallest justified next action |
|---|---|---|---|---|---|---|
| Runtime ownership | No concrete `plugin::Runtime`; mutable state is implicit in `runtime.cpp` globals. | `runtime.hpp` exposes free lifecycle functions; hooks, workers, config, feature state, logger, addresses and transition state are owned in the anonymous namespace of `runtime.cpp`. | Reachable in every normal initialization and callback path; a maintainer must reconstruct ownership from one large integration TU. | **ARCHITECTURAL BLOCKER FOR v1.0** | YES | Define and document the final ownership boundary, then perform one bounded ownership extraction only if it reduces implicit state without changing behavior. A class is not required merely for naming or LOC. |
| Runtime class as a specific solution | A concrete `plugin::Runtime` class is required. | The need is for explicit ownership and lifetime, not a particular type name. | Design preference, not independently proven defect. | **RECOMMENDATION ONLY** | NO | Choose the smallest representation that makes ownership explicit. |
| Worker `Start()` publication | A worker can begin before its handle is stored, creating a publication defect. | Production workers use the shared stop event and do not inspect the worker-handle array. The handle is stored before `Start()` returns; no reachable worker path consumes the unpublished handle. | The described race is not shown to affect current production behavior. | **FALSE POSITIVE** | NO | Preserve as a documented review note; do not rewrite on this claim alone. |
| Worker concurrent mutation | `Start()`/`StopAndJoin()` mutate arrays and counts without synchronization. | Production startup and controlled shutdown are owner-thread operations; `DllMain` calls only const `SignalStop()`. The public API does not state this contract, and the harness does not test concurrent owner mutation. | Potential misuse and maintainability risk; no reachable production concurrent mutation was established. | **MAINTAINABILITY IMPROVEMENT** | NO | Document thread-affinity and permitted call contexts; add a concurrency test only if the contract is intentionally widened. |
| Worker self-join | Self-join is unsafe and the harness race does not prove safety. | `StopAndJoin()` checks the current thread ID and refuses to wait on itself. Production workers do not call `StopAndJoin()`; the current harness intentionally avoids claiming concurrent self-join coverage. | Self-join refusal is handled; the lifecycle contract is externally serialized and does not prove arbitrary concurrent shutdown safety. | **ALREADY HANDLED** | NO | Keep the self-join contract explicit; do not claim broader concurrency proof than the harness provides. |
| Worker `INFINITE` wait | An unbounded join can hang shutdown. | Workers wait on an interruptible stop event; normal production loops are designed to terminate after signaling. `StopAndJoin()` joins outside `DllMain`. | A stuck or future non-cooperative worker could block controlled shutdown; no current stuck-worker evidence exists. | **MAINTAINABILITY IMPROVEMENT** | NO | Keep as a bounded lifecycle contract decision, not an automatic rewrite. |
| Worker raw handles/destructor | Raw handles and a default destructor are unsafe. | `WorkerLifecycle` closes worker handles and the stop event on successful `StopAndJoin()`. The supported process-lifetime path does not rely on destructor teardown. | Ownership is real but its lifetime contract is implicit; no current leak in the validated controlled-stop path was shown. | **MAINTAINABILITY IMPROVEMENT** | NO | Document ownership and process-lifetime assumptions; consider RAII only in a separate bounded lifecycle change. |
| DLL lifetime / normal unload | Initialization handle is detached and normal `FreeLibrary` can race code/hooks. | `DllMain` closes the initialization-thread handle; normal detach only signals workers, while process termination skips teardown. The project explicitly does not claim normal DLL unload support. | Normal dynamic unload is not a supported production scenario, but the contract is not sufficiently visible in the production documentation. | **ARCHITECTURAL BLOCKER FOR v1.0** | YES | Make the supported lifetime model explicit in production architecture/safety documentation and loader assumptions. Do not claim safe normal unload without a real owner outside loader lock. |
| Build reproducibility | `build.cmd` depends on an absolute VS path, `/std:c++latest`, manual source list and production `_TEST` defines. | All are directly present in `build.cmd`; README documents VS 2022/C++23 but does not make the author's absolute path reproducible. | A second developer cannot reliably clone and build without reconstructing the author's environment; test-only behavior is compiled into production. | **ARCHITECTURAL BLOCKER FOR v1.0** | YES | Create one documented portable production build path: fixed C++23, toolchain discovery/configuration, explicit production defines and separate harness/research builds. CMake is optional. |
| Harness evidence strength | Current harnesses do not prove the orchestration claims attributed to them. | `feature_status_harness` tests enum names/independence only; `worker_lifecycle_harness` exercises the real lifecycle object but not concurrent owner mutation; config harness checks actual file contents across success/failure paths. | Some reports must narrow their claims; this is an evidence/documentation gap, not proof that production behavior is wrong. | **MAINTAINABILITY IMPROVEMENT** | NO | Correct claim-to-evidence wording and add orchestration/fault-injection coverage only for a specifically required contract. |
| DI/fakes everywhere | Real orchestration tests require dependency injection/fakes. | No evidence establishes that DI is necessary for the current bounded contracts. | Proposed testing architecture, not a current defect. | **RECOMMENDATION ONLY** | NO | Add seams only where a concrete untestable contract remains. |
| Resolver definitions and validation | Resolver maintenance is too implicit; typed descriptors are needed. | Signatures are centralized; resolvers enforce cardinality and instruction validation; current runtime logs executable hashes; gameplay and cinematic resolvers have separate structural checks. | Basic fail-closed behavior is present. Per-signature provenance/update workflow is incomplete and must be reconstructed from comments/reports. | **MAINTAINABILITY IMPROVEMENT** | NO | Add a compact supported-build/update workflow and provenance index; do not introduce `SignatureDescriptor` without a demonstrated need. |
| `helper.hpp` dependency | Legacy helper remains in production and contains unsafe write APIs. | `runtime.cpp` includes `helper.hpp`; `signature_scanner.cpp` uses its `Memory::PatternScanAll` path. `Memory::Write` and `PatchBytes` are not reachable from the current stable runtime write path. | Scanner dependency is reachable; unsafe write helpers are dead/legacy in the current stable call graph. | **MAINTAINABILITY IMPROVEMENT** | NO | Boundedly replace the scanner dependency or isolate the legacy header after higher-priority ownership/build work. Do not perform a full helper rewrite from this finding alone. |
| PE boundary validation | DOS/NT header and section bounds are not fully checked. | Gameplay/cinematic resolvers validate magic/signature and inspect sections, but do not fully validate `e_lfanew`, NT header range or section-table bounds before dereference. | Defensive weakness for malformed/unexpected module input; normal `GetModuleHandle(nullptr)` points to the loaded executable and no malformed-image runtime evidence exists. | **MAINTAINABILITY IMPROVEMENT** | NO | Add bounded image-bound validation if the resolver safety contract is expanded; no patch-specific failure is currently proven. |
| Central memory-read/write contract | Readability/protection logic should be unified and overflow guarded. | `platform::win32::ReadMemory`/`IsWritable` provide a shared query contract; cinematic application guards `base + offset`, while gameplay `WriteAspectAndFlags` relies on the trusted validated source and does not perform the same explicit overflow guard. | Defensive consistency gap; no reachable valid-source overflow evidence. | **MAINTAINABILITY IMPROVEMENT** | NO | Review the shared memory contract with concrete overflow/boundary tests before changing gameplay behavior. |
| Cinematic failed-write documentation | Skipping the native store and advancing RIP is too dangerous to remain only in reports. | Batch 3 classified this path as intentional fail-closed behavior and the source contains the guarded continuation, but the production architecture documentation does not present the invariant prominently. | A maintainer can misinterpret or “fix” the behavior; no confirmed runtime defect. | **MAINTAINABILITY IMPROVEMENT** | NO | Add a concise local safety contract/ADR-style note during documentation finalization. |
| Production `_TEST` defines | Production behavior depends on test-named compile-time macros. | `build.cmd` defines `GAMEPLAY_FIX_ATOMICITY_TEST` and `POST_CINEMATIC_GAMEPLAY_REPLAY_ATOMIC_EXIT_HANDOFF_TEST`; these alter compiled runtime paths. | This undermines production/test separation and makes the current artifact semantics harder to reason about. | **ARCHITECTURAL BLOCKER FOR v1.0** | YES | Decide and document whether these are now stable production behavior; rename/promote or isolate them in a bounded build/source change before release. |
| Forwarding wrappers / partial extraction | Wrappers and incomplete boundaries add navigation noise. | Thin wrappers remain in `runtime.cpp`; several domain modules own primitives while cross-domain coordination remains centralized by design. | Some wrappers are intentional integration boundaries; not all are incomplete extraction. | **MAINTAINABILITY IMPROVEMENT** | NO | Review only wrappers with no ownership or semantic value; do not split coordinator logic merely to reduce file length. |
| Repository hygiene | Tracked objects, test executables and research `bin/obj` outputs make the repository non-clean. | `git ls-files` confirms tracked `.obj` files and generated research/tool binaries; `.gitignore` covers `.asi`/PDB/ILK but not these outputs. The worktree is also intentionally dirty during active refactor. | Generated artifacts affect cloneability and release review; dirty worktree alone is not a defect. | **ARCHITECTURAL BLOCKER FOR v1.0** | YES | Before release, define an approved cleanup scope, extend ignore rules, remove generated artifacts from repository history/state where authorized, and perform a clean checkout review. Do not delete them during this triage. |

## Confirmed defects

No new reachable runtime safety defect was confirmed by this review beyond the
already handled findings. The review did confirm three release-readiness gaps:

- production build reproducibility is not yet independent of the author's
  local toolchain path and test defines;
- production behavior still depends on test-named compile-time switches;
- generated artifacts are tracked and repository hygiene is not release-ready.

These are classified as v1.0 architectural/release blockers rather than
gameplay defects.

## Architectural blockers for v1.0

1. Make runtime ownership and cross-domain lifetime explicit in production
   structure; a concrete `Runtime` class is optional, explicit ownership is
   not.
2. Document and enforce the supported ASI lifetime model, including the fact
   that normal dynamic unload is not claimed.
3. Make the production build reproducible with a fixed standard and explicit
   production/test separation; CMake is not required.
4. Remove or formally promote the production `_TEST` compile-time paths.
5. Clean generated artifacts and establish a release-ready repository state.

## Non-blocking maintainability work

- Document `WorkerLifecycle` thread-affinity, stop/join and handle-ownership
  contracts.
- Improve harness claim precision and add orchestration tests only where a
  required contract lacks evidence.
- Add resolver provenance and a game-update playbook.
- Isolate the scanner from legacy `helper.hpp` and review dead unsafe helpers.
- Harden PE/memory boundary validation with targeted tests.
- Document the cinematic failed-write invariant and remove valueless wrappers.

## Rejected or already-handled claims

- A concrete `plugin::Runtime` class as the only valid architecture.
- Transactional `HookSet` as an automatic requirement.
- DI/fakes everywhere.
- CMake as a mandatory build system.
- Full `WorkerLifecycle` rewrite based on the current claims alone.
- `Start()` handle-publication race as a demonstrated production defect.
- Self-join refusal: already handled by the current implementation.
- Legacy `Memory::Write`/`PatchBytes` as a reachable stable-runtime write
  defect.
- Dirty worktree as an architecture defect during active development.

## Minimal ordered pre-v1.0 worklist

1. Resolve/document the runtime ownership and ASI lifetime contracts without
   changing validated feature behavior.
2. Separate and stabilize the production build contract: fixed C++23,
   toolchain discovery, production/test defines and a documented clean build.
3. Perform the smallest justified production extraction for any ownership or
   legacy-helper dependency that remains after steps 1–2.
4. Correct release documentation and harness claims to match their actual
   evidence.
5. Execute an approved repository hygiene cleanup and read-only Git review.
6. Run the final Batch 4 regression only after the above production changes
   are complete.

## Triage stop condition

This report does not authorize implementation of any work item. Any next
change requires its own bounded scope, plan, validation and Git review. The
full Batch 4 regression remains stopped.
