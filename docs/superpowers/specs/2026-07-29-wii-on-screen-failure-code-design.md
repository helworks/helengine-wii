# Wii On-Screen Failure Code Design

## Goal

Make a packaged Wii boot failure identifiable on real vWii hardware without relying on SD, USB, ISFS, fonts, cooked assets, generated-engine rendering, or USB Loader GX settings.

## Evidence

The IOS56 image now reaches the Wii executable through USB Loader GX and presents the runtime failure color. The loader diagnostic confirms that IOS249 with base 56 is selected and that the WBFS fragment list is accepted. The missing SD and ISFS traces therefore no longer justify treating the loader or image handoff as the primary unknown; the remaining ambiguity is which runtime boundary selects the red failure state.

The GameCube hardware investigation established a useful diagnostic pattern: write a compact hexadecimal checkpoint directly into both external framebuffers after GX completes its display copy. This bypasses the same engine, asset, font, and loader-dependent paths that are under investigation.

The initial green frame on Wii is not an intentional boot phase. `WiiApplication::InitializeVideo` exposes newly allocated external framebuffers before clearing them. Both framebuffers must be cleared to black before video output is enabled.

## Chosen Approach

The Wii host will track one four-digit hexadecimal failure code while startup and the first frame progress through risky boundaries. Tracking a code does not present it and does not change the black startup display.

When a failure path selects the existing red failure color, `PresentFrame` will write the tracked code directly into both external framebuffers after `GX_DrawDone`. The overlay will use fixed 3-by-5 hexadecimal glyphs encoded in native code and packed YCbYCr framebuffer colors. It will not initialize a console, load a font, submit GX geometry, allocate an asset, or call generated-engine rendering.

The visible failure frame will contain:

- the existing solid red background
- a small black rectangle near the upper-left safe margin
- four white hexadecimal characters such as `A003`

Normal startup remains black. Successful engine rendering remains unchanged.

## Failure-Code Families

The code families identify the broad runtime subsystem while the remaining digits identify the last risky boundary entered:

| Family | Meaning |
| --- | --- |
| `Axxx` | generated-core and packaged-storage initialization |
| `Bxxx` | scene loading and first update |
| `Cxxx` | first draw and presentation preparation |
| `E000` | failure occurred without a more specific assigned boundary |

Initialization codes will distinguish core construction, initialization options, packaged-storage initialization, packaged catalog creation, bridge construction, window setup, core initialization, generated-module registration, and startup-scene queueing. Update and draw codes will distinguish their precondition checks and the major calls already separated in `WiiApplication`.

The diagnostic reports the boundary entered, not an exception type. Existing exception handling and return behavior remain authoritative.

## Architecture

`WiiFailureScreen` will own the native framebuffer overlay implementation in its own header and source file. It will expose a narrow operation that receives the render mode, both external framebuffers, and the failure code. This keeps diagnostic pixel encoding out of `WiiApplication` and preserves one clear responsibility per class.

`WiiApplication` will:

1. initialize its tracked failure code to `E000`
2. update the code immediately before each risky startup, update, or draw boundary
3. retain the existing red failure color and black normal boot colors
4. invoke `WiiFailureScreen` only after a red failure frame has been copied and GX has completed that copy
5. clear both newly allocated external framebuffers to black before enabling video output

The standalone `WiiBootHost` will also clear its allocated framebuffer before enabling video so its black-only contract is deterministic.

## GameCube Findings Relevant To Wii

The GameCube boot history exposed three loader-specific assumptions that inform the next root-cause investigation:

1. Nintendont did not patch libogc's private DVD completion handler, so a seemingly valid synchronous disc read stalled after loader handoff.
2. The working GameCube fix removed `DVD_Mount` from packaged startup and introduced a loader-compatible disc reader.
3. Direct framebuffer checkpoints remained observable when loader storage logs and engine rendering were unavailable.

The current Wii packaged startup similarly mixes GameCube-style `DVD_Init` and `DVD_MountAsync` operations with Wii `/dev/di` operations even though USB Loader GX has already opened the data partition before running the apploader. This is the leading root-cause candidate, but the failure-code build will identify the exact boundary before that storage path is changed.

## Testing

A focused builder-side source-contract test will be added first. It will require:

- a dedicated `WiiFailureScreen` implementation with fixed hexadecimal glyphs
- a tracked failure code in `WiiApplication`
- direct overlay invocation only on a failure frame and only after `GX_DrawDone`
- explicit black clears for both `WiiApplication` framebuffers before `VIDEO_SetBlack(FALSE)`
- an explicit black clear for `WiiBootHost` before `VIDEO_SetBlack(FALSE)`

After the test fails for the expected missing implementation, the runtime changes will be added. Validation will run the focused test and one packaged Wii build. The resulting WBFS will be tested through unchanged USB Loader GX settings, and the reported four-character code will select the next root-cause fix.

## Success Criteria

- no uninitialized green frame appears before startup
- all non-failure boot phases remain black
- a runtime failure remains red and shows a legible four-character white code
- the overlay has no dependency on filesystems, logs, fonts, cooked assets, or generated rendering
- normal successful frame presentation remains unchanged
- focused source-contract tests and the packaged native build succeed
