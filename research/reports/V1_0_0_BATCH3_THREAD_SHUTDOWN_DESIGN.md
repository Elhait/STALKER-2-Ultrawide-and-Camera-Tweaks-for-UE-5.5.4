# v1.0.0 Batch 3 — Thread and Shutdown Design

## Scope

This design addresses the linked lifecycle cluster identified in the Batch 3.1
audit:

```text
CreateThread
  -> worker handle immediately closed
  -> no stop signal
  -> Shutdown cannot request and await termination
  -> DLL_PROCESS_DETACH
  -> hook reset and aspect restoration at the detach boundary
```

The design does not change hook rollback, failed-write continuation, config
persistence or feature behavior.

## Ownership

`plugin::Runtime` owns the worker lifecycle state:

- worker thread handles for hotkey, one-shot and diagnostic monitor workers;
- one process-wide stop event or equivalent stop signal;
- startup state and whether each worker was created successfully.

Workers do not own or close their own handles. The owner closes handles only
after a permitted join has completed, or after process termination has made
joining unnecessary.

## Stop protocol

Each loop must replace an unconditional sleep/loop condition with an interruptible
wait on the shared stop signal. A timeout preserves the current polling cadence;
the signaled result exits the worker without starting new work.

The stop operation is idempotent:

1. signal the shared stop state/event;
2. prevent new worker creation;
3. let workers observe the signal and return;
4. join only from a non-loader-lock controlled shutdown path;
5. close owned handles after the join.

No worker may wait for or join itself.

## Join boundary

Controlled shutdown may request stop and wait for workers only when called from
outside `DllMain` and outside a worker thread. This is the only path allowed to
perform ordered hook reset, aspect restoration and worker-handle cleanup.

`DllMain` must not become a hidden join point. It cannot safely perform an
unbounded wait or complex teardown under the loader lock.

## Detach semantics

The implementation must distinguish two operations:

### Controlled shutdown

Used by an explicit non-loader-lock owner. It may stop workers, join them, reset
hooks in the established order and release owned resources.

### Detach-safe path

Used by `DLL_PROCESS_DETACH`. It must be minimal and non-blocking:

- signal workers only if that operation is safe for the chosen primitive;
- never wait for worker termination;
- never perform complex hook rollback or aspect restoration under the loader
  lock;
- on process termination (`lpReserved != nullptr`), skip teardown entirely;
  the operating system is terminating the process and no reusable plugin state
  remains.

Because the current plugin has no external non-loader-lock unload entry point,
normal unload support is not considered complete until such a controlled owner
exists. The implementation batch must not claim that `DllMain` itself provides
safe normal-unload cleanup.

## Implementation gate

Implementation may begin only if it preserves these contracts:

- worker polling cadence remains equivalent when no stop is requested;
- startup failure remains fail-closed and does not leak created worker handles;
- a worker cannot observe partially destroyed Runtime state;
- process termination does not wait or execute complex cleanup;
- controlled shutdown is separately testable from detach notification.

## Validation gate

The implementation requires:

- build success;
- unit or harness coverage for stop-before-start, partial-start failure,
  idempotent stop, join ordering and self-join refusal;
- targeted runtime validation of normal operation and controlled stop;
- a separate review that `DllMain` detach performs no blocking teardown.

No implementation or runtime behavior was changed by this design document.
