# Wii Draw-Stage Hardware Diagnostic Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Wii diagnostic WBFS that replaces ambiguous `C002` failures with persistent `C101`-`C104` draw-stage codes without changing loader or rendering behavior.

**Architecture:** A Wii-owned enum records the current native render-manager substage. `WiiRenderManager3D` updates it before overlay capture, frame-plan construction, and GX submission; `WiiApplication` combines that state with the generated core's existing scene-transition stage when an exception escapes `EngineCore->Draw()`.

**Tech Stack:** C++20, devkitPPC/libogc, generated Helengine C++, xUnit source-contract tests, Docker Wii toolchain, Wiimms ISO Tools, Dolphin.

---

## File Structure

- Create `src/platform/wii/WiiDrawStage.hpp`: Wii-native draw substages shared by the renderer and application.
- Modify `src/platform/wii/WiiRenderManager3D.hpp`: store and expose the last entered draw stage.
- Modify `src/platform/wii/WiiRenderManager3D.cpp`: update the stage immediately before each risky draw operation.
- Modify `src/platform/wii/WiiFailureCode.hpp`: define `C101` through `C104`.
- Modify `src/platform/wii/WiiApplication.hpp`: declare failure-checkpoint refinement.
- Modify `src/platform/wii/WiiApplication.cpp`: map scene/native draw stages after caught draw exceptions.
- Modify `builder.tests/WiiRuntimeSourceTests.cs`: guard the complete visible diagnostic contract.

### Task 1: Add the native draw-stage contract

**Files:**
- Create: `src/platform/wii/WiiDrawStage.hpp`
- Modify: `src/platform/wii/WiiRenderManager3D.hpp`
- Modify: `src/platform/wii/WiiRenderManager3D.cpp`
- Test: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing renderer-stage source test**

Add a test named `DrawFailureDiagnostic_TracksNativeRendererSubstages` that reads the three native files and asserts this contract:

```csharp
Assert.Contains("enum class WiiDrawStage : uint8_t", drawStageHeaderSource, StringComparison.Ordinal);
Assert.Contains("OverlayCapture", drawStageHeaderSource, StringComparison.Ordinal);
Assert.Contains("FramePlanBuild", drawStageHeaderSource, StringComparison.Ordinal);
Assert.Contains("RasterSubmission", drawStageHeaderSource, StringComparison.Ordinal);
Assert.Contains("WiiDrawStage GetLastDrawStage() const;", renderManagerHeaderSource, StringComparison.Ordinal);
Assert.Contains("WiiDrawStage LastDrawStage;", renderManagerHeaderSource, StringComparison.Ordinal);
Assert.Contains("LastDrawStage = WiiDrawStage::OverlayCapture;", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("LastDrawStage = WiiDrawStage::FramePlanBuild;", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("LastDrawStage = WiiDrawStage::RasterSubmission;", renderManagerSource, StringComparison.Ordinal);
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run:

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter FullyQualifiedName~DrawFailureDiagnostic_TracksNativeRendererSubstages --verbosity minimal
```

Expected: FAIL because `WiiDrawStage.hpp` and the stage members do not exist.

- [ ] **Step 3: Implement the draw-stage enum and renderer tracking**

Create the enum with `None`, `OverlayCapture`, `FramePlanBuild`, and `RasterSubmission`. Initialize `LastDrawStage` to `None`, update it immediately before each corresponding operation in `WiiRenderManager3D::Draw()`, and expose it with:

```cpp
WiiDrawStage WiiRenderManager3D::GetLastDrawStage() const {
    return LastDrawStage;
}
```

Every enum and member receives a substantive documentation comment.

- [ ] **Step 4: Run the focused test and verify it passes**

Run the Step 2 command.

Expected: PASS, 1 test.

- [ ] **Step 5: Commit the renderer stage contract**

```powershell
git add -- builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiDrawStage.hpp src/platform/wii/WiiRenderManager3D.hpp src/platform/wii/WiiRenderManager3D.cpp
git commit -m "Track Wii native draw stages"
```

### Task 2: Translate caught draw failures into visible subcodes

**Files:**
- Modify: `src/platform/wii/WiiFailureCode.hpp`
- Modify: `src/platform/wii/WiiApplication.hpp`
- Modify: `src/platform/wii/WiiApplication.cpp`
- Test: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing visible-code mapping test**

Add `DrawFailureDiagnostic_MapsCaughtFailuresToPersistentVisibleCodes` and assert:

```csharp
Assert.Contains("SceneCommit = 0xC101U", failureCodeSource, StringComparison.Ordinal);
Assert.Contains("OverlayCapture = 0xC102U", failureCodeSource, StringComparison.Ordinal);
Assert.Contains("FramePlanBuild = 0xC103U", failureCodeSource, StringComparison.Ordinal);
Assert.Contains("RasterSubmission = 0xC104U", failureCodeSource, StringComparison.Ordinal);
Assert.Contains("void RefineDrawFailureCheckpoint();", applicationHeaderSource, StringComparison.Ordinal);
Assert.Contains("void WiiApplication::RefineDrawFailureCheckpoint()", applicationSource, StringComparison.Ordinal);
Assert.Contains("get_LastSceneTransitionStage()", applicationSource, StringComparison.Ordinal);
Assert.Contains("WiiDrawStage::OverlayCapture", applicationSource, StringComparison.Ordinal);
Assert.Contains("WiiDrawStage::FramePlanBuild", applicationSource, StringComparison.Ordinal);
Assert.Contains("WiiDrawStage::RasterSubmission", applicationSource, StringComparison.Ordinal);
Assert.Equal(3, applicationSource.Split("RefineDrawFailureCheckpoint();", StringSplitOptions.None).Length - 1);
```

The expected count covers one call in each of the three catch blocks; the method definition has no trailing semicolon.

- [ ] **Step 2: Run the focused mapping test and verify it fails**

Run:

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter FullyQualifiedName~DrawFailureDiagnostic_MapsCaughtFailuresToPersistentVisibleCodes --verbosity minimal
```

Expected: FAIL because the four codes and refinement method do not exist.

- [ ] **Step 3: Implement failure refinement**

Define the four documented codes in `WiiFailureCode`. Add `RefineDrawFailureCheckpoint()` to `WiiApplication`; it must:

1. Map `BeforeCompleteFrameBoundary` and `CompleteFrameBoundaryCommitBegin` to `SceneCommit`.
2. Otherwise map `EngineRenderManager3D->GetLastDrawStage()` to the corresponding native code.
3. Leave the existing `CoreDraw` (`C002`) checkpoint unchanged for `None` or unknown state.

Call the method at the start of each `EngineCore->Draw()` catch block, before entering failure presentation. Do not modify successful draw flow.

- [ ] **Step 4: Run both diagnostic tests and verify they pass**

Run:

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~DrawFailureDiagnostic_TracksNativeRendererSubstages|FullyQualifiedName~DrawFailureDiagnostic_MapsCaughtFailuresToPersistentVisibleCodes" --verbosity minimal
```

Expected: PASS, 2 tests.

- [ ] **Step 5: Commit visible checkpoint refinement**

```powershell
git add -- builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiFailureCode.hpp src/platform/wii/WiiApplication.hpp src/platform/wii/WiiApplication.cpp
git commit -m "Refine Wii draw failure codes"
```

### Task 3: Build and verify the hardware diagnostic

**Files:**
- Build output: `C:/dev/helprojs/demodisc/output/wii-draw-stage-diagnostic-20260729/game.iso`
- Build output: `C:/dev/helprojs/demodisc/output/wii-draw-stage-diagnostic-20260729/RCIE01.wbfs`

- [ ] **Step 1: Run the diagnostic source tests and existing failure-latch regression**

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~DrawFailureDiagnostic_|FullyQualifiedName~FailureScreen_LatchesRuntimeFailuresUntilShutdown|FullyQualifiedName~PackagedBootstrap_DeclaresPackagedDiscHelpers" --verbosity minimal
```

Expected: all selected tests pass.

- [ ] **Step 2: Build the packaged Wii ISO**

Run `C:/dev/helworks/helengine/scripts/build-platform.ps1` for project `C:/dev/helprojs/demodisc/project.heproj`, platform `wii`, and fresh output `C:/dev/helprojs/demodisc/output/wii-draw-stage-diagnostic-20260729`. Wait until `wii-build-phase.txt` ends with `packaged outputs verified`.

- [ ] **Step 3: Produce and verify the matched WBFS**

Use `tmp/tools/wit-v3.05a-r8638-cygwin64/bin/wit.exe`:

```powershell
wit verify --verbose game.iso
wit copy game.iso RCIE01.wbfs --wbfs --overwrite
wit verify --verbose RCIE01.wbfs
```

Expected: ISO and WBFS each report `+OK`, disc ID `RCIE01`, and one DATA partition.

- [ ] **Step 4: Launch the ISO in Dolphin and inspect runtime evidence**

```powershell
.\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\demodisc\output\wii-draw-stage-diagnostic-20260729\game.iso
```

Confirm Dolphin remains responsive and `runtime_registry_trace.txt` contains engine initialization, startup-scene queueing, scene commit, and rendered-frame entries. Do not take screenshots.

- [ ] **Step 5: Record hashes and repository state**

Record SHA-256 for `game.iso`, `RCIE01.wbfs`, and `native/helengine_wii.dol`. Confirm `git status --short` contains only the user's pre-existing untracked audio files.
