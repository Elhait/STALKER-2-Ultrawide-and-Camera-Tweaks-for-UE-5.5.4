# Dialogue ground-truth correlation diagnostic

## Objective

Determine whether the existing standalone `DialogueBoundary` observables can
be correlated with the game-owned `APC::IsInStaticDialog()` state. The result
must distinguish ordinary gameplay false-positives, post-cinematic FOV descent
and a real static dialogue without changing production behavior.

## Established evidence and current state

- UE4SS directly observed `IsInStaticDialog()` edges on the tested static
  dialogue path: `false -> true -> false`.
- The current Dialogue classifier uses broad gameplay/FOV-descent heuristics
  and is known to false-positive outside dialogue.
- The current diagnostic boundary can safely observe `XMM6`, source/context
  registers, `[RSI+0x2C]`, coordinator, phase and policy.
- Standalone access to `IsInStaticDialog()` itself is deferred; UE4SS is the
  ground-truth oracle for this experiment.

## Approved scope

- Reuse the existing diagnostic ASI boundary if its telemetry already contains
  the fields below; otherwise add only diagnostic-only, change-driven fields.
- Use `research/ue4ss/DialogueGroundTruthCorrelation/Scripts/main.lua` as the
  UE4SS Lua edge logger for `IsInStaticDialog()` with UE4SS line timestamps.
- Correlate both logs for one bounded runtime scenario.
- Classify `[RSI+0x2C]` and other existing observables as candidate
  discriminators only if they agree with the oracle across all three contexts.

## Explicit non-goals

- No production Dialogue classifier repair.
- No changes to HorPlus, Cinematics, Gameplay, coordinator lifecycle or FOV
  mathematics.
- No cooldown, timer, polling worker or new hook.
- No guessed APC field, RVA, caller address, ABI or reflection bridge.
- No requirement to cover every dialogue type in this first run.

## Required telemetry

### UE4SS oracle

Log only state edges with a monotonic sequence and timestamp:

```text
DIALOG_ORACLE seq=<N> t=<timestamp> IsInStaticDialog=<false|true>
```

Polling may run at 50–100 ms, but unchanged values must not be logged.

### Existing ASI boundary

For bounded change-driven samples, retain:

```text
timestamp / sequence
XMM6
XMM1 before/after, if already available
RCX / RDX / RSI
receiver/vtable validity and RVA, if already available
safe [RSI+0x2C]
camera source/raw FOV
coordinator
Dialogue phase/policy
```

The observer must record samples independently of whether the current
classifier labels them Candidate or Active. It must not transform values,
reset state, arm replay or alter return registers.

## Runtime scenario

Use the existing diagnostic configuration and one save/load session:

```ini
Gameplay.Enabled=true
Gameplay.Mode=HorPlus
Cinematics.AspectRatio=Auto
Dialogue.Zoom=Reduced
```

Capture in this order:

```text
A. ordinary gameplay; cause one non-dialogue FOV change if convenient
B. enter and exit one cinematic; remain in gameplay afterward
C. start one real static dialogue; hold it briefly; exit it
```

Record the approximate wall-clock time of each manual boundary. Do not change
the Dialogue policy during the run.

## Analysis contract

Partition ASI samples into A/B/C by timestamps and compare them to oracle
edges:

```text
A/B: IsInStaticDialog=false
C active interval: IsInStaticDialog=true
```

For every candidate observable, report:

```yaml
observable:
false_interval_values:
true_interval_values:
transition_alignment:
stable_across_samples: YES/NO
discriminator_candidate: YES/NO
```

A value difference in only one convenient sample is not sufficient. A
candidate must remain distinct across ordinary gameplay, cinematic EXIT and
real dialogue samples, with no unexplained overlap that would preserve the
known false-positive.

## Validation and stop conditions

- Static preflight: confirm diagnostic telemetry is before classifier
  decisions and that all reads are safe/fail-closed.
- Build only a diagnostic ASI if the existing build does not already contain
  the required fields; no production ASI is built for this plan.
- Run `git diff --check` after any diagnostic-only edit.
- Game runtime is user-run; Codex does not launch the game.
- Stop after one run and report `candidate established` or `candidate not
  established`.
- If no candidate separates the oracle intervals, do not repair the heuristic
  and do not add another broad tracer in this batch. Keep direct-state access
  deferred and record the need for a new upstream observation seam.

## Expected outcome

One of:

```yaml
Practical discriminator: ESTABLISHED
  production repair: eligible for a separate bounded implementation plan

Practical discriminator: NOT ESTABLISHED
  production repair: none
  next research: new upstream/native observation seam, separately approved
```

## Final review

Review the actual changed paths, preserve all pre-existing working-tree edits,
and report completed, remaining, deferred, blocked and not-runtime-validated
items. Do not stage or commit.
