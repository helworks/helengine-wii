# Helengine Wii Host

This repository contains the native Wii host scaffold for Helengine.

## Current milestone

- Docker-only build using devkitPro Wii tooling
- Native `.dol` output for direct loading in Dolphin
- First boot check with a solid pink screen

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

The native build reserves `HELENGINE_CORE_CPP_ROOT` for later `cs2.cpp` integration, but the first milestone does not compile generated core output yet.

## Boot check

Load `build/helengine_wii.dol` in Dolphin. The expected result for this milestone is a solid pink frame with no immediate crash or reset loop.
