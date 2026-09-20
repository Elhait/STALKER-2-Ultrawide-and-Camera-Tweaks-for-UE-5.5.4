# Architecture A1 — Runtime Validation Matrix

This matrix is an acceptance checklist for the next game session. It records
runtime questions introduced by A1; it is not a claim that they have already
been tested.

| Scenario | Expected result |
|---|---|
| Full configuration, Gameplay + Cinematic + non-Native Dialogue | All requested features available; normal Dialogue and post-cinematic recovery continue. |
| Cinematic presentation policy Native, FovMode Native, hotkeys off, non-Native Dialogue, Gameplay enabled | Cinematic FOV lifecycle observation is installed without presentation intervention; Dialogue is available. |
| Gameplay disabled, non-Native Dialogue | Dialogue reports unavailable/fails closed; no partial Dialogue lifecycle hook is active. Native cinematic behavior remains intact. |
| Gameplay resolver failure, non-Native Dialogue | Gameplay reports failed; Dialogue fails closed; Cinematic remains independently reported. |
| Cinematic lifecycle resolver failure, non-Native Dialogue | Cinematic lifecycle observation is unavailable; Dialogue fails closed; Gameplay remains independently reported. |
| Native Dialogue with missing lifecycle dependencies | Native Dialogue remains available/bypassed as configured; no non-Native dependency is invented. |
| F10 transition toward non-Native Dialogue without capability | Transition is rejected and the current policy remains unchanged. |
| Cinematic EXIT followed by Gameplay recovery with full capability | Exclusion is armed at EXIT and released by the existing Gameplay recovery observer. |

For each applicable scenario, capture initialization summary and feature-status
lines. Do not infer runtime hook success from status alone without the normal
resolver/install evidence.
