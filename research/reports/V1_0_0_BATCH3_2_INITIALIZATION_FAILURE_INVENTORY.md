# v1.0.0 Batch 3.2 — Initialization Failure Inventory

## Scope

This is a bounded inventory of failure paths reachable during
`plugin::Initialize`. It does not change catch blocks, hook ordering, feature
behavior or rollback semantics.

The intended policy is graceful degradation where independent features can
continue safely. An outer exception is not itself proof that the whole Runtime
must be rolled back.

## Acquisition order

The current initialization sequence is:

1. resolve module/config/log paths and truncate the log file;
2. create the logger;
3. create the shared worker stop event;
4. compute module/game executable hashes;
5. optionally install research-only post-EXIT trace hooks;
6. load or create configuration, falling back to defaults on load failure;
7. optionally install the dialogue boundary hook;
8. optionally resolve and install the cinematic aspect-store hook;
9. optionally resolve and install cinematic ENTER/EXIT FOV hooks;
10. optionally resolve and install the gameplay camera hook;
11. start configured hotkey and diagnostic workers.

## Failure-path inventory

| Failure source | Already acquired | Current handling | Semantic classification |
| --- | --- | --- | --- |
| log file/logger creation | path/log file only | logger exception returns from `Initialize` | Runtime prerequisite classification remains open; logging loss may be recoverable, but current policy is fatal stop |
| worker stop-event creation | logger | throws to outer handler | Shared worker prerequisite only; feature-local if no worker-dependent feature is enabled; classification open |
| executable hash unavailable | logger/stop event | optional trace hooks are skipped; production continues | Feature-local/research-only recoverable |
| research trace install fails | logger/stop event and prior trace hooks | logs `NOT_INSTALLED`; production continues | Feature-local/research-only recoverable |
| config create/read/parse failure | logger/stop event | defaults remain active; production continues | Feature-local/recoverable |
| dialogue boundary resolve/install fails | logger/stop event/config | native dialogue policy is retained; other features continue | Feature-local/recoverable |
| cinematic aspect-store resolve/install fails | logger/stop event/config/dialogue state and possibly dialogue hook | throws to outer handler; current implementation stops later initialization | Feature-local in principle if gameplay/other domains can safely continue; current global-stop handling is a candidate |
| cinematic ENTER/EXIT FOV resolve/hook creation fails | prior optional hooks may be installed | throws to outer handler | Feature-local in principle when cinematic override is independent; current global-stop handling is a candidate |
| gameplay camera resolve/hook creation fails | prior optional hooks may be installed | throws to outer handler | Feature-local in principle when gameplay is independent; current global-stop handling is a candidate |
| hotkey worker creation fails | all requested hooks and shared stop event | throws to outer handler | Feature-local/recoverable if hotkeys are optional; current global-stop handling is a candidate |
| diagnostic worker creation fails | all requested production hooks and earlier workers | throws to outer handler | Research-only/feature-local; current global-stop handling is a candidate |
| unknown exception in outer initialization body | unknown subset of resources | catch-all stops workers but does not reset hooks/aspect | Runtime-fatal if ownership is genuinely uncertain; cleanup asymmetry remains open |

## Important dependency boundary

Dialogue has a presentation dependency on the gameplay FOV outcome, but not
necessarily a hard initialization dependency on the gameplay hook. If Gameplay
is intentionally disabled, Dialogue can still initialize and operate; however,
the underlying gameplay FOV may remain incorrect for an ultrawide user, making
dialogue framing appear worse. With dialogue scaling disabled, that quality
impact may be absent or not noticeable.

This distinction is therefore:

```text
Gameplay → Dialogue technical initialization
    not established as a hard dependency

Gameplay FOV outcome → Dialogue visual quality
    soft/presentation dependency
```

The following are independent enough to require explicit graceful-degradation
consideration rather than automatic all-or-nothing rollback:

```text
Dialogue boundary failure
    → native dialogue behavior
    → gameplay/cinematics may continue

Cinematic override failure
    → native cinematic behavior
    → gameplay/dialogue may continue if their own hooks are valid

Gameplay hook failure
    → gameplay aspect correction unavailable
    → cinematic/dialogue may continue if their own dependencies are valid

Hotkey/diagnostic worker failure
    → affected control/diagnostic path unavailable
    → already-installed production features may continue
```

These are semantic candidates, not implementation changes. The current code
still routes several of them through the outer fatal-style catch.

## Current finding status

- `catch (std::exception&)` versus `catch (...)` asymmetry remains open.
- Gameplay, Cinematics and Dialogue are technically independent user-facing
  features. A missing feature can reduce presentation quality without making
  the other features unsafe to run.
- `DISABLED` and `FAILED` remain distinct diagnostic states, but neither should
  automatically invalidate independently initialized features.
- Unknown ownership state after a non-standard exception is the strongest
  runtime-fatal candidate.
- A runtime-fatal classification is reserved for shared infrastructure or
  ownership corruption where safe continuation cannot be established.

## Final dependency and failure classification

```text
Gameplay requested + initialization fails
    → Gameplay correction unavailable
    → Cinematics/Dialogue may continue

Cinematics requested + initialization fails
    → native cinematics
    → Gameplay/Dialogue may continue

Dialogue requested + initialization fails
    → native dialogue behavior
    → Gameplay/Cinematics may continue

Shared Runtime ownership or lifecycle state is invalid/unknown
    → runtime-fatal candidate
    → stop without leaving unsafe partial state
```

Presentation impact on ultrawide is recorded as a quality relationship only:
the absence of one correction may make the final image visibly incomplete, but
it is not a hard initialization dependency for the other features.

### Current behavior versus contract

- Config failure, dialogue failure and optional trace failure already follow the
  feature-local/native-fallback contract.
- Gameplay hook failure, cinematic hook failure and optional worker startup
  failure currently route through the outer fatal-style path despite the
  independent-feature contract; these are the concrete behavior-change
  candidates.
- Unknown exceptions remain runtime-fatal candidates because ownership may be
  uncertain, but the catch asymmetry cannot be fixed until the boundary between
  feature-local and runtime-fatal failure is implemented deliberately.

## Non-goals

- no catch-block changes;
- no transactional `HookSet` redesign;
- no installation-order changes;
- no cinematic failed-write changes;
- no config persistence changes;
- no feature behavior changes.

## Next gate

Define the smallest behavioral contract for feature-local failure and
runtime-fatal failure separately. Only then decide whether the current outer
catch asymmetry is a defect and what the smallest correction is.
