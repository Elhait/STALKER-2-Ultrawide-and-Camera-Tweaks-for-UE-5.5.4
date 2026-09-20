# Tests / Harnesses / Validation — Production Contract Coverage Batch

## Result

The lightweight deterministic validation architecture was repaired without
introducing a framework or simulating UE/runtime behavior.

## Unified runner

`test.cmd` now performs a machine-checkable registration audit before building:

```text
runner_sources=31
runner_compiled=31
runner_executed=31
compile_checks=True
run_checks=True
sets_equal=True
```

The missing `coordinator_recovery_harness` compile check and missing
`cinematic_selection_harness` execution were fixed. Every compile and execute
step now has an immediate non-zero failure propagation check.

## Added/expanded coverage

- `cinematic_aspect_harness`: writable/invalid/native fallback, null and
  overflow refusal, zero/one/multiple resolver matches, malformed candidate,
  and truncated candidate.
- `dialogue_fov_harness`: independently recorded dialogue endpoints,
  projection/exit endpoints, monotonicity, out-of-range behavior and NaN/
  infinity handling.
- `instruction_validator_harness`: complete/truncated instructions, exact
  span boundaries, forward/backward rel32 calls, in-span/out-of-span targets,
  invalid call operands and refusal cases.
- `gameplay_camera_safety_harness`: aspect-only writes, all-or-nothing
  aspect/flags writes, overflow and no-partial-write behavior.
- `gameplay_camera_resolver_harness`: zero, unique valid, multiple and
  signature-mismatch resolver cases.
- `memory_harness`: production `IsWritable` coverage for writable,
  executable-writable, read-only, execute-read, no-access, boundary,
  null and overflowed ranges.
- Misleading output/assertion labels were renamed to describe actual evidence.
- Runtime fixtures now carry machine-readable scenario/source metadata; game,
  mod and build hashes remain `UNKNOWN` where the fixture source did not
  establish them.

## Honest limits

The fixed camera-writer signature encodes the tested MOVSS operand bytes, so a
distinct invalid-operand resolver fixture cannot be injected through the
public resolver API without changing the signature contract or extracting a
new seam. Signature mismatch and all reachable resolver refusal paths are
covered; invalid-operand coverage remains a documented partial boundary.

The deterministic suite does not claim proof of SafetyHook callback ordering,
native UE thread affinity, actual executable hook installation, visual
framing, save/load camera recreation, recovery liveness, or performance.

## Validation

- Runner self-audit: PASS.
- `test.cmd`: PASS; all 31 registered harnesses compiled, executed and had
  checked results.
- `build.cmd`: PASS.
- `build-diagnostic.cmd`: PASS.
- `git diff --check`: PASS apart from normal Git line-ending warnings.
- Runtime: NOT_PERFORMED.

## Manifest

```yaml
unified_runner:
  status: FIXED
  harness_sources: 31
  harnesses_compiled: 31
  harnesses_executed: 31
  results_checked: 31
cinematic_aspect_coverage:
  status: ADDED
dialogue_fov_coverage:
  status: ADDED
instruction_rel32_span_coverage:
  status: ADDED
gameplay_write_resolver_coverage:
  status: PARTIAL
is_writable_coverage:
  status: ADDED
runtime_fixture_provenance:
  status: IMPROVED
misleading_test_claims:
  status: FIXED
runtime_only_contracts:
  status: EXPLICITLY_EXCLUDED
test_cmd: PASS
production_build: PASS
diagnostic_build: PASS
diff_check: PASS
runtime: NOT_PERFORMED
```
