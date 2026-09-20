# v1.0.0 — Remove Unnecessary Gameplay-Disabled Camera Observer

## Objective

Remove the shared gameplay camera-writer observer when Gameplay is disabled and
the only remaining reason to resolve camera-writer infrastructure is
`Cinematics=Auto`. Preserve cinematic aspect/FOV behavior and all enabled
Gameplay behavior while eliminating the repeated observer read path.

## Established evidence and current state

- With `Gameplay=false`, `ReplayManualTransition` does not reach the gameplay
  replay/write path; it only reads the camera source and updates
  `g_lastObservedAspect`.
- The observer performs `SafeRead`, which reaches `VirtualQuery` and `memcpy`
  on every camera-writer hit.
- `XMM0` at the validated `MOVSS [RBX+0x30], XMM0` writer carries FOV, not the
  aspect value used by the observer.
- `Cinematics=Auto` resolves aspect from the current client viewport, not from
  `g_lastObservedAspect`.
- No required Auto consumer for continuous camera-writer observation was found.
- The observer hot-path design audit selected bounded Design D.

## Approved scope

- Change only the initialization/install condition for the shared gameplay
  camera-writer hook.
- Keep the hook installed when Gameplay is enabled.
- Do not install it solely for `Cinematics=Auto` when Gameplay is disabled.
- Preserve cinematic aspect-store and cinematic FOV hooks and their policies.

## Explicit non-goals

- Do not rewrite `ReplayManualTransition`, `SafeRead` or hook infrastructure.
- Do not derive aspect from `XMM0`.
- Do not add throttling, sampling or a replacement observer.
- Do not change Gameplay correction, cinematic ENTER/EXIT, Auto viewport
  resolution, dialogue behavior or configuration defaults.
- Do not run the full Batch 4 regression in this batch.
- Do not modify `helper.hpp`.

## Expected files or areas

- `src/plugin/runtime.cpp`: bounded camera-writer hook installation condition
  and associated status/log wording only if required.
- Observer design and safety reports.
- `backlog/TASKLOG.md`.

## Implementation batches

### D1 — Installation-condition change

Guard the shared gameplay camera-writer hook so it is required for Gameplay
only. Leave cinematic hooks and their initialization paths unchanged.

### D2 — Static and build validation

Verify the resulting control flow for Gameplay disabled/enabled configurations,
run relevant harnesses, build `STALKER2CameraTweaks.asi` with `build.cmd`, and
run `git diff --check`.

### D3 — Targeted runtime validation handoff

Prepare the rebuilt artifact and exact test conditions for one user-run game
check. Do not launch the game from the agent. Runtime validation is performed
by the user and must check native AspectRatio interaction plus one Auto
cinematic after a clean restart.

## Risks and rollback / safe-failure behavior

- Risk: a native aspect-setting path may rely on the camera-writer hook for an
  unrelated side effect; targeted validation is required before claiming no
  regression.
- If the build or static control-flow review reveals an unintended change,
  stop and revert only this bounded condition change; do not broaden scope.
- If targeted runtime validation regresses native aspect settings or Auto
  cinematics, restore the previous installation condition and classify Design D
  as rejected for now.

## Stop conditions and phase gates

- Stop if implementing the condition requires changes to hook infrastructure,
  resolver semantics or feature policy.
- Stop after D2 until the user performs the targeted runtime check.
- Do not claim stutter causality from a clean run; the batch validates removal
  safety and preserved behavior only.
- Do not resume full Batch 4 until this targeted result is recorded.

## Expected final Git review

Inspect status and relevant diff paths, compare them with this plan, run the
approved validation, and report completed, remaining, deferred, blocked and
not-runtime-validated items. Do not stage, commit or publish.
