# v1.0.0 Batch 4 Regression — Gameplay Disabled Configuration

## Status

`REGRESSION / OPEN — production fix not yet attempted`

## Scenario

Steam 2.0.5, current v1.0.0 build, with:

```ini
[Gameplay]
Enabled=false
```

The user reports that the gameplay aspect correction still appears to affect
the first session and that the game's aspect-ratio setting no longer responds
as expected. The symptom was initially reported as recoverable after
leaving/re-entering the game, but a subsequent restart reproduced the same
failure again. The run was stopped as a regression branch before any
production change.

## Log evidence

The startup log records:

```text
Configuration: Gameplay.Enabled=0 Cinematic.AspectRatio=Auto Dialogue.Zoom=Reduced
Initialization summary: Gameplay=DISABLED CinematicAspect=AVAILABLE CinematicFOV=AVAILABLE Dialogue=AVAILABLE
```

It also records:

```text
Installing read-only camera observer for cinematic FOV aspect.
Read-only camera observer installed: true.
```

The observer is installed because the current initialization condition is
`gameplayRequested || cinematicRequested`, even when Gameplay is disabled.
The runtime hook is `ReplayManualTransition`; it enters the callback and then
returns early when `g_config.gameplayEnabled` is false, after read-only
observation work.

The current log contains cinematic subsystem initialization messages only. It
contains no `Global cinematic ENTER`, `Global cinematic EXIT`, `Cinematic
aspect store`, `AtomicExitHandoffArmed` or `RecoveryStart` event. Therefore the
reported regression occurred before any cinematic lifecycle event in this run.

## Contract comparison

Expected:

```text
Gameplay.Enabled=false
→ gameplay correction and gameplay writer intervention disabled
→ native gameplay aspect behavior remains responsive
→ independent Cinematics/Dialogue may continue
```

Observed:

```text
Gameplay status reports DISABLED
→ observer hook still installed through cinematic initialization
→ user reports gameplay aspect setting does not respond correctly
```

This is sufficient to stop the Batch 4 closure. It is not yet sufficient to
claim the exact causal write owner: the log proves observer installation, but
not a gameplay write by the observer in this run.

## Static causal audit

The startup path was traced through `ReplayManualTransition` and the cinematic
aspect-store hook:

```text
Gameplay=false
→ ReplayManualTransition callback remains installed for Cinematics=Auto
→ callback reads camera aspect and updates g_lastObservedAspect
→ gameplay original/replay path is not called
→ no gameplay-owned aspect/flags write is reachable from this branch
```

The observer callback is therefore read-only with respect to game memory in
the reproducing configuration. `g_lastObservedAspect` is only consumed by the
cinematic aspect-store application path; it is not itself a gameplay camera
write.

A separate static candidate remains:

```text
Cinematics=Auto
→ global CinematicAspectStore hook installed at startup
→ ApplyCinematicAspectStore has no cinematic-lifecycle guard
→ any invocation of that store hook can write targetObject + aspectOffset
```

This is a concrete unguarded write path, but the supplied reproducing log has
no `Cinematic aspect store:` entry. Therefore the hook is a causal candidate,
not yet a runtime-confirmed owner of this specific regression.

The follow-up menu reproduction also produced no `Cinematic aspect store:`
entry while the user attempted to change `AspectRatio` from the main menu.
The UI emitted selection sounds and requested save-on-exit, but the setting was
only adjustable after re-entering the menu. This rejects the hypothesis that
the cinematic aspect-store callback fired during the observed menu interaction.
The remaining runtime candidates are the camera-writer observer's hook-side
interaction or the game's own settings-apply/re-entry lifecycle.

The unconditional `Gameplay aspect fix loaded. FOV is preserved from the
game's settings.` line is a logging defect/misleading status message only; it
does not prove that the Gameplay feature was enabled or that a gameplay write
occurred.

## Lifecycle refinement

The current reproduction boundary is:

```text
change Gameplay.Enabled → false
restart game
→ first-session aspect-setting failure

leave/re-enter game
→ behavior may recover without source/config changes
```

The repeated restart reproduction confirms a startup or camera-state
initialization/lifecycle interaction, while the earlier recovery after
re-entry shows state sensitivity. This does not prove
whether the observer, cinematic aspect store, native camera initialization or
another startup state owns the symptom.

## Classification

```text
Batch 4 regression                  CONFIRMED by user observation
Gameplay independence contract      VIOLATED in observable behavior
Exact causal implementation path    OPEN
Restart reproduction                CONFIRMED by repeated user observation
Startup/re-entry sensitivity        CONFIRMED by user observation
Cinematic ENTER/EXIT in repro       NOT OBSERVED
Startup-only causal boundary        OPEN; post-cinematic paths excluded for this run
Gameplay observer memory mutation   NOT FOUND statically
Cinematic aspect-store write path   REJECTED for this menu reproduction; no runtime hit
Camera observer hook interaction    OPEN
Native settings apply/re-entry      OPEN
Misleading Gameplay status log      CONFIRMED
Production fix                       NOT STARTED
```

## Required next bounded audit

Audit the `Gameplay.Enabled=false + Cinematics=Auto` startup path before the
first cinematic and determine whether the read-only observer is necessary for
cinematic Auto aspect resolution. The audit must distinguish:

1. the observer installation itself changes native writer behavior despite its
   callback being read-only;
2. the cinematic aspect-store hook is invoked by the native aspect-setting
   path and owns the observed behavior; or
3. the visual symptom is caused by a separate game/configuration state.

Do not investigate cinematic ENTER/EXIT, recovery or post-cinematic handoff as
the cause of this reproduction. Do not change production code or rerun the
complete regression matrix until the startup causal path is identified. Do not
weaken the accepted independence contract.
