# Helengine Wii Host

This repository contains the Wii platform host and builder integration for Helengine.

## Build

```powershell
dotnet run --project ..\helengine\tools\build-waiter\helengine.buildwaiter.csproj -- `
  --output ..\helprojs\city\wii-build `
  --require game.iso `
  -- powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\scripts\build-platform.ps1 `
  -Project ..\helprojs\city\project.heproj `
  -Platform wii `
  -Output ..\helprojs\city\wii-build
```

The Build Waiter returns successfully only after `game.iso` is fresh and non-empty.

## Run In Emulator

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_in_emulator.ps1 `
  -ArtifactPath ..\helprojs\city\wii-build\game.iso
```

## More Docs

- [Docker Build Notes](docs/Docker.md)
- [Platform Notes](docs/PlatformNotes.md)

## Portable source imports

The builder targets .NET 9 and reuses the engine's portable texture importer registry for raw image work items, preserving authored importer identifiers, RGBA row order, alpha and runtime asset ids. Serialized textures and embedded font atlases retain their existing paths; Wii-native encoding and numeric serialization policies remain selected by the build request. Native SDK compilation still requires the configured Docker image, and disc packaging requires an absolute `HELENGINE_WII_WIT_PATH` to the host's WIT executable.
