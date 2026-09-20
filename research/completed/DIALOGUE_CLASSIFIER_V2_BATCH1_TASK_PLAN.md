# DialogueClassifierV2 Batch 1 — task plan

## Objective

Implement only the confirmed post-cinematic Dialogue false-positive exclusion. Prevent native post-cinematic FOV recovery from creating Dialogue `Candidate`/`Active` state while preserving HorPlus seamless EXIT and the validated AspectRecalculation choreography.

## Established evidence and current state

- `APC::IsInStaticDialog()` ground truth confirmed the current FOV classifier creates false positives outside real Dialogue.
- In HorPlus, Cinematic EXIT returns the coordinator to `Gameplay` immediately while native recovery samples continue through the validated gameplay writer.
- `TraceCinematicExit()` captures the native EXIT target FOV and existing `kRecoveryEpsilon` is already used for recovery convergence.
- The same gameplay writer is observable in HorPlus and AspectRecalculation.
- Existing `g_postExitTraceArmed` is diagnostic-only and is not used as production lifecycle state.

## Approved scope

- Add minimal explicit production state for post-cinematic Dialogue exclusion.
- Arm it from existing Cinematic EXIT.
- Observe/release it at the existing validated gameplay writer using source identity, finite FOV and the existing target/convergence epsilon.
- Make `TraceDialogueBoundary()` pass native FOV and keep Dialogue phase `Inactive` while exclusion is active.
- Add deterministic harness/unit coverage for the exclusion predicate/state transitions.
- Run relevant static/harness validation and read-only Git review.

## Explicit non-goals

- No Batch 2 Candidate hardening.
- No Batch 3 selected/active policy snapshot.
- No new hook, offset, timer, frame counter or hard-coded FOV target.
- No changes to HorPlus math, cinematic transforms, coordinator timing or AspectRecalculation replay.
- No changes to F9/F10 behavior.
- No game launch or runtime ASI validation in this batch.

## Expected files/areas

- `src/plugin/runtime.cpp` for the bounded lifecycle state and guards.
- Existing dialogue/gameplay test or harness locations, only if a production predicate can be tested without duplicating logic.

## Implementation batches

### Batch 1A — state and lifecycle observation

- Arm exclusion on Cinematic EXIT.
- Clear/reinitialize it on Cinematic ENTER and new EXIT.
- Store expected source lazily on the first recovery writer sample.
- Release on finite same-source recovery convergence against the captured EXIT target using `kRecoveryEpsilon`.
- Cancel safely on invalid sample or source replacement so exclusion cannot remain permanently armed.

### Batch 1B — Dialogue boundary guard

- While exclusion is active, reset Dialogue runtime state, leave the incoming FOV unchanged and return.
- Do not alter normal Candidate/Active/Exiting logic outside the exclusion.

### Batch 1C — deterministic validation

- Add/extend harness coverage for arm, suppression, repeated samples, convergence release, post-release eligibility, ENTER reset, invalid sample and source replacement.
- Run `git diff --check` and relevant tests only.

## Validation

- Static source review of both HorPlus and AspectRecalculation writer paths.
- Unit/harness tests for the exact production state/predicate where feasible.
- Existing gameplay, cinematic and Dialogue tests relevant to the touched state.
- No build or runtime launch unless explicitly approved later; build correctness is not runtime behavior proof.

## Risks and rollback/safe failure

- If source identity, target capture or convergence cannot be expressed deterministically, stop before implementation.
- If a recovery sample is invalid or the source changes, cancel the exclusion and reset Dialogue state; never keep an unobservable exclusion armed indefinitely.
- Preserve native pass-through on all ambiguous paths.
- Rollback is limited to the new Batch 1 state/guard; existing gameplay/cinematic logic remains untouched.

## Stop conditions and phase gates

- Stop if implementation requires a timer, new memory offset, changed coordinator choreography or modified FOV math.
- Stop after Batch 1 validation; do not start Batch 2/3 automatically.

## Expected final Git review

Review changed paths against this plan, explicitly confirm HorPlus, AspectRecalculation, cinematic transforms, Dialogue phase rules outside exclusion and F9/F10 are unchanged, then archive this plan under `research/completed/` only after validation.
