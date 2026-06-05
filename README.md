# Helengine Wii Host

This repository contains the native Wii host scaffold for Helengine.

## Current milestone

- Docker-only build using devkitPro Wii tooling
- Native `.dol` output for direct loading in Dolphin
- Host-only pink-frame bootstrap when `HELENGINE_CORE_CPP_ROOT` is unset
- Generated-core build that initializes the Wii runtime host and emits a `.dol`

## Build

```bash
docker build -t helengine-wii .
docker run --rm -v "$PWD":/workspace -w /workspace helengine-wii make
```

If Docker Desktop's credential helper blocks anonymous pulls on this machine, use:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-wii .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-wii make
```

The build emits `build/helengine_wii.dol`.

## Generated core seam

The native build consumes generated engine output from `HELENGINE_CORE_CPP_ROOT` when a generated core deployment root is provided.

## Generated core build

Generate Wii-targeted core output:

```bash
rtk dotnet run --project C:\dev\helworks\csharpcodegen\codegen\codegen.csproj --artifacts-path C:\tmp\csharpcodegen-wii-codegen -- --cpp --project C:\dev\helworks\helengine\engine\helengine.core\helengine.core.csproj --output C:\dev\helworks\helengine-wii\tmp\generated-core-wii --platform wii --compiler gcc --endianness big --preset wii-core-boot
```

Build the Wii player with generated core enabled:

```bash
rtk docker run --rm -v C:/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii helengine-wii make clean all
```

The build emits `build/helengine_wii.dol`.

## Generated core verification

For a deterministic emulator probe, build with a frame limit:

```bash
rtk docker run --rm -v C:/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii -e HELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT=3 helengine-wii make clean all
```

Load `build/helengine_wii.dol` in Dolphin.

Expected result:

- `Core::Initialize(...)` completes
- `Update()` and `Draw()` both run for at least three frames
- the visible frame reaches the generated-frame diagnostic teal color before the verification build exits

## Boot check

Load `build/helengine_wii.dol` in Dolphin. The expected result for this milestone is a solid pink frame with no immediate crash or reset loop.
