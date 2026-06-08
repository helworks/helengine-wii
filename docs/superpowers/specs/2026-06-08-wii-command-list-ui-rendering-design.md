# Wii Command-List UI Rendering Design

## Goal

Make Wii 2D UI rendering follow the shared engine 2D command model so packaged-disc Wii output matches the authored menu structure instead of rendering text alone.

This work must make the packaged Wii main menu render its background, menu item panels, selection visuals, and text through one coherent Wii 2D path.

## Problem

The current Wii 2D bridge captures multiple drawable types but only renders text.

Current behavior in `src/platform/wii/WiiRenderManager2D.cpp`:

- text drawables are emitted through a GX glyph path
- sprite drawables are captured but never rendered
- rounded-rectangle drawables are captured but never rendered
- menu visibility today depends on text alone, so the authored menu structure does not match

That leaves Wii with a platform-specific “text-only overlay” instead of a real implementation of the engine’s 2D rendering contract.

## Desired Outcome

Wii should execute the same 2D rendering model the generated runtime already produces:

- quads and sprites
- glyphs
- rounded rectangles
- clip rect changes

The first target is exact structural parity for the authored main menu:

- background color visible
- menu item panels visible
- selection highlight visible
- current layout colors visible
- text still rendered correctly

Rounded corners do not need perfect final visual fidelity in this first pass, but bounds, border thickness, fill color, border color, and clip behavior must be honored.

## Source Of Truth

The source of truth for Wii non-3D UI rendering must be the generated command-list model, not ad hoc Wii-only drawable queues.

Relevant existing generated runtime pieces already present locally:

- `tmp/generated-core-wii/RenderCommandList2D.hpp`
- `tmp/generated-core-wii/RenderCommandList2D.cpp`
- `tmp/generated-core-wii/RenderCommandListBuilder2D.hpp`
- `tmp/generated-core-wii/RenderCommandListBuilder2D.cpp`

Relevant payload types already exist in that model:

- quad payloads
- glyph payloads
- rounded-rect payloads
- clip rect payloads

Wii should consume that model rather than inventing a parallel menu renderer.

## Architecture

### Frame Flow

Per frame:

1. shared engine drawables are visited as they are today
2. Wii builds one `RenderCommandList2D` for the frame
3. Wii executes the command list in order through GX
4. framebuffer present remains owned by `WiiApplication`

### Wii Render Manager Role

`WiiRenderManager2D` remains the Wii-side bridge, but its responsibility changes:

- keep frame lifecycle control
- own any command-list scratch state needed for Wii execution
- execute shared 2D commands in order
- apply clip rect changes immediately when encountered

It should stop being conceptually “the text overlay renderer.”

### Command Execution Surface

The Wii renderer should be organized around explicit command executors:

- `ExecuteQuadCommand`
- `ExecuteGlyphCommand`
- `ExecuteRoundedRectCommand`
- `ApplyClipRect`

These can be private `WiiRenderManager2D` methods or small helpers in the same translation unit. The important design rule is that command dispatch is explicit and ordered, not mixed into one large special-case loop.

## Rendering Behavior

### Quads And Sprites

Quad-like commands should render as textured GX quads using existing texture object support from `WiiRuntimeTexture`.

Expected behavior:

- honor quad bounds
- honor source rect
- honor color tint
- honor clip rect
- skip texture commands whose runtime texture is missing or not initialized

### Glyphs

Glyph rendering should stay supported, but it should execute as one command kind in the same command stream rather than as an isolated text-only pass.

The existing text rendering logic is a valid implementation reference for:

- font atlas texture binding
- glyph source rect mapping
- glyph color tint

### Rounded Rectangles

Rounded-rect commands must render at least:

- fill color
- border color
- border thickness
- bounds
- corner mask
- clip rect

First-pass quality target:

- visual structure must match the authored menu
- square and panel rendering must be correct
- rounded corners may be approximated if needed

Acceptable first-pass approximations:

- treat zero-radius rounded rects as plain solid quads
- approximate rounded corners with coarse geometry or quadrant omission as long as visible authored menu panels match structurally

Unacceptable first-pass shortcuts:

- ignoring rounded-rect commands entirely
- silently drawing only fills when the border is required for visible menu structure
- bypassing the shared command model with a menu-specific renderer

### Clip Rects

Clip rect commands must be honored in the same order the command stream emits them.

Wii implementation should map clip rect changes onto `GX_SetScissor`.

Behavioral requirements:

- clip rects affect subsequent commands until changed again
- default frame clip is the full frame
- command bounds should not permanently mutate the global frame scissor after the frame ends

## Data Ownership And Boundaries

The generated runtime owns command production.

The Wii platform layer owns:

- command-list lifetime during the frame
- GX state setup
- GX command execution
- clip/scissor application

The Wii platform layer must not:

- rewrite generated command data after the fact
- introduce menu-specific special cases for layout
- depend on hardcoded knowledge of `DemoDiscMainMenu`

## Testing Strategy

### Source-Contract Tests First

Before production edits, add failing assertions in `builder.tests/WiiRuntimeSourceTests.cs` that prove the Wii renderer now:

- references `RenderCommandList2D`
- references `RenderCommandListBuilder2D`
- executes rounded-rect commands
- applies clip rect changes
- no longer presents itself as a text-only rendering path

These tests are intended as source-contract tests, not pixel tests.

### Verification After Green

After code changes:

1. run targeted Wii source-contract tests
2. run the packaged Wii test slice
3. rebuild packaged-disc Wii native outputs
4. regenerate disc layout
5. package a fresh ISO
6. verify visibly in Dolphin that the menu background and panels render alongside text

## Risks

### GX State Drift

Because Wii currently configures separate solid and text pipelines, command-list execution could accidentally leave GX in the wrong state between commands.

Mitigation:

- centralize per-command pipeline setup
- restore full-frame scissor at frame end
- keep command dispatch explicit

### Command-Model Mismatch

The generated command-list builder may encode some assumptions that the current Wii direct text path never had to honor.

Mitigation:

- treat generated command payloads as canonical
- implement one command kind at a time with source-contract coverage

### Overfitting To The Menu

The visible target is the main menu, but the implementation should not be menu-specific.

Mitigation:

- no scene-id checks
- no hardcoded menu colors or positions
- command-driven execution only

## Non-Goals

This spec does not require:

- perfect antialiased rounded-corner rasterization
- a new generalized vector graphics layer
- generated code rewrites
- changes to authored menu scene data
- changing the 3D renderer

## Success Criteria

This design is complete when all of the following are true:

- Wii no longer renders only text for authored 2D UI
- packaged-disc Wii shows the authored menu background and panel structure
- text, panels, and clip behavior come from one shared command-driven path
- the implementation is generic enough to serve other authored 2D UI scenes, not just `DemoDiscMainMenu`
