# MatchGameplay Cinematic Reference Provenance — Task Plan

## Objective

Determine the provenance and variability of the cinematic reference FOV `R`
using existing runtime logs and research reports only.

## Scope

- Inventory existing cinematic ENTER/EXIT evidence.
- Extract per-episode gameplay baseline, authored/native ENTER FOV, transformed
  ENTER FOV, dynamic range, EXIT target, post-EXIT baseline and aspect.
- Classify `R` as fixed, episode-specific or not established.
- Update the MatchGameplay research report with evidence and limitations.

## Non-goals

- No source or production changes.
- No diagnostic implementation.
- No game launch.
- No new executable/Ghidra analysis.
- No assumption that `R=90` is universal.

## Validation and stop conditions

- Use only logs/reports tied to known runtime evidence.
- Separate independent cinematic episodes from repeated lines within one episode.
- If provenance cannot be established, retain `R` as runtime-required.
- Stop after the offline report and `git diff --check`.

## Final review

Confirm that only the task plan and research report changed and that the report
separates mathematical proof from engine-semantic evidence.
