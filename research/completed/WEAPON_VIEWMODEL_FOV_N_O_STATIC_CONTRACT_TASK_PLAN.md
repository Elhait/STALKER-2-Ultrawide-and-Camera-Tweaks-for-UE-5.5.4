# Weapon Viewmodel FOV — N/O Static Contract

## Result

Static inspection confirmed that the invariant `N` package contains a compiled
WVF implementation payload and selector-related tokens, while `O` contains the
concrete profile marker:

```text
N: /Weapon_Viewmodel_FOV/WVF.WVF_C
N: .wvf, all-, DeferredActorSpawnFromClass, ExecuteUbergraph
O: Stalker2/Content/GameLite/all-0.wvf
```

The N containers are byte-identical between FOV 0 and FOV 40. The N `.utoc`
name table contains the shared FOV, offset, scope and binocular profile set.

## Confirmed and unconfirmed

```text
N contains WVF implementation indicators: CONFIRMED
N contains .wvf/all- selector tokens:      CONFIRMED
O contains exact all-0.wvf marker path:     CONFIRMED
N → exact all-0.wvf dependency:             NOT PROVEN
marker → profile → runtime reapply action: NOT PROVEN
```

This corrects the earlier metadata-only interpretation of N. It still does not
justify inferring actor spawning, file reading or continuous reapplication from
strings alone.

## Scope and safety

- Read-only static inspection only.
- Production source, ASI, release assets and installed game files unchanged.
- No broad UE4SS enumeration, value writes or archive repackaging.

## Next gate

Further work requires a separate bounded cooked-Blueprint serialization/parser
pass or a targeted runtime probe for the WVF Blueprint behavior. Production
integration remains unauthorized until runtime ownership and visible proof are
established.
