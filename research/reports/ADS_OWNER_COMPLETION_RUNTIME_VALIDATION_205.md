# ADS Owner Completion Runtime Validation — Steam 2.0.5

## Scope

Validate the bounded ADS-owner completion repair against the supplied runtime log. No HorPlus, AspectRecalculation, cinematic, or positive Dialogue-classifier changes were included in this batch.

## Evidence

- Game image hash: `E7B481A97C02D80581FAB0BECE940214A88EBE30211088A00129845A039F9293`.
- Candidate ASI recorded 94 `WIDEBOY_ADS` samples.
- The final observed OUT endpoint before Dialogue was `0.0069866 / 0.993013`, which is inside the repaired completion tolerance.
- `EARLY_CB DIALOGUE RETURN 6`: `0`.
- `EARLY_CB DIALOGUE RETURN 5`: `7` during cinematic EXIT exclusion.
- `EARLY_CB DIALOGUE RETURN 4`: `396` during Dialogue lifecycle.
- `Dialogue candidate captured`: `7`.
- `Dialogue lifecycle started`: `5`.
- One cinematic ENTER and one cinematic EXIT were observed.

## Result

The previous stale-owner failure is not reproduced. After the final ADS OUT sample, the ADS owner releases sufficiently for real Dialogue processing. Dialogue candidates and lifecycle starts are observed again, while ADS ownership no longer suppresses them with `RETURN 6`.

## Validation limits

This is runtime validation of the tested Steam 2.0.5 scenario only. It does not establish coverage for every weapon, optic, dialogue type, or executable version.

## Status

`ADS owner completion repair: PASS for tested scenario.`

Stable `STALKER2CameraTweaks.asi` was not replaced. The validated artifact is the diagnostic candidate `STALKER2CameraTweaks_WideboyAdsDiagnostic.asi`.
