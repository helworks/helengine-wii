# Wii Packaged Scene List Parity Design

## Goal

Make packaged Wii scene selection from the main menu work by ensuring the packaged Wii build receives the same authored scene list the Windows demo-disc build already includes.

## Current Problem

The packaged Wii menu can now navigate and confirm correctly, and `MenuComponent` already calls `SceneManager->LoadScene(...)` for scene-loading items. The remaining failure is earlier in the build pipeline: the packaged Wii build manifest/runtime scene manifest does not include all scenes referenced by the current menu, so those scenes are missing from the ISO.

## Scope

This design covers:

- packaged Wii build manifest scene parity with the Windows demo-disc build
- staged cooked scene parity for the packaged Wii ISO
- packaged Wii runtime scene manifest parity

This design does not cover:

- direct-DOL Wii developer bootstrap parity
- menu-driven scene discovery
- runtime scene-loading fallback behavior
- ISO packaging logic changes

## Source of Truth

The existing build system is already the source of truth for which scenes belong in a platform build. The fix must stay there.

The scene list should flow through the current pipeline exactly as it does for Windows:

1. build graph resolves the authored per-platform scene list
2. `PlatformBuildManifest.Scenes` contains that resolved scene set
3. Wii staging copies the corresponding `cooked/scenes/*` artifacts
4. `WiiRuntimeSceneManifestWriter` emits runtime entries from that same manifest
5. `wit` packages the extracted disc root into the ISO

If the scene is absent from `PlatformBuildManifest.Scenes`, it will also be absent from the staged disc root and the final ISO.

## Required Behavior

For the current main menu scene set, packaged Wii should receive the same scene ids the Windows demo-disc build already receives.

That means:

- no Wii-only scene subset
- no hand-maintained duplicate Wii scene list
- no menu parsing at runtime or packaging time

The Wii packaged build request should inherit the existing authored scene selection from the build graph.

## Implementation Boundary

The fix must target the scene-selection/build-request path, not later stages.

Specifically:

- do not patch `MenuComponent`
- do not patch `SceneManager`
- do not patch `WiiRuntimeSceneManifestWriter` to invent missing scenes
- do not patch ISO packaging to inject scene files outside the manifest flow

Instead:

- identify where the Windows packaged build request keeps the full scene set
- identify where the Wii packaged build request drops scenes
- remove that divergence so both platform requests carry the same scene set for the demo-disc target

## Testing Strategy

Add a focused test first that proves the packaged Wii request/build manifest receives the same scene ids as the Windows demo-disc request for the current authored menu scene set.

Then verify the downstream effect with existing or lightly extended Wii packaged workspace tests:

- Wii packaged manifest contains the expected scenes
- Wii packaged staging includes the expected `cooked/scenes/*` files
- Wii runtime scene manifest includes those scene entries

## Verification

Implementation will be considered complete when:

1. a focused build-system test fails before the change because Wii drops scenes that Windows keeps
2. the focused test passes after the change
3. packaged Wii builder tests still pass
4. a fresh Wii ISO contains the required scene set
5. selecting a scene from the packaged Wii main menu actually loads it in Dolphin
