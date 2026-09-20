# v1.0.0 Final Architecture Hardening — A1–A5 Closure Review

## Scope and method

This is a read-only closure review of the approved A1–A5 hardening scope. It
does not introduce a third architecture audit, a new wishlist, source changes,
builds, harness runs or a game launch.

Existing validation evidence is recorded, not repeated:

- `build.cmd`: PASS.
- `WorkerLifecycle` harness: PASS.
- `FeatureStatus` harness: PASS.
- `ConfigPersistence` harness: PASS.
- `git diff --check`: PASS.
- Runtime validation after A1–A5: NOT YET RUN.

## A1 — Runtime ownership: CLOSED

The translation-unit-private `RuntimeState` is the explicit lifetime owner in
`src/plugin/runtime.cpp` for module identity, workers, hooks, configuration,
feature availability, transition state, dialogue state and telemetry. Existing
`g_*` identifiers are field aliases into that object, preserving validated
control flow while making ownership legible from the production structure;
callback functions remain functions.

`docs/ARCHITECTURE.md` documents the boundary: domain modules own reusable
logic; `RuntimeState` owns integration, lifecycle and cross-domain
coordination. No current-code contradiction requires an artificial
`plugin::Runtime` class or a separate coordinator type.

## A2 — ASI lifetime contract: CLOSED

Implementation and documentation agree on process-lifetime use:

- initialization begins asynchronously from `DLL_PROCESS_ATTACH`;
- runtime state owns workers and installed hooks;
- controlled stop/join/reset belongs outside loader lock;
- process-termination detach performs no complex teardown;
- normal detach signals only; and
- normal dynamic `FreeLibrary`/manual unload is explicitly not claimed.

The implementation does not promise a supported unload path beyond this
contract.

## A3 — Reproducible production build: CLOSED

`build.cmd` contains no author-specific Visual Studio path. It discovers a
supported Visual Studio installation through `vswhere`, requires the x64 C++
toolchain in the MSVC 17.14+ range, uses explicit `/std:c++latest` for that
toolchain's C++23-era support and compiles repository-relative source and
dependency paths. `README.md` documents the same entry point and requirement.

No CMake/MSBuild migration is required to satisfy the approved reproducibility
contract.

## A4 — Production/test separation and engineering documentation: CLOSED

The stable atomic gameplay and post-cinematic handoff behavior no longer
depends on production-defined test-named macros. The remaining references to
`POST_CINEMATIC_GAMEPLAY_REPLAY_ATOMIC_EXIT_HANDOFF_TEST` occur only inside
the separately gated `POST_CINEMATIC_GAMEPLAY_REPLAY_DEFER_TEST` diagnostic
branch; `build.cmd` defines neither branch.

The production scanner no longer includes `helper.hpp`; its bounded pattern
and executable-section helpers are local to `signature_scanner.cpp`. No new
PE/memory rewrite was required.

The following documents match the current implementation and evidence scope:

- `docs/ARCHITECTURE.md` — ownership and lifetime;
- `docs/SAFETY_INVARIANTS.md` — fail-closed and persistence/lifecycle
  contracts; and
- `docs/UPDATING_GAME_VERSION.md` — resolver/update provenance workflow.

## A5 — Repository hygiene: CLOSED

The approved generated-artifact cleanup removed 149 generated files from the
working tree and Git index. The current index contains zero matching generated
paths in the approved areas:

- root-level generated `.obj`/`.exe`;
- `build-artifacts/obj`; and
- `research/tools/CUE4ParseWVF/bin|obj`.

`.gitignore` covers regenerated outputs. Nine intentionally preserved release
archives remain tracked. The dirty working tree reflects the uncommitted
v1.0.0 refactor and does not contradict the hygiene result.

## Independent-review blocker reconciliation

| Confirmed blocker | Final disposition |
| --- | --- |
| Implicit runtime ownership | CLOSED |
| ASI lifetime/unload contract | CLOSED |
| Non-reproducible production build | CLOSED |
| Production `_TEST` dependency | CLOSED |
| Repository hygiene | CLOSED |

## Non-blocking architecture-quality backlog

The following items are not defects by default and do not block this closure:

- transactional `HookSet` design;
- broader DI/fakes and orchestration testing;
- alternative build-system/CMake evaluation;
- `WorkerLifecycle` API/design quality;
- further `runtime.cpp` decomposition;
- PE/memory validation improvements;
- remaining legacy `helper.hpp` cleanup;
- richer signature/resolver descriptors and fixtures;
- local visibility of the cinematic failed-write contract; and
- harness/evidence quality refinement.

Any change in the subsequent quality pass requires concrete evidence that the
current design is inferior for this project's production-quality v1.0 goal in
ownership, safety, testability, update resilience or code clarity. A cleaner
looking alternative alone is insufficient.

## Verdict

**A1–A5 closure: APPROVED.**

**Architecture freeze candidate: APPROVED.**

This is not yet the operational `ARCHITECTURE FREEZE`: the agreed optional
architecture-quality pass must first classify its bounded items as justified,
deferred, preferred-current-design or rejected. No production change is
authorized by this closure review, and Batch 4 regression remains pending.
