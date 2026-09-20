# CUE4Parse WVF research dumper

Targeted research tool for the standard Weapon Viewmodel FOV N IoStore container.

This project is intentionally not part of the production build. It must only read
the disposable package copy under `research-artifacts/weapon-fov-isolation-backup`.

Current gate: direct `IoStoreReader` opens and mounts N; the WVF export data chunk
is readable. `IoPackage` reaches serialization but requires a UE5.5.4-compatible
`.usmap` because the package uses unversioned properties.
