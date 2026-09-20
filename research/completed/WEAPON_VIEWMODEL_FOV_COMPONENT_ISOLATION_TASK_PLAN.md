# Weapon Viewmodel FOV — Component Isolation

## Result

The complete reference `Weapon Viewmodel FOV - 0` installation preserves
correct weapon framing after the tested cinematic EXIT. Component isolation
produced this matrix:

```text
A  N + F_99_P + O  CORRECT  (control)
B  F_99_P only      BROKEN
C  N + O only       CORRECT
D  N only           BROKEN
E  F_99_P + O       BROKEN
```

The smallest tested working set is `N + O`. The `F_99_P` replacement curves
are not required for the post-cinematic correction. `N` and `O` are both
required in the tested combinations.

## Evidence

The test used the same save and cinematic scenario for every configuration.
No ADS, pause or re-equip repair was used before recording the result. All nine
reference `FOV 0` files were restored after testing.

```text
N + O active → correct post-cinematic weapon framing
N without O  → broken
O without N  → broken
F_99_P alone → broken
```

`N` contains the shared WVF asset set (`WVF`, `WVF_Actor`,
`Weapon_Viewmodel_FOV`, and the named FOV/offset/scope/binocular profiles),
but runtime loading or continuous overwrite is not inferred from names alone.
The `O` group remains a functional marker/selector candidate; its exact loader
semantics are not established.

## Scope boundaries

- `STALKER2CameraTweaks.asi` and production source were not modified.
- CameraComponent/PlayerCameraManager research was not reopened.
- Custom Weapons were excluded.
- No runtime implementation or release packaging was performed.

## Next research gate

If research is reopened, use UE4SS with the complete reference installation to
look for runtime objects/assets named `WVF`, `WVF_Actor` and
`Weapon_Viewmodel_FOV`. Require runtime ownership evidence and visible proof
before considering any production integration.
