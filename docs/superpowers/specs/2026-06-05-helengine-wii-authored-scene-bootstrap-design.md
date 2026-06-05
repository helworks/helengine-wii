# Helengine Wii Authored Scene Bootstrap Design

## Goal

Create the next Wii bootstrap milestone after generated-core host boot: wire real authored-scene bootstrap inputs into Wii engine initialization while keeping the runtime target intentionally small.

Success for this milestone means Wii no longer initializes the engine core with a placeholder content root and null scene catalog. Instead, it resolves a staged content root, validates the required authored-scene files, constructs a runtime scene catalog, and keeps running in Dolphin with the current diagnostic rendering behavior.

## Constraints

- The implementation should stay structurally close to `helengine-gc`.
- This is a bootstrap milestone, not a rendering parity milestone.
- The step should be small and isolated.
- The staged content layout should mirror the GameCube host pattern.
- Failures should remain explicit. Missing or invalid bootstrap content must fail initialization instead of silently falling back to placeholder values.

## Chosen Approach

Mirror the GameCube scene-bootstrap structure inside a Wii-specific boundary.

The Wii host will gain a `WiiSceneBootstrap` class that owns staged content-root resolution, required-file validation, runtime scene-catalog creation, and startup-scene metadata. `WiiApplication` will remain the orchestrator and will stop hardcoding placeholder bootstrap values in `CoreInitializationOptions`.

This keeps the change small, keeps Wii code Wii-owned, and preserves a straightforward diff against the GameCube runtime path. It also avoids a premature shared Nintendo abstraction before Wii has enough real overlap to justify one.

## Architecture

### `WiiSceneBootstrap`

`WiiSceneBootstrap` is the only new runtime boundary in this slice.

It should provide:

- `GetValidatedContentRootPath()`
- `CreateSceneCatalog()`
- `GetStartupSceneId()`

Its responsibilities are:

- resolve a staged authored-content root using the mirrored GameCube host-path lookup pattern
- validate the required bootstrap files under that root
- create the runtime scene catalog for the startup authored scene
- expose the startup scene id for the next scene-activation slice or for immediate use if the current generated-core surface already supports it cleanly

### `WiiApplication`

`WiiApplication` should continue to own native initialization and engine lifecycle orchestration only.

Its scene-bootstrap responsibilities become:

- request validated bootstrap data from `WiiSceneBootstrap`
- assign that data into `CoreInitializationOptions`
- continue to initialize the current bridge services and engine core as it does today

`WiiApplication` should no longer supply placeholder bootstrap values such as `ContentRootPath = "."` or `SceneCatalog = nullptr` once this path is enabled.

### Render And Input Boundaries

`WiiRenderManager2D`, `WiiRenderManager3D`, and `WiiInputManager` stay as they are unless a narrow compatibility change is required for the bootstrap wiring to compile.

Visible authored rendering is outside this milestone.

## Data Flow

The engine initialization flow for this milestone is:

1. `WiiApplication::InitializeEngineCore()` constructs `Core` and reads `CoreInitializationOptions`.
2. `WiiApplication` calls `WiiSceneBootstrap::GetValidatedContentRootPath()`.
3. `WiiSceneBootstrap` resolves the content root using the mirrored GameCube-style fallback pattern:
   - repo-relative staged content path
   - Windows host absolute path
   - WSL absolute path if that mirrored path is still useful in the current workflow
4. `WiiSceneBootstrap` validates the minimum required authored-scene files under that root.
5. `WiiApplication` calls `WiiSceneBootstrap::CreateSceneCatalog()` and `WiiSceneBootstrap::GetStartupSceneId()`.
6. `WiiApplication` assigns the validated content root and runtime scene catalog into `CoreInitializationOptions`.
7. If the generated-core initialization surface already exposes a clean startup-scene hook, `WiiApplication` assigns the startup scene id in the same step. If not, the startup scene id is still provided by `WiiSceneBootstrap` and retained as the next activation milestone input.
8. `Core::Initialize(...)` runs with real bootstrap data instead of placeholders.
9. The normal update and draw loop continues unchanged.

## Staged Content Shape

The staged content bundle should mirror the GameCube layout pattern closely enough that later tooling can follow the same conventions:

- `cooked/scenes/...`
- supporting cooked assets under the same content root

The milestone should validate only the minimum authored-scene files needed to prove bootstrap correctness. It should not try to generalize packaging or builder behavior yet.

## Error Handling

Bootstrap failures should be explicit and early.

- `WiiSceneBootstrap` should throw when no valid staged content root can be resolved.
- `WiiSceneBootstrap` should throw when required authored-scene files are missing.
- `WiiApplication` should catch those failures during engine initialization, report the failing stage through `SYS_Report`, switch to the failure clear color, and stop normal initialization.

There should be no silent fallback to placeholder bootstrap values once the bootstrap path is enabled.

## Testing

This milestone should be verified in the smallest layers that prove the bootstrap plumbing is real.

### Build Verification

- The Wii generated-core Docker build with `HELENGINE_CORE_CPP_ROOT` set should still emit `build/helengine_wii.dol`.

### Failure Verification

- With staged authored content missing, Wii should fail during initialization with a clear bootstrap-stage report.
- It should not continue running with placeholder `ContentRootPath` or null `SceneCatalog` values.

### Success Verification

- With staged authored content present in the mirrored GameCube-style layout, Dolphin should boot the Wii `.dol`, initialize the engine core, and remain running.
- Visible authored rendering is not required for this milestone.

## Non-Goals

- visible authored-scene rendering parity with GameCube
- packaged-content or disc-style Wii bootstrap
- shared GC/Wii bootstrap extraction
- broad renderer or input upgrades unrelated to scene-bootstrap wiring
- builder or asset-cooking automation beyond whatever staged content already exists
