# Weapon Viewmodel FOV — WVF Runtime Discovery

## Result

The bounded exact-name UE4SS probe was completed in two reference states:

```text
N + O:  WVF_Actor, Weapon_Viewmodel_FOV and WVF → no exact reflected matches
N only: WVF_Actor, Weapon_Viewmodel_FOV and WVF → no exact reflected matches
```

The probe itself loaded successfully and performed no writes. The identical
result did not distinguish the configurations. Container names are therefore
not confirmed reflected class names or live object names.

## Evidence and limits

The component isolation matrix remains:

```text
A  N + F_99_P + O  CORRECT
B  F_99_P only      BROKEN
C  N + O only       CORRECT
D  N only           BROKEN
E  F_99_P + O       BROKEN
```

`N + O` is the smallest tested working set, but the exact runtime mechanism is
not identified. The reference installation was restored after the probe.

## Deferred next step

Do not start broad generic enumeration automatically. If reopened, design a
new bounded probe for likely reflected asset types (`BlueprintGeneratedClass`,
data assets or actors), with explicit throttling and a safe failure path. Any
future probe remains research-only; production source and ASI require separate
visual ownership proof.
