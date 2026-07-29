# Wii Draw-Stage Hardware Diagnostic Design

## Problem

The loader-safe Wii build reaches the native runtime on Wii U through USB Loader GX, initializes the engine, queues the startup scene, and completes an update. It then presents failure code `C002`, which identifies an exception somewhere inside `EngineCore->Draw()`. USB Loader GX provides no writable runtime trace file, so the existing checkpoint cannot distinguish scene asset loading from native rendering.

## Goal

Produce a diagnostic WBFS that reports the failing draw substage directly on the persistent red failure screen. The diagnostic must not require loader settings, initialize another filesystem, change rendering behavior, or depend on external logs.

## Considered Approaches

### Persistent stage codes (selected)

Record the last draw substage before each risky operation and translate it to a dedicated failure code if `EngineCore->Draw()` throws. This is deterministic, requires no writable device, and preserves the current control flow.

### Exception-message categories

Map known exception text to visible codes. This could identify a particular disc-read error, but it is fragile because generated and standard exception messages can change and unknown messages would remain ambiguous.

### Mount SD or USB for logging

Initialize FAT storage and write a detailed trace. This is rejected because USB Loader GX deliberately shuts down its subsystems before handoff; remounting storage would introduce another hardware variable into the exact startup path being diagnosed.

## Diagnostic Codes

- `C101`: failure while the generated core commits pending scene operations. This includes deferred scene loading and packaged asset reads performed at the frame boundary.
- `C102`: failure while the Wii 3D manager asks the Wii 2D manager to capture overlay drawables.
- `C103`: failure while the Wii scene bridge builds or validates the native frame plan.
- `C104`: failure while the Wii raster renderer submits the frame through GX.

The existing `C002` remains the fallback when no more specific stage was recorded. `C003` continues to identify failure after the generated draw returns, while captured 2D commands are submitted.

## Architecture

`WiiRenderManager3D` owns a small draw-stage enum because it owns the 2D capture, frame-plan, and raster boundaries. It updates that state immediately before each operation and exposes the last stage through a read-only method.

`WiiApplication` remains responsible for presentation policy. If `EngineCore->Draw()` throws, it first inspects `Core.LastSceneTransitionStage` to identify a frame-boundary scene commit. Otherwise it reads the Wii render manager's last draw stage and maps it to `C102`, `C103`, or `C104`. It then enters the existing persistent failure loop.

No generated C++ output will be edited. The diagnostic relies only on the generated core's existing `LastSceneTransitionStage` property and native Wii-owned source.

## Control Flow

1. `WiiApplication` sets the fallback checkpoint to `C002` and calls `EngineCore->Draw()`.
2. If the core is committing pending scene operations and an exception escapes, the catch path changes the checkpoint to `C101`.
3. When control enters `WiiRenderManager3D::Draw()`, the manager records `OverlayCapture`, `FramePlanBuild`, and `RasterSubmission` before their respective operations.
4. If an exception escapes the Wii renderer, the application maps the recorded stage to `C102`, `C103`, or `C104`.
5. Successful generated drawing proceeds unchanged to the existing `C003` checkpoint and captured-command submission.

## Validation

- Add source-contract tests for all four code values, stage updates, catch-path mapping, and preservation of `C002` as a fallback.
- Build the packaged-disc runtime to prove the native types and generated-core property compile together.
- Verify the ISO and derived WBFS with WIT.
- Boot the ISO through Dolphin and confirm the normal scene still initializes and renders without displaying a diagnostic code.
- Use the WBFS on Wii U through ordinary USB Loader GX settings. The resulting visible code is the hardware diagnostic result.

## Non-Goals

- Fixing the underlying draw failure before its failing substage is known.
- Adding or changing USB Loader GX settings.
- Restoring SD, USB, or ISFS trace output.
- Editing generated C++ code or changing scene/rendering behavior.
