# Production Architecture

## Runtime ownership

The translation-unit-private `RuntimeState` in `src/plugin/runtime.cpp` is the
primary integration and lifetime owner for production runtime state: module identity,
configuration, workers, installed hooks, feature availability,
cinematic/gameplay transition state, dialogue state and runtime telemetry. It
is intentionally not a public `plugin::RuntimeState` type. Callback functions
use local `g_*` aliases to fields in that owner; those aliases are not
callbacks or separate ownership boundaries. Lifecycle fields are changed through one
externally serialized Runtime control path; worker code only observes the stop
event.

Domain modules own reusable calculations, policies and resolver primitives.
The translation-unit-private `RuntimeState` owns their integration, lifecycle
and cross-domain coordination. A separate coordinator type is not required
unless it can own an independent state machine without receiving the entire
runtime as callbacks.

User-facing feature configuration and lifecycle capability are separate
concerns. A domain may install an observation-only dependency for another
domain without enabling its presentation intervention. A dependent
non-Native Dialogue lifecycle is available only when its cinematic lifecycle
observation and Gameplay recovery observation capabilities are available;
otherwise it fails closed while unrelated feature status remains independent.

## Build and test policy

`build.cmd` is the supported production build entry point. It discovers the
documented Visual Studio toolchain, uses C++23-era MSVC facilities and applies
`/W4` to the production compilation. Warnings are reviewed but `/WX` is not
currently required because vendor sources are part of the same bounded build
and do not have the project's warning policy.

`test.cmd` is the single repository test entry point. It builds and runs all
current Windows harnesses with the same compiler family and returns failure if
any harness fails. Test outputs are generated only under the ignored
`build-artifacts/` directory.

## ASI lifetime contract

The supported production model is **loaded until process termination**.
Normal dynamic `FreeLibrary`/manual ASI unload is not claimed as supported.

- `DllMain(DLL_PROCESS_ATTACH)` starts initialization asynchronously and does
  not retain a joinable initialization handle.
- Runtime workers and installed hooks are owned by the runtime state.
- Controlled shutdown must occur outside the loader-lock boundary: signal the
  workers, join them, then reset hooks and restore state.
- `DLL_PROCESS_DETACH` on process termination performs no complex teardown.
- Normal detach only sends the stop notification and must not perform joins,
  hook reset or memory restoration under loader lock.
- The runtime owner is deliberately process-resident and is not destroyed by
  DLL static teardown. Normal dynamic unload remains unsupported.

Loaders and integrations must keep the ASI resident until process termination
unless a future implementation establishes a non-loader-lock owner for a
complete controlled unload.
