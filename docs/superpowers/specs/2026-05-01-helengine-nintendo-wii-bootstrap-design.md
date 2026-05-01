# Nintendo Wii Bootstrap Design

## Goal

Create the first working `helengine-wii` bootstrap that builds in Docker with devkitPro `devkitPPC`, targets Wii only, and boots in Dolphin to an immediate solid pink screen with no input dependency.

## Constraints

- The repository should stay structurally close to `helengine-gc`.
- The build must be Wii-only, not a dual GameCube/Wii target.
- The first milestone should keep the same generated-core seam used in the other platform repos.
- The first runtime target is intentionally minimal: initialize, display pink, and keep running.

## Chosen Approach

Use a shared-shape scaffold with Wii defaults.

This keeps the repo layout and layering close to `helengine-gc` so future shared GX-oriented code can move or be referenced mechanically, including a possible future `helengine-gc` submodule. At the same time, this repo will not pretend to be cross-target: the build rules, macros, and boot path will be explicitly Wii-only from the start.

## Repository Shape

The initial scaffold should include:

- `Dockerfile`
- `Makefile`
- `README.md`
- `src/main.cpp`
- `src/platform/wii/WiiBootHost.hpp`
- `src/platform/wii/WiiBootHost.cpp`

The file naming should remain platform-explicit so future shared code can live above the platform host layer without blurring the boot boundary.

## Build Design

The Docker image should follow the working GameCube pattern:

- Base image: `devkitpro/devkitppc:latest`
- Explicit `DEVKITPRO`, `DEVKITPPC`, and `LIBOGC` environment variables
- Explicit tool paths for the compiler and `elf2dol`

The Makefile should be derived from the proven GameCube scaffold, but targeted for Wii:

- Include `$(DEVKITPPC)/wii_rules`
- Set `HW_RVL=1`
- Use Wii `MACHDEP`
- Link against libogc and its dependencies using explicit library search paths
- Output `build/helengine_wii.elf` and `build/helengine_wii.dol`

The build should preserve:

- `HELENGINE_CORE_CPP_ROOT ?=`
- `HELENGINE_WII_HAS_GENERATED_CORE=0/1`

This keeps the same generated-core override seam used elsewhere without adding generated code now.

## Runtime Design

`main.cpp` should hand off immediately to a Wii-specific boot host.

`WiiBootHost` should:

- initialize the Wii video subsystem
- configure a framebuffer using the normal libogc Wii path
- clear the visible frame to a solid pink color
- flush/video-sync as needed
- remain alive in a simple loop so Dolphin keeps showing the frame

No controller setup, input polling, audio, filesystem, or GX renderer setup is required for this milestone.

## Platform Boundary

This repository is Wii-first and Wii-only for now.

That means:

- no GameCube compatibility toggle
- no shared GC/Wii compile target inside this repo
- no effort spent making the first bootstrap run on both platforms

Future code sharing is expected to happen through layout compatibility and clean boundaries, not through weakening the Wii target.

## Verification

Success means:

1. `docker build -t helengine-wii .` succeeds
2. `docker run --rm -v "$PWD":/workspace -w /workspace helengine-wii make` succeeds
3. the build emits `build/helengine_wii.dol`
4. Dolphin boots the `.dol` and shows a stable solid pink screen immediately

## Non-Goals

- GameCube compatibility
- Wii-exclusive subsystem initialization beyond what is needed to boot and show video
- controller input
- shared engine runtime code beyond the generated-core seam
- GX-based rendering for this first milestone
