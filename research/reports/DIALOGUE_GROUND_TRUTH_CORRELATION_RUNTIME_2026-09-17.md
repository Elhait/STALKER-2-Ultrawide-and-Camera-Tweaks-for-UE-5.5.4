# Dialogue ground-truth correlation runtime — 2026-09-17

## Inputs and configuration

The two user-provided logs were correlated by their independent timestamps:

- `UE4SS.log` — UE4SS oracle and `DialogueGroundTruthCorrelation` output.
- `STALKER2CameraTweaks.log` — diagnostic ASI DialogueBoundary samples.

ASI configuration at startup:

```ini
Gameplay.Enabled=true
Gameplay.Mode=HorPlus
Cinematics.AspectRatio=Auto
Dialogue.Zoom=Disabled
```

The UE4SS logger loaded at `14:02:03.535` and emitted edge-only state changes.

## Ground-truth oracle

Observed `IsInStaticDialog` edges:

```text
14:03:55.334  true
14:03:58.200  false
14:03:59.451  true
14:04:01.787  false
14:04:02.873  true
14:04:04.451  false
14:04:04.823  true
14:04:10.917  false
14:04:11.541  true
14:04:13.124  false
14:04:14.235  true
14:04:18.272  false
```

This confirms six observed static-dialogue intervals in this run. It does not
establish coverage of every dialogue implementation in the game.

## Correlation findings

### Post-cinematic false-positive exposure

The cinematic exited at `14:03:11.546` and the coordinator immediately became
`Gameplay`. The first DialogueBoundary sample followed at `14:03:11.551`:

```text
IsInStaticDialog: false (oracle remained false until 14:03:55.334)
RSI+0x2C:         90
XMM6:             90
phase:            Inactive -> Candidate
coordinator:      Gameplay
policy:           Disabled
```

This is direct evidence that the existing classifier creates a Dialogue
`Candidate` during a non-dialogue post-cinematic interval. It is not evidence
that the game-owned dialogue state was active.

### Real dialogue interval

Immediately before the first oracle `true` edge, the ASI observed:

```text
14:03:55.227  RSI+0x2C=70  XMM6=89.9766  phase=Candidate
14:03:55.227  classifier -> Active
14:03:55.334  oracle -> IsInStaticDialog=true
```

During the corresponding true interval, `[RSI+0x2C]=70` remained stable while
the incoming FOV descended from approximately `90` toward `70`. At the oracle
false edge (`14:03:58.200`), the ASI was already returning to a `90`-valued
context and `phase=Candidate` around `14:03:58.135`.

The later true intervals show the same broad pattern: dialogue-associated
samples expose `[RSI+0x2C]=70`, while the sampled post-dialogue/recovery and
candidate contexts expose `[RSI+0x2C]=90`. This makes `[RSI+0x2C]` a strong
practical discriminator candidate, not yet a proven universal discriminator.

### Receiver/source context

The diagnostic hook consistently reported a readable receiver and the same
receiver vtable value during the run. The `RSI` source/context pointer changed
between camera/FOV lifecycles, and `[RSI+0x2C]` changed with those contexts.
The current evidence does not establish a semantic meaning for the receiver,
vtable or source pointer themselves.

### Dialogue policy hotkey behavior

The log shows F10 changes while the oracle-driven dialogue intervals are being
observed:

```text
14:03:58.918  Dialogue.Zoom=Native
14:04:02.278  Dialogue.Zoom=Adaptive
14:04:05.093  Dialogue.Zoom=Reduced
14:04:13.716  Dialogue.Zoom=Disabled
```

The `14:04:05.093` change occurs while the oracle remains `true` from
`14:04:04.823` through `14:04:10.917`; subsequent ASI samples report
`policy=Reduced` during the still-active/exiting lifecycle. This confirms that
the current implementation reads the mutable selected policy during an active
lifecycle. It does not implement the documented `next dialogue` snapshot
semantics.

## Status

```yaml
IsInStaticDialog ground truth:
  tested static-dialogue edges: CONFIRMED
  all dialogue types: NOT ESTABLISHED

Post-cinematic generic Candidate:
  confirmed while oracle=false: YES

[RSI+0x2C]:
  value 70 aligned with observed oracle=true intervals: YES
  value 90 aligned with observed false candidate/recovery: YES
  universal discriminator: NOT ESTABLISHED

Receiver/vtable/source identity:
  observable: YES
  semantic ownership: NOT ESTABLISHED

F10 active-policy mutation:
  observed during oracle=true interval: YES

Production repair:
  NOT PERFORMED
```

## Conclusion

The existing FOV heuristic is not a reliable positive Dialogue ownership test:
it creates a Candidate while `IsInStaticDialog=false`. The synchronized run
promotes `[RSI+0x2C]` to a strong practical candidate because its observed
values align with the game-owned oracle across the sampled intervals, but one
run does not prove it for every non-dialogue FOV transition or dialogue type.

No production classifier change is authorized by this evidence alone. The
direct `IsInStaticDialog()` standalone access path remains deferred. The
separate hotkey finding is confirmed and should be handled by a bounded repair:
snapshot the selected Dialogue policy at a confirmed dialogue ENTER and use
that snapshot until the lifecycle exits.

## Limits

- No production source or configuration was changed for this runtime analysis.
- The game was user-run; Codex did not launch it.
- Evidence is limited to the supplied logs and the tested static-dialogue path.
