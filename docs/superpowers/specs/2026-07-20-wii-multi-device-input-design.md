# Wii Multi-Device Input Design

## Goal

Support Wiimote-plus-Nunchuk and GameCube controllers concurrently on Wii. The engine exposes physical devices separately, while gameplay actions can be activated by either device.

## Device Model

- The Wiimote is exposed as its own device with connection state, D-pad, A, B, 1, 2, Plus, and Minus buttons.
- The attached Nunchuk is represented as the Wiimote extension device and exposes its analog stick as a left-stick input. Its connection state is distinct from the Wiimote’s state.
- GameCube controller ports remain separate engine devices and retain their existing buttons, D-pad, analog sticks, triggers, and connection state.
- Polling must capture Wiimote/Nunchuk data and GameCube data in the same frame; one device must not suppress the other.

## Demo Disc Action Mapping

The Demo Disc resolves actions across all connected Wii devices:

- Wiimote A and GameCube A activate Play.
- Wiimote B and GameCube B activate Menu/Back.
- Wiimote D-pad and GameCube D-pad navigate menus.
- Nunchuk stick and GameCube left stick navigate menus and control gameplay tilt.
- Wiimote 1 and 2 remain available to the engine but are not used or shown by the Demo Disc profile.

The Wii UI hint displays `A Play   B Menu` and identifies both D-pad and Nunchuk-stick navigation where the surrounding layout supports it.

## Runtime Architecture

The native Wii backend polls each physical source independently and publishes separate device states through the engine input backend contract. Game-level action resolution remains above the backend and evaluates all connected device states for each action. This keeps device identity available for future profiles without requiring each game to duplicate native polling.

## Failure Handling

- A missing Wiimote, missing Nunchuk, or missing GameCube controller is a normal disconnected state, not a runtime error.
- A Wiimote without a Nunchuk still contributes its buttons and D-pad.
- A GameCube controller remains usable when no Wiimote is connected.
- Unsupported extension types remain disconnected extensions and must not corrupt the Wiimote button state.

## Validation

Add focused source and behavior coverage for:

1. Wiimote polling uses a data format that includes the Nunchuk extension.
2. Wiimote A/B and D-pad mappings are stable.
3. Nunchuk stick axes map to the shared left-stick convention.
4. GameCube polling remains active in the same frame.
5. Device slots remain separate while Demo Disc actions accept either source.
6. Demo Disc Wii hints use A/B terminology and do not mention 1/2.

