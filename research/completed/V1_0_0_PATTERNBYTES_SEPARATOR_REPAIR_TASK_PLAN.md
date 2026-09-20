# V1.0.0 PatternBytes Separator Repair Task Plan

## Objective

Repair the confirmed `PatternBytes` separator-to-wildcard no-progress defect
that causes `CinematicEnter` parsing to grow without bound and throw
`std::bad_alloc`.

## Established evidence and current state

- Runtime evidence shows `CinematicEnter` parsing reaches offset 11, a space
  immediately before `??`.
- `strtoul` performs no conversion, the cursor does not advance, and repeated
  `0x00` values grow the parser vector until allocation fails.
- The scanner and resolver are otherwise not implicated in this failure.

## Approved scope

- Skip token separators before wildcard or hexadecimal parsing.
- Preserve current hex and wildcard representations and scanner behavior.
- Guarantee parser forward progress or bounded rejection for malformed input.
- Add parser/scanner harness coverage for mixed hex/wildcard and malformed
  inputs, including all current production string patterns.
- Remove temporary diagnostics used only for this investigation.

## Explicit non-goals

- Do not change production patterns, scan ranges, section selection,
  accumulation, resolver validation, hook ordering or cinematic semantics.
- Do not repair the Cinematics Aspect/FOV failure-domain boundary.
- Do not launch the game.

## Expected files or areas

- `src/hooks/signature_scanner.cpp`
- `src/plugin/runtime.cpp` (temporary diagnostic logging cleanup only)
- `tests/platform/signature_scanner_harness.cpp`
- `test.cmd`
- `backlog/TASKLOG.md`

## Validation

- Parser/scanner harness, including malformed-input termination.
- Existing WorkerLifecycle, FeatureStatus, ConfigPersistence and memory
  harnesses through `test.cmd`.
- Production `build.cmd`.
- `git diff --check`.

## Risks and safe failure

The repair must preserve valid pattern output exactly. Malformed input must
terminate without unbounded allocation. If validation fails, retain the
existing source changes for review and do not perform runtime deployment.

## Stop condition

Stop after validation and read-only Git review. Runtime validation is a
separate user-controlled step.

## Final review

Confirm that the diff contains only the parser repair, bounded harness
coverage, diagnostic cleanup and task records; the Cinematics semantic
failure-domain finding remains open and untouched.
