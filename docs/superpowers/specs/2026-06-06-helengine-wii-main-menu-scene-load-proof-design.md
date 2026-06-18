# Helengine Wii Main Menu Scene-Load Proof Design

## Goal

Prove that the authored Wii startup scene `Scenes/DemoDiscMainMenu.helen` loads through the current direct-`DOL` developer boot path in Dolphin, without taking on any visible UI rendering work in this slice.

## Scope

This slice is limited to runtime evidence for scene loading.

In scope:

- Wii-owned trace/log additions around startup-scene bootstrap and first scene/content load activity
- Dolphin verification using the existing direct-`DOL` generated-core path
- fallback acceptance that Dolphin executes the title and stays alive without crashing if richer tracing remains unavailable

Out of scope:

- 2D main-menu rendering
- interaction/navigation
- packaged-disc startup changes beyond what already exists
- broad generated-core tracing infrastructure refactors

## Approach

The Wii host already selects and queues `Scenes/DemoDiscMainMenu.helen`. The missing piece is durable evidence that the scene load progressed beyond queueing.

The runtime will gain narrow trace points in the existing Wii-owned startup path and, if needed, one minimal bridge into the scene/content load surface already exposed by the generated core. The implementation should prefer existing observable state before adding any new cross-cutting diagnostics.

The preferred proof chain is:

1. Wii bootstrap logs the startup scene id being queued.
2. Wii runtime records the first authored scene/content path requested during load.
3. Dolphin executes the title successfully on the direct-`DOL` path.

If step 2 cannot be obtained from the existing surfaces without invasive changes, the fallback proof is:

1. startup scene id queued
2. Dolphin executes `helengine_wii.dol`
3. Dolphin does not immediately crash or reset

## Implementation Boundaries

Expected touch points:

- `src/platform/wii/WiiApplication.cpp`
- possibly one or two Wii runtime source tests guarding the new trace contract
- only the smallest generated-core-facing surface needed to expose first scene/content request evidence, if Wii-owned logging alone is insufficient

The implementation must not add speculative rendering behavior or hide failures with best-effort fallbacks.

## Verification

Required verification:

- focused `builder.tests` for any new source-contract assertions
- Docker rebuild of the Wii generated-core binary
- Dolphin rerun against `build/helengine_wii.dol`

Success is satisfied by either:

- trace evidence that `Scenes/DemoDiscMainMenu.helen` and an expected cooked scene/content path were requested during startup, or
- failing that, evidence that Dolphin executed the title and the runtime remained alive without an immediate crash

## Risks

- Dolphin direct-`DOL` execution may not expose enough runtime logging by default.
- The existing generated-core scene/content services may not surface first-load state in a Wii-owned place yet.
- Overreaching into generalized tracing infrastructure would expand scope beyond a proof slice.
