# Wii Menu Input Design

## Goal

Make the packaged Wii main menu navigable in Dolphin and on Wii by feeding the existing generated menu/input systems with a correct Wii controller signal.

## Current Problem

`WiiInputManager` currently polls only `PAD_*`, which is the GameCube controller path. In a normal Wii/Dolphin Wii session, the rendered menu appears correctly but receives no usable Wii Remote input, so the shared `MenuComponent` never gets the expected navigation and confirm/back actions.

## Scope

This design covers:

- sideways Wii Remote menu navigation
- existing GameCube controller fallback
- mapping both device types into one logical `InputGamepadState`

This design does not cover:

- Wii pointer aiming/clicking
- Nunchuk support
- multi-controller menu ownership
- runtime-specific menu logic outside the input backend

## Input Contract

The Wii host should continue to satisfy the existing generated engine input abstraction instead of introducing a Wii-specific menu path.

The runtime will still expose a single logical gamepad frame to the engine:

- `DPadUp`
- `DPadDown`
- `DPadLeft`
- `DPadRight`
- `South`
- `East`
- `Start`
- `Select`

The shared `MenuComponent` and `StandardPlatformInput` layers remain the source of truth for action resolution, press/release behavior, and repeat handling.

## Device Strategy

`WiiInputManager` should initialize and scan both controller backends every frame:

- `PAD` for GameCube controller support
- `WPAD` for Wii Remote support

The backend should resolve one active logical controller each frame:

1. Prefer a connected and active Wii Remote in sideways mode.
2. Otherwise fall back to the GameCube controller state.

This keeps the engine contract simple while allowing both common dev/test paths to work.

## Button Mapping

### Sideways Wii Remote

- `D-pad` -> `DPadUp`, `DPadDown`, `DPadLeft`, `DPadRight`
- `2` -> `South` for confirm/select
- `1` -> `East` for back/cancel
- `+` -> `Start`
- `-` -> `Select`

### GameCube Controller

Keep the existing GameCube mapping:

- `D-pad` -> `DPadUp`, `DPadDown`, `DPadLeft`, `DPadRight`
- `A` -> `South`
- `B` -> `East`
- `X` -> `West`
- `Y` -> `North`
- `Start` -> `Start`
- `Z` -> `Select`
- sticks and triggers remain populated as they are today

## Behavior Boundary

Held navigation behavior should not be implemented as Wii-specific menu code.

The Wii backend only reports stable per-frame button state. The shared engine input/menu stack remains responsible for:

- press detection
- release detection
- hold repeat timing
- menu selection movement

This avoids creating a second menu-input implementation on Wii.

## Implementation Shape

### Runtime

Update `WiiInputManager` to:

- include `wiiuse/wpad.h`
- call `WPAD_Init()`
- configure Wii Remote operation for sideways menu input
- call `WPAD_ScanPads()` each frame
- read Wii Remote buttons and convert them into the logical gamepad contract
- preserve the existing GameCube polling path as fallback

### Tests

Add source-contract coverage proving that:

- `WPAD` headers are included
- `WPAD_Init()` is called
- `WPAD_ScanPads()` is called
- sideways Wii button mappings exist for `D-pad`, `1`, `2`, `+`, and `-`
- the `PAD` fallback path still exists

## Verification

Implementation will be considered complete when:

1. the new focused source-contract test fails before implementation
2. the focused test passes after implementation
3. the packaged Wii test slice passes
4. the packaged Wii native rebuild passes
5. a fresh ISO boots in Dolphin and the menu can be navigated with the target controller mapping
