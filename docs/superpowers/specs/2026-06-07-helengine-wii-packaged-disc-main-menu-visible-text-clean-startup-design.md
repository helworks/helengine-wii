# Helengine Wii Packaged-Disc Main Menu Visible Text Clean-Startup Design

## Goal

Make the packaged-disc Wii build reach the authored main menu in Dolphin with visible authored text, no Dolphin startup warning dialog, and GPU-only cooked-font rendering.

## Success Criteria

This slice succeeds only if one packaged-disc Dolphin run satisfies all of the following at once:

1. Dolphin shows no startup warning dialog.
2. Host-captured Dolphin logs provide the primary proof trail for the packaged startup and menu text path.
3. The emulation window visibly shows authored main-menu text from the cooked font atlas path.
4. No CPU text rendering or fallback font generation is introduced.

The required visual bar for this slice is text only. Backgrounds, panels, sprites, and general 2D completeness remain out of scope.

## Scope

In scope:

- packaged-disc boot only
- removal or correction of the current packaged-disc startup warning condition
- one repeatable Dolphin launch and host-log capture path for packaged-disc verification
- temporary packaged-disc runtime diagnostics that emit through host-captured logs
- minimal Wii runtime fixes needed to make authored main-menu text visible
- removal or gating of temporary diagnostics once the proof path is working

Out of scope:

- direct-`DOL` boot
- CPU text rendering
- sprite or rounded-rectangle rendering
- broader 2D renderer completion
- menu interaction or navigation
- visual polish beyond visible authored text

## Constraints

Hard constraints for this slice:

- packaged-disc is the only valid proof path
- no Dolphin startup warning dialog is allowed in the final proof run
- host-captured Dolphin logs are the main proof source
- GPU-only text rendering is required
- cooked font assets and their atlas textures are required

Temporary on-screen debug markers are allowed during development, but they are not the primary proof channel and must not remain part of the final success state.

## Current Baseline

The current Wii runtime already contains:

- packaged-disc scene bootstrap
- raw texture upload into `WiiRuntimeTexture`
- GX glyph-quad rendering in `WiiRenderManager2D`
- a 2D capture bridge that can visit cameras and queue text drawables

The current failure state is still incomplete:

- packaged-disc runs can still surface the `IOS_FS: Failed to rename temporary FST file` warning in some launch paths
- the visible output remains black or otherwise fails to show authored menu text
- host-captured Dolphin logs are not yet the dependable proof channel for the runtime text path

## Chosen Approach

Treat this as one packaged-disc-only debugging and rendering lane with a strict clean-start gate.

The work proceeds in this order:

1. stabilize one repeatable packaged-disc Dolphin launch path that captures host-visible logs
2. eliminate the startup-warning condition on that path
3. add temporary packaged-disc runtime diagnostics that emit through host-captured logs
4. identify the first broken link between scene load and visible text submission
5. apply the minimal runtime fix needed to make authored menu text appear
6. remove or gate temporary debug-only output so the proof build is clean

This keeps the slice narrow and avoids drifting into generalized renderer or tooling work.

## Proof Path

The only accepted proof path for this slice is:

- launch an existing packaged-disc ISO in Dolphin
- capture host-visible logs during startup and menu boot
- confirm a clean startup with no warning dialog
- confirm visible authored menu text on screen

Screenshot-driven debugging is not the primary evidence path for this slice. Host-captured logs are.

Repo-local runtime text files are optional secondary diagnostics, but they are not the required success mechanism.

## Runtime Diagnostics

Temporary diagnostics should be added only where needed to answer the specific question: why authored menu text is still not visible.

Preferred diagnostic outputs:

- packaged startup scene id
- runtime content root
- font resolution stage
- first resolved font relative path
- first resolved atlas texture relative path
- enabled camera visitation count
- visited 2D drawable count
- queued text drawable count
- whether at least one glyph quad submission happened

These diagnostics should be written in a form that can surface through host-captured Dolphin logs.

Temporary on-screen markers may still exist during investigation, but they should remain small, disposable, and secondary to the log output.

## Rendering Boundary

The rendering fix must remain text-first.

Allowed work:

- fix queue visitation issues
- fix text submission preconditions
- fix font or atlas resolution issues
- fix packaged runtime state that prevents the text pass from becoming visible

Disallowed expansion:

- implementing sprite rendering
- implementing rounded-rectangle rendering
- building a general 2D command system
- adding CPU text fallback

## Likely Touch Points

Expected files likely involved in this slice:

- `src/platform/wii/WiiApplication.cpp`
- `src/platform/wii/WiiRenderManager2D.hpp`
- `src/platform/wii/WiiRenderManager2D.cpp`
- `src/platform/wii/WiiSceneBootstrap.cpp`
- `src/platform/wii/WiiDiscFileSystem.cpp`
- existing packaged-disc Dolphin helper scripts under `tmp/`
- `builder.tests/WiiRuntimeSourceTests.cs`

New files are acceptable only if they clearly support the packaged-disc launch/logging workflow or isolate temporary diagnostics cleanly.

## Verification

Required verification for implementation:

- focused Wii source-audit tests for any new runtime contract
- packaged-disc rebuild
- packaged-disc Dolphin run through the chosen log-capture path

The final proof run must demonstrate all of:

- no startup warning dialog
- host-captured logs showing the packaged text path reaching the expected stage
- visible authored main-menu text on screen
- no CPU-rendered fallback path

## Risks

- the startup warning may come from the packaged-disc launch environment rather than the menu renderer itself
- host-captured Dolphin logs may still hide the specific runtime lines needed, requiring a small targeted logging adjustment
- the current broken link may be before glyph submission, such as font resolution, text queue population, or presented-frame visibility

## Non-Goals

This slice does not promise:

- a complete main menu
- interactive navigation
- background art
- menu panel rendering
- generalized Wii UI renderer completeness

It only proves a clean packaged-disc boot and visible authored main-menu text through the GPU-only cooked-font path.
