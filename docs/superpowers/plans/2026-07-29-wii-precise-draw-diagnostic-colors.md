# Wii Precise Draw Diagnostic Colors Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Wii hardware diagnostic WBFS that maps every remaining generated draw boundary through `C10A` and gives each draw code a distinct background color.

**Architecture:** Authored engine `Core.cs` records portable FPS/debug transition markers that normal code generation emits into Wii C++. The Wii host refines caught exceptions from generated and native stage state, while `WiiFailureScreen` owns a pure code-to-GX-color palette used by failure presentation.

**Tech Stack:** C#/.NET 9, xUnit, C++20, devkitPPC/libogc, generated Helengine C++, Docker Wii toolchain, Wiimms ISO Tools, Dolphin.

---

## File Structure

- Modify `C:/dev/helworks/helengine/engine/helengine.core/Core.cs`: authored portable transition markers before FPS and debug render counters.
- Create `C:/dev/helworks/helengine/engine/helengine.editor.tests/CoreDrawStageSourceTests.cs`: source contract proving marker order without touching generated output.
- Modify `src/platform/wii/WiiFailureCode.hpp`: define `C105` through `C10A`.
- Modify `src/platform/wii/WiiApplication.cpp`: refine remaining generated draw stages and request code-specific failure colors.
- Modify `src/platform/wii/WiiFailureScreen.hpp`: expose the presentation-layer color resolver.
- Modify `src/platform/wii/WiiFailureScreen.cpp`: map approved draw codes to exact GX colors.
- Modify `builder.tests/WiiRuntimeSourceTests.cs`: guard mappings, precedence, colors, and successful black presentation.

### Task 1: Add authored FPS and debug draw markers

**Files:**
- Modify: `C:/dev/helworks/helengine/engine/helengine.core/Core.cs`
- Create: `C:/dev/helworks/helengine/engine/helengine.editor.tests/CoreDrawStageSourceTests.cs`

- [ ] **Step 1: Write the failing authored-source test**

Create one documented test class with this test:

```csharp
using Xunit;

namespace helengine.editor.tests;

/// <summary>
/// Verifies authored draw-stage markers that platform hosts consume after normal code generation.
/// </summary>
public sealed class CoreDrawStageSourceTests {
    /// <summary>
    /// Ensures FPS and debug frame counters each have a distinct transition marker without patching generated output.
    /// </summary>
    [Fact]
    public void Draw_RecordsDistinctMarkersBeforeFpsAndDebugFrameCounters() {
        string engineRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string coreSource = File.ReadAllText(Path.Combine(engineRootPath, "helengine.core", "Core.cs"));

        int fpsMarkerIndex = coreSource.IndexOf("LastSceneTransitionStage = \"BeforeFpsRenderFrame\";", StringComparison.Ordinal);
        int fpsCallIndex = coreSource.IndexOf("FPSComponent.RecordRenderFrame();", StringComparison.Ordinal);
        int debugMarkerIndex = coreSource.IndexOf("LastSceneTransitionStage = \"BeforeDebugRenderFrame\";", StringComparison.Ordinal);
        int debugCallIndex = coreSource.IndexOf("DebugComponent.RecordRenderFrame();", StringComparison.Ordinal);

        Assert.True(fpsMarkerIndex >= 0 && fpsCallIndex > fpsMarkerIndex);
        Assert.True(debugMarkerIndex > fpsCallIndex && debugCallIndex > debugMarkerIndex);
    }
}
```

- [ ] **Step 2: Run the focused engine test and verify it fails**

```powershell
dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~CoreDrawStageSourceTests --verbosity minimal
```

Expected: FAIL because both authored transition strings are absent.

- [ ] **Step 3: Add the authored markers**

In `Core.Draw()`, retain the existing calls and feature guard while adding:

```csharp
LastSceneTransitionStage = "BeforeFpsRenderFrame";
FPSComponent.RecordRenderFrame();
#if !HELENGINE_CODEGEN_FEATURE_DISABLED_DEBUG_OVERLAY
LastSceneTransitionStage = "BeforeDebugRenderFrame";
DebugComponent.RecordRenderFrame();
#endif
```

- [ ] **Step 4: Run the focused engine test and verify it passes**

Run the Step 2 command.

Expected: PASS, 1 test.

- [ ] **Step 5: Commit only the authored engine files**

```powershell
git -C C:\dev\helworks\helengine add -- engine/helengine.core/Core.cs engine/helengine.editor.tests/CoreDrawStageSourceTests.cs
git -C C:\dev\helworks\helengine commit -m "Add precise core draw stage markers"
```

Do not stage or alter the pre-existing physics, utility, plan, or specification changes in the engine worktree.

### Task 2: Map remaining generated draw stages

**Files:**
- Modify: `src/platform/wii/WiiFailureCode.hpp`
- Modify: `src/platform/wii/WiiApplication.cpp`
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing generated-stage mapping test**

Add `DrawFailureDiagnostic_MapsRemainingGeneratedDrawStages` and assert:

```csharp
Assert.Contains("DrawSetup = 0xC105U", failureCodeSource, StringComparison.Ordinal);
Assert.Contains("FrameBoundaryBookkeeping = 0xC106U", failureCodeSource, StringComparison.Ordinal);
Assert.Contains("RenderManagerBoundary = 0xC107U", failureCodeSource, StringComparison.Ordinal);
Assert.Contains("PostRenderMetrics = 0xC108U", failureCodeSource, StringComparison.Ordinal);
Assert.Contains("FpsRenderFrame = 0xC109U", failureCodeSource, StringComparison.Ordinal);
Assert.Contains("DebugRenderFrame = 0xC10AU", failureCodeSource, StringComparison.Ordinal);
Assert.Contains("sceneTransitionStage == \"DrawBegin\"", applicationSource, StringComparison.Ordinal);
Assert.Contains("sceneTransitionStage == \"CompleteFrameBoundaryCommitEnd\"", applicationSource, StringComparison.Ordinal);
Assert.Contains("sceneTransitionStage == \"AfterCompleteFrameBoundary\"", applicationSource, StringComparison.Ordinal);
Assert.Contains("sceneTransitionStage == \"BeforeRenderManager3DDraw\"", applicationSource, StringComparison.Ordinal);
Assert.Contains("sceneTransitionStage == \"AfterRenderManager3DDraw\"", applicationSource, StringComparison.Ordinal);
Assert.Contains("sceneTransitionStage == \"BeforeFpsRenderFrame\"", applicationSource, StringComparison.Ordinal);
Assert.Contains("sceneTransitionStage == \"BeforeDebugRenderFrame\"", applicationSource, StringComparison.Ordinal);
```

Also assert that the `GetLastDrawStage()` switch appears before the `DrawBegin` mapping so `C102`-`C104` retain precedence over `C107`.

- [ ] **Step 2: Run the focused mapping test and verify it fails**

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter FullyQualifiedName~DrawFailureDiagnostic_MapsRemainingGeneratedDrawStages --verbosity minimal
```

Expected: FAIL because `C105` through `C10A` are absent.

- [ ] **Step 3: Implement the remaining mappings**

Add documented enum values `DrawSetup`, `FrameBoundaryBookkeeping`, `RenderManagerBoundary`, `PostRenderMetrics`, `FpsRenderFrame`, and `DebugRenderFrame`. After the existing scene and native-stage mappings, use one formatted `if / else if` chain:

```cpp
if (sceneTransitionStage == "DrawBegin") {
    SetFailureCheckpoint(WiiFailureCode::DrawSetup);
} else if (sceneTransitionStage == "CompleteFrameBoundaryCommitEnd"
    || sceneTransitionStage == "AfterCompleteFrameBoundary") {
    SetFailureCheckpoint(WiiFailureCode::FrameBoundaryBookkeeping);
} else if (sceneTransitionStage == "BeforeRenderManager3DDraw") {
    SetFailureCheckpoint(WiiFailureCode::RenderManagerBoundary);
} else if (sceneTransitionStage == "AfterRenderManager3DDraw") {
    SetFailureCheckpoint(WiiFailureCode::PostRenderMetrics);
} else if (sceneTransitionStage == "BeforeFpsRenderFrame") {
    SetFailureCheckpoint(WiiFailureCode::FpsRenderFrame);
} else if (sceneTransitionStage == "BeforeDebugRenderFrame") {
    SetFailureCheckpoint(WiiFailureCode::DebugRenderFrame);
}
```

Each native-stage switch case must return after setting its code so generated-stage mapping cannot overwrite it.

- [ ] **Step 4: Run all draw mapping tests and verify they pass**

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~DrawFailureDiagnostic_" --verbosity minimal
```

Expected: all selected draw diagnostic tests pass.

- [ ] **Step 5: Commit the generated-stage mappings**

```powershell
git add -- builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiFailureCode.hpp src/platform/wii/WiiApplication.cpp
git commit -m "Map remaining Wii core draw stages"
```

### Task 3: Resolve code-specific failure backgrounds

**Files:**
- Modify: `src/platform/wii/WiiFailureScreen.hpp`
- Modify: `src/platform/wii/WiiFailureScreen.cpp`
- Modify: `src/platform/wii/WiiApplication.cpp`
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing palette and presentation test**

Add `DrawFailureDiagnostic_UsesDistinctCodeSpecificBackgrounds` and assert the public declaration:

```csharp
Assert.Contains("static GXColor ResolveBackgroundColor(WiiFailureCode code);", failureScreenHeaderSource, StringComparison.Ordinal);
```

Normalize the source with `ReplaceLineEndings("\n")`, then assert every exact case and return pair:

```csharp
string normalizedFailureScreenSource = failureScreenSource.ReplaceLineEndings("\n");

Assert.Contains("case WiiFailureCode::CoreDraw:\n                return GXColor { 0xC0, 0x40, 0x00, 0xFF };", normalizedFailureScreenSource, StringComparison.Ordinal);
Assert.Contains("case WiiFailureCode::SceneCommit:\n                return GXColor { 0x60, 0x20, 0x90, 0xFF };", normalizedFailureScreenSource, StringComparison.Ordinal);
Assert.Contains("case WiiFailureCode::OverlayCapture:\n                return GXColor { 0x00, 0x30, 0xA0, 0xFF };", normalizedFailureScreenSource, StringComparison.Ordinal);
Assert.Contains("case WiiFailureCode::FramePlanBuild:\n                return GXColor { 0x00, 0x70, 0x60, 0xFF };", normalizedFailureScreenSource, StringComparison.Ordinal);
Assert.Contains("case WiiFailureCode::RasterSubmission:\n                return GXColor { 0xA0, 0x00, 0x60, 0xFF };", normalizedFailureScreenSource, StringComparison.Ordinal);
Assert.Contains("case WiiFailureCode::DrawSetup:\n                return GXColor { 0x70, 0x30, 0x10, 0xFF };", normalizedFailureScreenSource, StringComparison.Ordinal);
Assert.Contains("case WiiFailureCode::FrameBoundaryBookkeeping:\n                return GXColor { 0x60, 0x60, 0x00, 0xFF };", normalizedFailureScreenSource, StringComparison.Ordinal);
Assert.Contains("case WiiFailureCode::RenderManagerBoundary:\n                return GXColor { 0x10, 0x20, 0x70, 0xFF };", normalizedFailureScreenSource, StringComparison.Ordinal);
Assert.Contains("case WiiFailureCode::PostRenderMetrics:\n                return GXColor { 0x00, 0x70, 0x90, 0xFF };", normalizedFailureScreenSource, StringComparison.Ordinal);
Assert.Contains("case WiiFailureCode::FpsRenderFrame:\n                return GXColor { 0x00, 0x70, 0x20, 0xFF };", normalizedFailureScreenSource, StringComparison.Ordinal);
Assert.Contains("case WiiFailureCode::DebugRenderFrame:\n                return GXColor { 0x70, 0x20, 0xA0, 0xFF };", normalizedFailureScreenSource, StringComparison.Ordinal);
```

Assert `ResolvePresentedClearColor()` checks `FailureActive` and returns `WiiFailureScreen::ResolveBackgroundColor(FailureCode)` before its generated-frame clear-color logic.

- [ ] **Step 2: Run the palette test and verify it fails**

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter FullyQualifiedName~DrawFailureDiagnostic_UsesDistinctCodeSpecificBackgrounds --verbosity minimal
```

Expected: FAIL because the color resolver does not exist.

- [ ] **Step 3: Implement the presentation-owned palette**

Add the documented public resolver to `WiiFailureScreen`. Implement a switch returning the approved `GXColor` for `CoreDraw` and `SceneCommit` through `DebugRenderFrame`; return the existing opaque red `{ 0xFF, 0x00, 0x00, 0xFF }` for every other code.

At the start of `WiiApplication::ResolvePresentedClearColor()` add:

```cpp
if (FailureActive) {
    return WiiFailureScreen::ResolveBackgroundColor(FailureCode);
}
```

- [ ] **Step 4: Run mapping, palette, and persistent-failure tests**

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~DrawFailureDiagnostic_|FullyQualifiedName~FailureScreen_LatchesRuntimeFailuresUntilShutdown" --verbosity minimal
```

Expected: all selected tests pass.

- [ ] **Step 5: Commit the code-specific palette**

```powershell
git add -- builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiFailureScreen.hpp src/platform/wii/WiiFailureScreen.cpp src/platform/wii/WiiApplication.cpp
git commit -m "Color Wii draw diagnostics by code"
```

### Task 4: Build and verify the colored hardware diagnostic

**Files:**
- Build output: `C:/dev/helprojs/demodisc/output/wii-colored-draw-diagnostic-20260729/game.iso`
- Build output: `C:/dev/helprojs/demodisc/output/wii-colored-draw-diagnostic-20260729/RCIE01.wbfs`

- [ ] **Step 1: Run the focused engine and Wii regressions**

Run the Task 1 engine test and the Task 3 selected Wii tests. Both commands must pass before building.

- [ ] **Step 2: Build through the normal packaged Wii pipeline**

Run `C:/dev/helworks/helengine/scripts/build-platform.ps1` with project `C:/dev/helprojs/demodisc/project.heproj`, platform `wii`, and the fresh output directory above. Wait until `wii-build-phase.txt` ends with `packaged outputs verified`.

- [ ] **Step 3: Verify generated markers without rewriting generated output**

Locate the build's workspace-owned generated-core directory from the build process/log, then run:

```powershell
rg -n "BeforeFpsRenderFrame|BeforeDebugRenderFrame" <generated-core>/Core.cpp
```

Expected: both authored marker strings are present in generated `Core.cpp` before their respective counter calls.

- [ ] **Step 4: Derive and verify the matched WBFS**

Use `tmp/tools/wit-v3.05a-r8638-cygwin64/bin/wit.exe` to verify `game.iso`, copy it to `RCIE01.wbfs` with `--wbfs --overwrite`, and verify the WBFS. Both must report `+OK`, `RCIE01`, IOS56, and one DATA partition.

- [ ] **Step 5: Launch the ISO in Dolphin without screenshots**

Use `scripts/launch_in_emulator.ps1`. Confirm Dolphin remains responsive and its runtime trace shows engine initialization, startup-scene queueing, scene commits, disc asset reads, and repeated rendered frames.

- [ ] **Step 6: Record hashes and repository states**

Record SHA-256 for the ISO, WBFS, and native DOL. Confirm the Wii repository contains only the user's two pre-existing untracked audio files, and confirm the engine repository still contains all pre-existing unrelated changes while the authored marker commit contains only `Core.cs` and `CoreDrawStageSourceTests.cs`.
