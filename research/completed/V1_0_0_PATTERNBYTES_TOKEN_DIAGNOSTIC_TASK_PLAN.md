# V1.0.0 PatternBytes Token Diagnostic Task Plan

## Objective

Localize the second `PatternBytes(CinematicEnter)` failure after the parser
cursor repair, using bounded token and cursor diagnostics only.

## Established evidence and current state

- The current diagnostic build completes `FindAll` for earlier patterns,
  including section scanning and one result.
- `CinematicEnter` reaches `FINDALL 00` but fails with `std::bad_alloc`
  before `PatternBytes` returns.
- No parser repair is authorized in this batch.

## Approved scope

- Log the actual `CinematicEnter` input and length.
- Log bounded parser token offsets, token values/wildcards and cursor
  progression.
- Log no-progress state and bounded remaining input when detected.
- Distinguish parser/input, `bytes.push_back`, diagnostic logging or other
  allocation stages where possible, preserving exception behavior.

## Explicit non-goals

- Do not change `strtoul` semantics, cursor progression or wildcard handling.
- Do not change signatures, scanner bounds, accumulation, resolver behavior,
  cinematic logic or failure domains.
- Do not run the game.

## Expected files or areas

- `src/hooks/signature_scanner.cpp`
- `backlog/TASKLOG.md`

## Validation

- `build.cmd`.
- `git diff --check`.
- No harness or game run by the agent.

## Risks and safe failure

Diagnostics must be bounded and must not alter the parser control flow. Any
exception is rethrown through the existing failure path. If logging itself
allocates, it must not be mistaken for a parser repair or semantic result.

## Stop condition

Stop after build, diff check and read-only diff review. Runtime execution is
performed separately by the user.

## Final review

Confirm that only diagnostic instrumentation and task records changed, with
no scanner or cinematic behavior change.
