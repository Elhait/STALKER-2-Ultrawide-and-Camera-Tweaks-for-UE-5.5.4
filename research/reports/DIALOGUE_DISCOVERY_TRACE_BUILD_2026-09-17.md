# Dialogue discovery trace diagnostic build — 2026-09-17

## Result

Built a separate diagnostic ASI for a bounded F12-triggered discovery trace.
The production classifier, gameplay modes, Cinematics, Dialogue math and
configuration semantics were not changed.

## Diagnostic artifact

```text
Path: STALKER2CameraTweaks_DialogueDiscovery.asi
SHA-256: 3C825DDDF4FBCF81383658BB98B794C0A929B3FFCDBD5C4F3C71F609B4C1C2F0
Size: 1,142,272 bytes
```

## Runtime behavior prepared

- F12 rising edge starts one session for 10,000 ms.
- F12 during an active session is ignored.
- A worker expires the session and writes one `DIALOG_DISCOVERY END` summary.
- The trace is inactive by default and does not depend on the user's normal
  hotkey setting.
- The existing validated DialogueBoundary hook is reused; no new resolver or
  hook is installed.
- The callback does not modify XMM registers, aspect/FOV fields, flags,
  coordinator state, replay state or Dialogue state.
- Failed safe reads leave the affected field unavailable and continue the
  session; no pointer-chain walk is performed.

## Instrumented fields

For each valid `RSI` context during the active window:

- `RSI/context`, `RCX/receiver`, `RDX`;
- receiver vtable safe-read;
- `XMM6`, `XMM1`;
- `[RSI+0x28]`, `[RSI+0x2C]`, `[RSI+0x30]`;
- raw DWORD snapshot of `[RSI+0x00..0x80]` at 4-byte granularity;
- coordinator, replay state, current Dialogue phase and selected Dialogue
  policy.

Only changed snapshots are logged. Continuous float values use a 0.01
threshold; discrete raw DWORD changes are compared exactly. A context-pointer
change creates a new baseline and is logged separately.

## Intentionally omitted

- `FUN_140D08FE8(param_1 - 0x28)` and related object fields `+0x2CE8`,
  `+0x2CEC`, `+0x2CF0`: omitted because the ABI/call safety is not validated;
  no unknown native function is called.
- Validated executable caller RVA: unavailable at the current mid-hook and
  not substituted with an unreliable stack read.
- Active Dialogue policy snapshot: no production active-policy semantics were
  created solely for discovery telemetry.
- Arbitrary memory outside `RSI+0x80`: omitted to keep the trace bounded and
  fail-closed.

## Validation

- Diagnostic build: PASS.
- MSVC emitted only existing non-fatal SafetyHook/Zydis anonymous-struct
  warnings.
- F12 start/expiry and trace scope: statically verified by source inspection.
- Production classifier and normal path: not changed by the diagnostic macro.
- `git diff --check`: PASS (line-ending warnings only).
- Game launch/runtime test: NOT RUN; user owns the runtime trace.

## Runtime instructions

Use the separate diagnostic ASI, then:

1. Reach ordinary gameplay and press F12 once.
2. Enter the real static dialogue during the next ten seconds and remain in it
   for several seconds.
3. Compare `DIALOG_DISCOVERY` timestamps with the UE4SS
   `IsInStaticDialog()` oracle.

This build discovers correlations only. No observed field is automatically a
Dialogue discriminator.
