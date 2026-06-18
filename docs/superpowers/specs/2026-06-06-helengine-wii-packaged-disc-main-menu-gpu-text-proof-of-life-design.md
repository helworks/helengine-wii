# Helengine Wii Packaged-Disc Main Menu GPU Text Proof-of-Life Design

## Goal

Render the authored packaged-disc main menu text in Dolphin using only GPU-driven glyph quads built from the cooked font assets already staged into the Wii disc image.

## Scope

This slice is limited to first visible proof-of-life for packaged-disc menu text.

In scope:

- packaged-disc Wii boot only
- loading packaged font assets and their staged external atlas textures
- GPU-only text rendering for all authored `ITextDrawable2D` instances captured during the menu frame
- readable authored title and button text in Dolphin
- focused Wii runtime source tests that lock the text-rendering contract

Out of scope:

- direct-`DOL` developer boot improvements
- CPU text rasterization or fallback font generation
- sprite rendering
- rounded-rectangle rendering
- authored background panels, ornaments, or full menu polish
- general 2D renderer completion beyond the minimum text path

## Constraints

The implementation must preserve these hard constraints:

- no CPU rendering
- cooked font assets only
- GPU-only rendering
- packaged-disc startup path only

The proof target is text-first. Every authored menu text drawable should become visible, even if non-text UI elements remain absent.

## Current Baseline

The packaged-disc runtime already reaches the authored startup scene and captures 2D draw requests through `WiiRenderManager2D`.

The missing behavior is native rendering:

- `WiiRenderManager2D::DrawText` only queues text drawables
- `WiiRenderManager2D::BuildTextureFromRaw` still throws
- `WiiRenderManager2D::BuildTextureFromCooked` still throws
- the frame loop presents diagnostic clear colors but no glyph quads

The generated Wii runtime currently resolves packaged scene textures and external cooked font atlases through the generic content manager, then calls `RenderManager2D::BuildTextureFromRaw`. That means the first proof slice should target raw `TextureAsset` upload into GX-backed runtime textures, not a separate platform-owned cooked-texture file format.

## Chosen Approach

Add the smallest native Wii text path on top of the existing 2D capture bridge.

The runtime will:

1. keep packaged-disc scene loading unchanged
2. implement one Wii-owned runtime texture type that can hold a GX texture object and its uploaded atlas bytes
3. implement `WiiRenderManager2D::BuildTextureFromRaw` so packaged font atlas `TextureAsset` payloads become native runtime textures
4. render every queued `ITextDrawable2D` as textured glyph quads during the Wii frame draw path

This approach is intentionally narrower than building a full 2D command system. It proves that the authored menu scene can materialize readable text from packaged content without taking on sprites, panels, or generalized UI rendering in the same slice.

## Rendering Design

The first native Wii text pass should mirror the shared text layout semantics closely enough that authored menu text appears where the scene expects it.

Required behavior:

- use the `FontAsset` attached to each text drawable
- use the font's runtime atlas texture, never a CPU-generated bitmap
- honor `Text`, `Color`, `FontScale`, `WrapText`, `Size`, and `Alignment`
- draw glyphs as textured screen-space quads with GX
- keep all text submission on the GPU path

Acceptable simplifications for this proof slice:

- one straightforward 2D orthographic setup
- one glyph at a time submission
- simple alpha blending suitable for font atlases
- rough but stable positioning as long as authored text remains readable

This slice does not need:

- sprite batching
- panels
- clip rectangles
- advanced per-character effects
- generalized 2D material abstraction

## Texture Path

The native texture work should be just enough to support packaged font atlas uploads.

Expected behavior:

- accept the `TextureAsset` payload produced by packaged font staging
- validate supported color formats explicitly and fail loudly on unsupported ones
- allocate a Wii runtime texture object with width, height, ownership metadata, and GX texture state
- upload atlas bytes once during asset resolution, then reuse the runtime texture during rendering
- release native texture memory through the normal `ReleaseTexture` and font disposal path

The implementation should avoid speculative support for every future texture format unless the packaged menu fonts require it right now.

## Integration Boundary

Expected touch points:

- `src/platform/wii/WiiRenderManager2D.hpp`
- `src/platform/wii/WiiRenderManager2D.cpp`
- one or more new Wii runtime texture/native GX helper files under `src/platform/wii/`
- `src/platform/wii/WiiApplication.cpp` only as needed to invoke the native text pass cleanly
- `builder.tests/WiiRuntimeSourceTests.cs`

The packaged build workspace and scene bootstrap should remain behaviorally unchanged unless a small test-backed correction is required for packaged font atlas staging.

## Verification

Required verification:

- focused `builder.tests` covering the Wii runtime source contract for GPU text support
- packaged Wii rebuild using the shared `build-platform.ps1` flow
- packaged-disc Dolphin run

Success means all of the following are true:

1. the packaged-disc build still boots the authored main menu scene
2. packaged font atlas textures resolve without throwing
3. Dolphin shows readable authored menu text on screen
4. the text is coming from the cooked font atlas GPU path, with no CPU-rendered fallback

## Risks

- the packaged font atlas payload may use a texture format that the first Wii upload path does not yet support
- text placement may initially be close but not exact if the Wii path does not fully mirror existing alignment helpers
- broadening the work into sprites or generic 2D infrastructure would dilute the proof-of-life goal
