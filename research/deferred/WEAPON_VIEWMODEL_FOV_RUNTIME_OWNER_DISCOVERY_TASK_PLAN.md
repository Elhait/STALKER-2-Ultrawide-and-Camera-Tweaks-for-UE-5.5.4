# Weapon Viewmodel FOV — Runtime Owner Discovery

## Batch 1 result

Static inspection confirmed an explicitly loaded WVF mod descriptor in N and
compiled payload indicators:

```text
Weapon_Viewmodel_FOV.uplugin:
  ExplicitlyLoaded=true
  BuiltInInitialFeatureState=Registered
  Mod=true

N payload:
  /Weapon_Viewmodel_FOV/WVF.WVF_C
  .wvf
  all-
  DeferredActorSpawnFromClass
  ExecuteUbergraph

O payload:
  Stalker2/Content/GameLite/all-0.wvf
```

The local `repak 0.1.7` tool reported an Oodle hash mismatch while unpacking
the N payload, and the extracted AssetRegistry was zero bytes. Therefore the
exact Blueprint class argument, dependency edge and runtime owner were not
recovered.

## Current state

```text
N implementation/infrastructure          CONFIRMED
N explicitly loaded plugin descriptor    CONFIRMED
N WVF Blueprint/string indicators        CONFIRMED
O exact profile marker                   CONFIRMED
marker → selector → spawned helper      NOT PROVEN
runtime owner/state                      UNKNOWN
production seam                          UNKNOWN
```

## Deferred next step

Use a parser compatible with this UE 5.5.4/Oodle cooked Blueprint payload, or
obtain a separately approved targeted runtime bridge. Do not infer the spawn
class or expand to broad UObject enumeration from the current strings.

Production source, ASI and release assets remain unchanged.
