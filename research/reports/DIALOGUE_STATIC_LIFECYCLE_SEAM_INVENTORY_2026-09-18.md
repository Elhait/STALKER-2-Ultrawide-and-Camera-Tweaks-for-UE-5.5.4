# Dialogue static-lifecycle seam inventory — Steam 2.0.5

Date: 2026-09-18  
Scope: read-only comparison of production ownership detection and the current UE4SS `CXXHeaderDump`; no production changes, build, ASI or game launch.

## Identity and evidence boundary

The executable identity used by the existing static evidence is Steam 2.0.5:

| Field | Observed |
| --- | --- |
| SHA-256 | `E7B481A97C02D80581FAB0BECE940214A88EBE30211088A00129845A039F9293` |
| Image base | `0x140000000` |
| `.text` size | `0x7CCD000` |

The current CXX dump is treated as reflected declaration inventory, not as proof of native implementation addresses or ABI. Runtime oracle evidence is limited to the tested static-dialogue path.

## Executive result

The production Cinematic and Dialogue paths are architecturally different:

```text
Cinematics:
  dedicated ENTER/EXIT callsites
  → structural resolver validation
  → explicit coordinator ownership transitions

Dialogue:
  generic FOV-blend callback
  → Gameplay + non-Native policy + descending FOV
  → inferred Candidate/Active state
```

The dump contains a useful set of dialogue-related declarations, but no declaration by itself establishes a safe standalone ASI hook. The highest-value next target is not another FOV field. It is an executable lifecycle seam that creates or clears the state queried by `APC::IsInStaticDialog()`.

## 1. Production ownership comparison

### Cinematic — established positive boundary

`src/plugin/runtime.cpp` contains separate `TraceCinematicEnter()` and `TraceCinematicExit()` callbacks. Their resolver requires unique matches, decoded FOV operands, valid relative calls, surrounding virtual-call topology and a shared executable consumer target. The callbacks explicitly move the mod coordinator through `CinematicActive`, `CinematicExiting` or `Gameplay` according to the existing recovery contract.

This is a lifecycle-associated boundary. It does not prove a named engine boolean API, but it is a positive ownership boundary strong enough for the current production feature.

### Dialogue — established observation boundary, not ownership

`InstallDialogueBoundary()` resolves one generic FOV-blend signature and installs the callback at the validated instruction offset. `TraceDialogueBoundary()` then infers lifecycle from:

```text
valid incoming FOV
+ coordinator == Gameplay
+ policy != Native
+ descending FOV
→ Candidate / Active
```

The callback has no established positive game-owned Dialogue signal. Ascending FOV and baseline recovery are used to infer exit. UE4SS ground truth has already confirmed false positives outside real static dialogue.

`[RSI+0x2C]` is the FOV target/end value, not an ownership field. `ReplayManualTransitionOriginal()` invalidates stale state after a source/FOV discontinuity; it is a safeguard, not a positive detector.

## 2. CXXHeaderDump inventory

### A. `APC::IsInStaticDialog()` — direct state oracle

**Location:** `CXXHeaderDump/Stalker2.hpp`, `APC` declaration around line 6447.

```cpp
bool IsInStaticDialog();
```

**Semantics:** direct game-owned state query candidate. UE4SS runtime already observed `false → true → false` on the tested static-dialogue path.

**Classification:** positive ownership signal candidate; not a lifecycle hook by itself.

**Standalone status:** not established. Existing static evidence found only an isolated reflected name record and no native implementation, caller, field offset, APC acquisition path, ABI or ProcessEvent bridge.

**Use:** highest-value validation oracle and future seam target. Do not infer an ASI call or field read from the declaration alone.

### B. `UDialogManager`

**Location:** `Stalker2.hpp`, around line 10974.

```cpp
class UDialogManager : public UBaseTickableManager
{
};
```

**Semantics:** class name and manager base make it the strongest ownership-domain candidate in the dump. The generated declaration exposes no enter/exit methods or state fields, so the dump does not identify the transition function.

**Classification:** lifecycle manager candidate; executable callsite still required.

**Standalone status:** not established.

**Next evidence required:** native references to manager tick/dispatch or a callsite that changes the state observed by `APC::IsInStaticDialog()`.

### C. `UCustomConsoleManagerAA::XPlayDialog*` / `XRestartCurrentDialog`

**Location:** `Stalker2.hpp`, around lines 10030–10055.

Relevant declarations include:

```cpp
void XRestartCurrentDialog();
void XPlayDialogLine(int32 SpeakerUID, FString DialogPrototypeSID);
void XPlayDialogFromPool(FString DialogEventTypeName, int32 NPCUIDValue1, int32 NPCUIDValue2);
void XPlayCommentFromPool(FString DialogEventTypeName, int32 NPCUIDValue);
void XClearDialogQueue();
```

**Semantics:** explicit dialog command/dispatch surface. The `X` prefix and owning console manager indicate developer/debug or command routing rather than the authoritative runtime ownership transition.

**Classification:** useful executable search anchor, but not a production hook target until its downstream state transition is proven.

**Standalone status:** not established.

### D. `BP_Speaker_C` dialog signal receiver

**Location:** `CXXHeaderDump/BP_Speaker.hpp`, fields around `0x0358–0x0360` and bound event declaration.

```cpp
USignalSenderComponent* SenderPlayDialog;
USignalReceiverComponent* ReceiverPlayDialog;
void BndEvt__BP_Speaker_ReceiverPlayDialog_...(...);
```

**Semantics:** Blueprint signal path for a speaker object. It may initiate an audio/dialog event, but it is not evidence of the player static-dialogue ownership state and may cover only environmental speaker behavior.

**Classification:** low-confidence upstream event candidate; not a general Dialogue seam.

**Standalone status:** not established.

### E. `FAnimPlayerDialogData.bInDialog`

**Location:** `Stalker2.hpp`, around line 627.

```cpp
bool bInDialog;
```

**Semantics:** player animation/dialog data. This may be downstream state used after dialogue ownership has already been established.

**Classification:** state mirror/corroborating signal, not preferred ownership seam.

**Standalone status:** no owning player object path or update writer established.

### F. `FAnimHumanDialogData.bInDialog` and `AnimBP_Human::IsInDialog`

**Locations:** `Stalker2.hpp` around lines 278 and 8023; `AnimBP_Human.hpp` around `IsInDialog` at `0xBC21`.

**Semantics:** human/NPC animation state. These fields can be downstream, NPC-specific or unavailable for dialogue variants that do not use the same animation graph.

**Classification:** corroborating animation state only; not a universal Dialogue ownership contract.

**Standalone status:** not established.

### G. `UDialogProtectionManager` / `ADialogProtector`

**Locations:** `Stalker2.hpp` around lines 5276 and 10986.

**Semantics:** protection/interaction support around dialogue objects. No enter/exit ownership API or player-state field is exposed in the dump.

**Classification:** secondary support candidate; low priority for camera ownership.

**Standalone status:** not established.

### H. `UDialogFunctionLibrary`, asset/data subsystems, UI and subtitle classes

**Locations:** `Stalker2.hpp` around lines 10932–11070, 9403–9404 and related declarations.

**Semantics:** asset lookup, runtime data, widgets, subtitle events and UI. These are not evidence of the player camera's Dialogue ownership state.

**Classification:** exclude from primary ownership search unless a later executable trace proves a direct lifecycle call.

### I. Engine `PlayDialogue*` functions

**Semantics:** audio playback APIs such as `PlayDialogue2D` and `SpawnDialogueAttached`. They identify audio playback, not necessarily static camera/dialogue ownership.

**Classification:** exclude as primary camera ownership seams.

## 3. Ranked next search targets

| Rank | Target class | Why | Current status |
| --- | --- | --- | --- |
| 1 | Native setter/transition surrounding `APC::IsInStaticDialog()` state | Directly matches the game-owned oracle and could provide both ENTER and EXIT | Not established; requires executable callsite evidence |
| 2 | `UDialogManager` native dispatch/tick path | Most plausible ownership-domain manager in the dump | Candidate only; no exposed lifecycle method |
| 3 | `XPlayDialog*` command path followed downstream to APC state | Explicit dialog-start anchors may lead to the authoritative path | Candidate only; likely command/debug entry rather than final seam |
| 4 | Player/NPC dialog state writer feeding `FAnim*DialogData` | Could provide a corroborating edge if it is updated from the same owner | Downstream and coverage uncertain |
| 5 | `BP_Speaker` signal receiver | Useful for environmental audio/dialogue events, weak for general static dialogue | Narrow Blueprint-specific candidate |

Do not promote the following to ownership targets without contradictory new evidence: `[RSI+0x2C]`, fixed target values such as `70`, generic FOV descent, subtitle/UI events, audio playback functions or animation booleans alone.

## 4. Minimum next static batch

The next bounded batch, if continued, should search only for executable evidence tied to ranks 1–3:

1. Identify whether the matching 2.0.5 executable contains references to the `APC::IsInStaticDialog` reflected metadata beyond the already-known isolated record.
2. Inspect native references/callers around `UDialogManager` methods or its tick/dispatch vtable entries.
3. Inspect the downstream path of `XPlayDialogLine`, `XPlayDialogFromPool` and `XRestartCurrentDialog` only far enough to determine whether they converge on the state observed by `IsInStaticDialog()`.
4. Stop if no validated executable anchor or convergent state writer is found; do not broaden into arbitrary FOV or UI reverse engineering.

This batch must repeat the executable identity gate before interpreting any new Ghidra result.

## Final classification

```yaml
CONFIRMED:
  - Cinematic has a dedicated validated ENTER/EXIT boundary pair.
  - Dialogue currently uses a generic FOV-blend boundary and heuristic lifecycle.
  - APC::IsInStaticDialog() is a valid runtime oracle for the tested static-dialogue path.
  - FOV target/end values and animation/UI declarations are not ownership proof.

STATICALLY ESTABLISHED:
  - CXXHeaderDump contains the ranked candidate declarations listed above.
  - UDialogManager has no exposed lifecycle methods in the generated declaration.
  - Existing production Dialogue invalidation is a safeguard, not a positive detector.

NOT ESTABLISHED:
  - Native implementation or ABI for IsInStaticDialog().
  - APC field/object path or safe standalone invocation.
  - A production-positive Dialogue lifecycle seam.
  - Coverage of every dialogue type by IsInStaticDialog().

PRODUCTION CHANGES: NONE
ASI BUILD: NONE
RUNTIME VALIDATION: NONE
```
