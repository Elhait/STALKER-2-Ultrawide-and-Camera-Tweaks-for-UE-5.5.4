# Supported Build Manifest

## Current runtime target

- Game: S.T.A.L.K.E.R. 2: Heart of Chornobyl
- Engine: UE 5.5.4
- Runtime-validated game build: Steam 2.0.5
- Game image SHA-256 observed in the 2.0.5 evidence: `E7B481A97C02D80581FAB0BECE940214A88EBE30211088A00129845A039F9293`
- SHA role: identity and support evidence, not a production allowlist gate.

An unknown executable hash is not accepted as proof of compatibility.
Production compatibility gating requires a unique resolver match followed by
structural byte validation and, where applicable, decoded
instruction/operand validation. A changed image must still go through the
update workflow and runtime regression before support is claimed.

## Production resolver inventory

| Feature/role | Signature and implementation location | Uniqueness and validation | Relevant contract |
| --- | --- | --- | --- |
| Gameplay camera writer | `signatures::CameraWriter`; `gameplay::ResolveCameraWriter` in `src/gameplay/gameplay_camera.cpp` | Exactly one complete `.text` signature; Zydis decode validates `MOVSS [RBX+0x30], XMM0` | Callback contract: `RSI` is the source object and `RBX` is the output/target object; source fields use aspect `0x254` and flags `0x259`; writer input is `XMM0` |
| Cinematic aspect store | `signatures::CinematicAspectSetter`; `cinematics::ResolveAspectStore` called by `runtime.cpp` | `FindAll` must return one; store prefix and expected immediate are checked with byte comparisons; Zydis validates `MOV [RAX+0x254], imm` | Target aspect field `0x254`; expected original immediate `0x3FE38E39` |
| Cinematic ENTER FOV boundary | `signatures::CinematicEnter`; `ResolveCinematicFovCallsites` in `src/plugin/runtime.cpp` | Exactly one match; Zydis validates `MOVSS XMM0,[RIP+disp]`, a relative call and `EnterVcallPair` bytes | ENTER callsite and shared consumer target are validated |
| Cinematic EXIT FOV boundary (legacy) | `signatures::CinematicExit`; `ResolveCinematicFovCallsites` in `src/plugin/runtime.cpp` | Used first; exactly one match; Zydis validates `MOVSS XMM0,[RDI+0x38]`, a relative call and `ExitVcallPair` bytes | EXIT FOV sample at `[RDI+0x38]` |
| Cinematic EXIT FOV boundary (indexed fallback) | `signatures::CinematicExitIndexed`; `ResolveCinematicFovCallsites` in `src/plugin/runtime.cpp` | Used only when the legacy EXIT signature has no matches; exactly one match; Zydis validates `MOVSS XMM0,[RBX+RAX*4+0x38]`, a relative call and `ExitVcallPair` bytes | Indexed EXIT FOV sample at `[RBX+RAX*4+0x38]` |
| Dialogue boundary | `signatures::DialogueBoundary`; `InstallDialogueBoundary` in `src/plugin/runtime.cpp` | Exactly one match; validation is performed at the actual match `+9` hook location with a concrete six-byte `memcmp`. No Zydis decode is claimed. | Callback reads input FOV/value from `XMM6` and writes the transformed result to `XMM1`; boundary bytes are `FF 90 08 06 00 00` |

ENTER and EXIT call targets must resolve to the same executable target and be
executable. The indexed EXIT form is a fallback topology, not an additional
simultaneous hook. Relative displacements are intentionally wildcarded; fixed
addresses and raw RVAs are not portable evidence.

Signatures are owned by `src/hooks/signatures/`. The cinematic FOV and dialogue
resolver/control-flow integration remains in `src/plugin/runtime.cpp`, while
the reusable cinematic aspect resolver is in `src/cinematics/`.

## Callback and concurrency assumptions

- Gameplay camera callbacks receive the validated camera writer context:
  `RSI` is the source object, `RBX` is the output/target object, and the
  writer input is the `XMM0` value used by the validated `MOVSS` instruction.
  The relevant source/target state uses aspect `0x254` and flags `0x259`.
- Cinematic aspect application validates the target object and writable range
  before writing the owned aspect field.
- Dialogue callbacks read the incoming value from `XMM6` and write a valid
  transformed result to `XMM1`; the hook-site validation remains unique-match
  plus concrete byte validation rather than a decoded Zydis contract.
- Ordinary callback telemetry is thread-confined to the runtime/game owner
  thread. Atomic fields carry cross-thread observations where required.
- Worker lifecycle start/stop ownership belongs to one externally serialized
  Runtime control path; it need not be one physical thread. Workers wait on
  the Runtime-owned stop event and do not own joins.
- `DLL_PROCESS_DETACH` only signals stop; normal dynamic unload is unsupported.

## Update and regression evidence

For a new game build, re-establish executable identity, resolver uniqueness,
structural byte and applicable decoded-instruction validation, callback/register
contracts and field offsets, then complete the required runtime regression
before claiming support. Store static evidence in `research/reports/` and
runtime logs/evidence under the corresponding research record. Runtime support
is not extended to older or newer game builds without explicit executable
identity and regression evidence.
