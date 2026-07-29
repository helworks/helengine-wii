# Wii Scene-Load Stage Diagnostic Design

## Problem

The fresh hardware build reports `C002` on an orange background. The orange background proves USB Loader GX launched the current WBFS, but `C002` is still too broad to locate the exception.

The generated engine already records detailed scene-loading boundaries. `SceneManager.RecordTraceState()` writes `SceneManager:<stage>` to `Core.LastSceneTransitionStage`, and `RuntimeSceneLoadService.RecordTraceState()` writes `SceneLoad:<stage>`. A few scene-loaded event and cleanup boundaries write direct marker names. The Wii catch path currently recognizes none of those detailed values, so a scene-loading exception falls back to `C002` even after entering a known stage.

## Goal

Produce one hardware diagnostic build that converts the existing scene-manager and scene-load markers into persistent visible failure codes with distinct background colors. The result must identify whether the exception occurs during packaged content loading, entity/component materialization, loaded-scene registration, event dispatch, or cleanup.

The diagnostic must not alter scene-loading control flow, preallocate collections, disable frame-boundary commits, edit generated C++, depend on writable storage, or require USB Loader GX settings.

## Considered Approaches

### Map the existing detailed trace state (selected)

Inspect `Core.LastSceneTransitionStage` only after a caught generated draw exception. Use its `SceneManager:` or `SceneLoad:` prefix to select the owning trace object, then map that object's existing last-stage fields to Wii failure codes. When component deserialization is active, use the existing component type identifier to distinguish the components contained in the startup splash scene.

This preserves runtime behavior and provides the most information from one hardware test.

### Map only broad scene-loading regions

Use one code for scene deserialization and one for scene registration. This requires fewer codes but could leave component construction, hierarchy initialization, and collection insertion ambiguous, leading to another hardware build.

### Split frame-boundary execution in the Wii host

Disable automatic scene commits during `Core.Draw()` and invoke `CompleteFrameBoundary()` separately, as an earlier GameCube diagnostic did. This would expose an outer boundary but would change normal control flow and still would not locate the operation within scene loading.

## Diagnostic Ownership and Precedence

`WiiApplication.RefineDrawFailureCheckpoint()` remains the only policy layer that translates runtime state into a visible Wii failure code. It resolves a caught failure in this order:

1. Preserve an active Wii-native renderer stage when one exists.
2. Map direct scene-loaded event and cleanup markers.
3. If `Core.LastSceneTransitionStage` starts with `SceneLoad:`, map `RuntimeSceneLoadService.LastTraceStage` and its component type identifier.
4. If it starts with `SceneManager:`, map `SceneManager.LastTraceStage`.
5. Map the existing outer generated draw stages.
6. Preserve `C002` only when no recognized stage exists.

The prefix check is required because both trace objects retain their last values. Reading either object without confirming which subsystem wrote the current core marker could report stale state from an earlier successful operation.

## Scene-Manager Codes

The scene-manager family uses `C110` through `C11D`:

- `C110`: pending-operation commit begin or operation dispatch.
- `C111`: immediate scene-load entry and content-path resolution.
- `C112`: loaded-scene lookup and single-mode pre-load cleanup.
- `C113`: packaged scene content load entry.
- `C114`: runtime scene-load service call entry.
- `C115`: runtime scene-load service returned successfully.
- `C116`: loaded-scene record tracking entry.
- `C117`: loaded-scene list insertion returned successfully.
- `C118`: loaded-scene dictionary insertion returned successfully.
- `C119`: scene-loaded event dispatch entry.
- `C11A`: scene-loaded event dispatch returned successfully.
- `C11B`: scene-loaded event-argument release entry.
- `C11C`: scene-loaded event-argument release returned successfully.
- `C11D`: immediate scene-load completion or transient scene cleanup.

Markers that describe adjacent bookkeeping without a risky call map to the nearest following boundary. This keeps the code family finite while preserving the exact operation that had begun but had not returned.

Owned-asset registration uses `C140` through `C145`:

- `C140`: texture ownership registration.
- `C141`: font ownership registration.
- `C142`: audio ownership registration.
- `C143`: model ownership registration.
- `C144`: material ownership registration.
- `C145`: owned-asset registration returned successfully.

The exact scene-manager trace mapping is:

- `C110`: `CommitPendingOperationsAtFrameBoundaryBegin`, `CommitPendingOperationsAtFrameBoundaryOperation`, and `CommitPendingOperationsAtFrameBoundaryEnd`.
- `C111`: `LoadSceneImmediateBegin` and `LoadSceneImmediateBeforeResolveSceneContentPath`.
- `C112`: `LoadSceneImmediateAfterResolveSceneContentPath`, `LoadSceneImmediateBeforeLoadedSceneRecordLookup`, `LoadSceneImmediateAfterLoadedSceneRecordLookup`, `LoadSceneImmediateDisposeUntrackedRoots`, `LoadSceneImmediateUnloadSingleModeScenes`, `LoadSceneImmediateFlushReleasedTextures`, and `LoadSceneImmediateResetPhysicsTiming`.
- `C113`: `LoadSceneImmediateBeforeContentLoad`.
- `C114`: `LoadSceneImmediateBeforeSceneLoadServiceLoad`.
- `C115`: `LoadSceneImmediateAfterSceneLoadServiceLoad`.
- `C116`: `LoadSceneImmediateBeforeLoadedSceneRecordTrack`.
- `C117`: `LoadSceneImmediateAfterLoadedSceneRecordListAdd`.
- `C118`: `LoadSceneImmediateAfterLoadedSceneRecordDictionaryAdd`.
- `C119`: `LoadSceneImmediateBeforeSceneLoadedEvent`.
- `C11D`: `LoadSceneImmediateAfterSceneLoadedEvent` and `LoadSceneImmediateEnd`.
- `C140` through `C145`: `LoadSceneImmediateBeforeRegisterOwnedTextures`, `LoadSceneImmediateBeforeRegisterOwnedFonts`, `LoadSceneImmediateBeforeRegisterOwnedAudio`, `LoadSceneImmediateBeforeRegisterOwnedModels`, `LoadSceneImmediateBeforeRegisterOwnedMaterials`, and `LoadSceneImmediateAfterRegisterOwnedAssets`, respectively.

Direct core markers map as follows: `AfterSceneLoadedEventDispatch` to `C11A`, `BeforeSceneLoadedEventArgsRelease` to `C11B`, `AfterSceneLoadedEventArgsRelease` to `C11C`, and `AfterTransitionSceneAssetRelease` or `AfterSceneTransitionCommit` to `C11D`.

## Runtime Scene-Load Codes

The runtime materialization family uses `C120` through `C125`:

- `C120`: scene materialization entry or root-entity iteration.
- `C121`: entity construction and base-field initialization.
- `C122`: unknown component deserialization.
- `C123`: child-entity recursion.
- `C124`: entity completion or hierarchy initialization.
- `C125`: scene materialization returned successfully.

When the active stage is component deserialization, known startup-scene component types override generic `C122`:

- `C130`: `helengine.CameraComponent`.
- `C131`: `helengine.RoundedRectComponent`.
- `C132`: `helengine.ViewportComponent`.
- `C133`: `helengine.ReferenceCanvasFitComponent`.
- `C134`: `city.menu.HelenOfCodeSplashComponent, gameplay`.
- `C135`: `helengine.SpriteComponent`.

Any other component type remains `C122`; the diagnostic does not assume that an unknown type is valid or construct a fallback component.

The exact runtime scene-load trace mapping is:

- `C120`: `LoadBegin` and `BeforeRootEntityLoad`.
- `C121`: `LoadEntityBegin`.
- `C122` or `C130` through `C135`: `BeforeComponentLoad` and `LoadComponentBegin`, selected by the active component type identifier.
- `C123`: `BeforeChildEntityLoad`.
- `C124`: `LoadEntityEnd`.
- `C125`: `LoadEnd`.

## Presentation

Every new failure code receives its own opaque dark RGB background in `WiiFailureScreen`. Codes within the same family use related hues, but no two codes share an RGB value:

- `C110` `{ 0x70, 0x10, 0x10, 0xFF }`, `C111` `{ 0x80, 0x28, 0x08, 0xFF }`, `C112` `{ 0x78, 0x48, 0x00, 0xFF }`.
- `C113` `{ 0x68, 0x60, 0x00, 0xFF }`, `C114` `{ 0x38, 0x68, 0x00, 0xFF }`, `C115` `{ 0x08, 0x68, 0x18, 0xFF }`.
- `C116` `{ 0x00, 0x68, 0x48, 0xFF }`, `C117` `{ 0x00, 0x58, 0x70, 0xFF }`, `C118` `{ 0x00, 0x38, 0x80, 0xFF }`.
- `C119` `{ 0x28, 0x28, 0x80, 0xFF }`, `C11A` `{ 0x50, 0x20, 0x80, 0xFF }`, `C11B` `{ 0x78, 0x10, 0x70, 0xFF }`.
- `C11C` `{ 0x80, 0x18, 0x48, 0xFF }`, `C11D` `{ 0x58, 0x38, 0x48, 0xFF }`.
- `C120` `{ 0x50, 0x18, 0x08, 0xFF }`, `C121` `{ 0x60, 0x38, 0x08, 0xFF }`, `C122` `{ 0x58, 0x58, 0x10, 0xFF }`.
- `C123` `{ 0x20, 0x58, 0x20, 0xFF }`, `C124` `{ 0x10, 0x50, 0x58, 0xFF }`, `C125` `{ 0x20, 0x30, 0x68, 0xFF }`.
- `C130` `{ 0x68, 0x18, 0x30, 0xFF }`, `C131` `{ 0x68, 0x30, 0x18, 0xFF }`, `C132` `{ 0x48, 0x58, 0x08, 0xFF }`.
- `C133` `{ 0x18, 0x58, 0x38, 0xFF }`, `C134` `{ 0x18, 0x40, 0x68, 0xFF }`, `C135` `{ 0x48, 0x18, 0x68, 0xFF }`.
- `C140` `{ 0x48, 0x20, 0x10, 0xFF }`, `C141` `{ 0x58, 0x40, 0x10, 0xFF }`, `C142` `{ 0x38, 0x58, 0x18, 0xFF }`.
- `C143` `{ 0x10, 0x58, 0x50, 0xFF }`, `C144` `{ 0x18, 0x38, 0x70, 0xFF }`, `C145` `{ 0x50, 0x18, 0x58, 0xFF }`.

White diagnostic glyphs and the existing persistent dual-framebuffer presentation remain unchanged.

Successful startup and all non-failure boot frames remain black. Existing codes and their approved colors remain unchanged, including orange `C002` and red fallback handling for unrelated failures.

## Testing and Validation

- Add a failing Wii source-contract test for every new enum value, stage mapping, prefix guard, component-type mapping, and distinct color entry before production edits.
- Confirm the test fails because the mappings do not yet exist.
- Implement only the Wii-owned enum, refinement, and presentation changes needed to pass the test.
- Run the focused Wii source-contract tests.
- Build a fresh packaged ISO and matched WBFS without editing generated output.
- Verify both images with WIT, including disc ID `RCIE01`, IOS56, and one DATA partition.
- Confirm the generated DOL contains the existing `SceneManager:` and `SceneLoad:` markers.
- Launch the ISO in Dolphin and confirm startup scenes still load and frames continue rendering.
- Test the WBFS through ordinary USB Loader GX settings and report the visible code and background color.

## Non-Goals

- Fixing the underlying hardware exception before its exact stage is observed.
- Preallocating scene collections based on the GameCube investigation.
- Changing scene commit timing or generated engine behavior.
- Adding SD, USB, or ISFS logging.
- Changing USB Loader GX configuration.
- Editing generated C++ files.
