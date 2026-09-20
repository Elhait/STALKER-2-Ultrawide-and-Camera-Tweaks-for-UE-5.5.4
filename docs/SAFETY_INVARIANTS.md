# Production Safety Invariants

- Ambiguous, incomplete or structurally invalid signature matches fail closed.
- Resolver decode, fixed look-ahead, relative-call and byte-window validation
  are bounded by the validated executable section span.
- The gameplay and cinematic resolvers validate match cardinality and
  instruction shape before installing hooks. The executable SHA-256 is logged
  as identity/support evidence; it is not currently an allowlist gate. An
  unknown hash still cannot install a hook without a unique structurally valid
  resolver result.
- Gameplay, cinematics and dialogue are independently configurable user-facing
  interventions, but a feature may require observation capabilities installed
  by another domain. Missing required observation capability fails the
  dependent intervention closed without invalidating unrelated features.
- A failed configuration persistence operation does not truncate or replace
  the last-known-good INI.
- Cinematic aspect-store failure skips the custom/native store path and
  resumes after the replaced instruction; this is intentional fail-closed
  behavior.
- Cinematic hooks are created disabled and are enabled only after the required
  component installation commit; observation-only lifecycle installation does
  not enable presentation intervention.
- Staged configuration is flushed and closed successfully before atomic
  replacement of the managed INI.
- Auto viewport aspect uses only usable client geometry; transient degenerate
  dimensions fall back to the existing display/native path.
- Controlled shutdown signals and joins workers before resetting hooks or
  restoring patched state. Loader-lock detach does not perform that teardown.
- Runtime callbacks that mutate ordinary telemetry fields are expected to run
  on the game/runtime owner thread. Atomic fields are used for cross-thread
  observations; `WorkerLifecycle` is an externally serialized lifecycle
  primitive, not a general concurrent thread-management API. Lifecycle calls
  must not overlap initialization or one another; DLL detach only signals the
  stop event.
- The player's selected FOV is preserved; aspect correction does not use a
  hard-coded FOV compensation multiplier.
- Build success is not runtime proof. Runtime compatibility claims require
  executable identity, log evidence and the named regression scenario.
