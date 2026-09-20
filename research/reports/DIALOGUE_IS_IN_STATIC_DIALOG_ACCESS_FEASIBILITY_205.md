# `APC::IsInStaticDialog()` standalone access feasibility — Steam 2.0.5

## Identity gate

| Field | Expected | Observed | Result |
| --- | --- | --- | --- |
| Executable SHA-256 | `E7B481A97C02D80581FAB0BECE940214A88EBE30211088A00129845A039F9293` | same | PASS |
| Image base | `0x140000000` | `0x140000000` | PASS |
| `.text` size | `0x7CCD000` / `130863104` | same | PASS |
| Known writer anchor | current 2.0.5 writer evidence at RVA `0xA9F0FB` | previously reconciled | PASS |

Only the program whose Ghidra executable SHA matched this identity was
interpreted. The shared Ghidra project also contains older images; the helper
reported `IDENTITY=FAIL` and performed no search for each of them.

## Confirmed

- UE4SS CXXHeaderDump declares `bool APC::IsInStaticDialog();` in
  `Stalker2.hpp`.
- UE4SS runtime observation on the tested static-dialogue path recorded:

  ```text
  02:09:05.381  false
  02:09:06.652  true
  02:09:09.650  false
  ```

  This confirms a game-owned state edge for that tested path. It does not
  establish coverage of every dialogue type.
- The standalone ASI source currently contains camera/FOV SafetyHook paths, but
  no existing UObject/UClass/UFunction/ProcessEvent bridge or validated player
  controller/APC acquisition path.

## Statically established

The matching executable contains one exact ASCII `IsInStaticDialog` record:

```text
string:       VA 0x14943AB5C (.rdata)
direct ref:   VA 0x148D8A020, DATA, no containing function
refs to that metadata location: 0
```

The record is therefore name/metadata evidence only. It has no recovered code
reference, candidate native implementation, or direct caller in the current
Ghidra reference database. The result is reproducible in
`02-Research/evidence/find-is-in-static-dialog-native-path-205-headless.txt`
using the read-only helper and launcher under `02-Research/Ghidra/`.

## Not established

- Whether the declaration is a callable native `UFUNCTION`, and its native
  implementation address.
- Any APC field, component path, or stable offset which stores this state.
- APC acquisition, object lifetime, calling convention, thread requirements or
  a safe standalone ABI for invoking the query.
- A native transition hook for this state.
- A standalone reflection/ProcessEvent route.

The CXX dump supplies a reflected declaration, not an executable entry point or
field layout proof. The isolated `.rdata` name record cannot safely fill those
gaps.

## Access-path classification

| Strategy | Current feasibility | Safety / fail-closed assessment | Why |
| --- | --- | --- | --- |
| Direct validated field read | NOT ESTABLISHED | Unsafe to implement; must remain absent | No field offset, object chain or APC acquisition is known. |
| Resolved native `IsInStaticDialog()` call | NOT ESTABLISHED | Unsafe to implement; must remain absent | No native entry, ABI or call-context contract was recovered. |
| Observe native state transition | NOT ESTABLISHED | No existing hook point | Existing Dialogue hook observes a broader FOV path, not the game-owned state. A new transition anchor would require separate evidence. |
| Minimal UE reflection / `ProcessEvent` access | NOT ESTABLISHED | No standalone bridge | UE4SS can call the reflected method, but the ASI has no validated UE reflection or player-object bridge. |

## Recommendation and stop condition

There is **no production-safe standalone ASI access path established in this
audit**. Do not replace the Dialogue classifier with a guessed field read,
native call or reflection invocation.

The useful result remains: `IsInStaticDialog()` is a high-quality game-owned
signal for the tested static-dialogue path. If promoted later, the next bounded
research task must establish one concrete native access seam first—for example a
validated registration/object path or a distinct state-transition anchor—before
any classifier design. It must not infer that seam from the FOV heuristic or
from the isolated name string.

## Limits

- No production source, config, resolver, hook or classifier changed.
- No ASI was built and the game was not launched.
- This result does not reject future direct-state access; it rejects only the
  unproven access strategies above for the current Steam 2.0.5 evidence.
