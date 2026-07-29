# Wii On-Screen Failure Code Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep Wii startup black while displaying a four-character direct-framebuffer code on the existing red runtime failure screen.

**Architecture:** A dedicated `WiiFailureScreen` class writes fixed 3-by-5 hexadecimal glyphs directly into both packed YCbYCr external framebuffers after GX finishes copying the red failure frame. `WiiApplication` tracks a typed checkpoint at risky startup and first-frame boundaries, while video initialization explicitly clears newly allocated framebuffers before output is enabled.

**Tech Stack:** C++20, devkitPPC/libogc VI and GX APIs, C#/.NET 9 xUnit source-contract tests, Helengine platform build pipeline, Wiimms ISO Tools.

---

## File Map

- Create `src/platform/wii/WiiFailureCode.hpp`: readable typed mapping from runtime boundaries to four-digit hexadecimal codes.
- Create `src/platform/wii/WiiFailureScreen.hpp`: narrow direct-framebuffer overlay interface.
- Create `src/platform/wii/WiiFailureScreen.cpp`: packed YCbYCr background and 3-by-5 hexadecimal glyph writer.
- Modify `src/platform/wii/WiiApplication.hpp`: store the current checkpoint and whether failure presentation is active.
- Modify `src/platform/wii/WiiApplication.cpp`: assign checkpoints, invoke the overlay after `GX_DrawDone`, and clear both startup framebuffers.
- Modify `src/platform/wii/WiiBootHost.cpp`: clear its startup framebuffer before video output is enabled.
- Modify `Makefile`: compile `WiiFailureScreen.cpp` for generated-core and direct-DOL builds.
- Modify `builder.tests/WiiRuntimeSourceTests.cs`: lock the direct renderer, checkpoint mapping, call ordering, and deterministic black startup.

The existing uncommitted black boot-phase edits in `WiiApplication.cpp` and `WiiBootHost.cpp` are intentional user work. Preserve them and include them when the integrated behavior is committed. Do not add either temporary audio text file.

### Task 1: Add the direct-framebuffer hexadecimal renderer

**Files:**
- Create: `src/platform/wii/WiiFailureCode.hpp`
- Create: `src/platform/wii/WiiFailureScreen.hpp`
- Create: `src/platform/wii/WiiFailureScreen.cpp`
- Modify: `Makefile`
- Test: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing renderer source-contract test**

Add this test to `WiiRuntimeSourceTests`:

```csharp
/// <summary>
/// Ensures the Wii failure screen renders a fixed hexadecimal code directly into both external framebuffers without fonts, assets, or GX draw submission.
/// </summary>
[Fact]
public void FailureScreen_WritesHexCodeDirectlyIntoBothExternalFramebuffers() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string makefileSource = File.ReadAllText(Path.Combine(repositoryRootPath, "Makefile"));
    string headerPath = Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiFailureScreen.hpp");
    string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiFailureScreen.cpp");

    Assert.True(File.Exists(headerPath), "Expected WiiFailureScreen.hpp to exist.");
    Assert.True(File.Exists(sourcePath), "Expected WiiFailureScreen.cpp to exist.");

    string headerSource = File.ReadAllText(headerPath);
    string source = File.ReadAllText(sourcePath);
    Assert.Contains("class WiiFailureScreen", headerSource, StringComparison.Ordinal);
    Assert.Contains("static void WriteCode(const GXRModeObj* renderMode, void* const frameBuffers[2], WiiFailureCode code);", headerSource, StringComparison.Ordinal);
    Assert.Contains("constexpr uint32_t ForegroundColor = 0xEB80EB80U;", source, StringComparison.Ordinal);
    Assert.Contains("constexpr uint32_t BackgroundColor = 0x10801080U;", source, StringComparison.Ordinal);
    Assert.Contains("for (uint32_t frameBufferIndex = 0U; frameBufferIndex < 2U; frameBufferIndex++)", source, StringComparison.Ordinal);
    Assert.Contains("static constexpr uint8_t GlyphRows[16][5]", source, StringComparison.Ordinal);
    Assert.DoesNotContain("GX_Begin", source, StringComparison.Ordinal);
    Assert.DoesNotContain("CON_Init", source, StringComparison.Ordinal);
    Assert.Contains("$(SOURCE_DIR)/platform/wii/WiiFailureScreen.cpp", makefileSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```powershell
dotnet test builder.tests\helengine.wii.builder.tests.csproj -p:HelEngineRoot=C:\dev\helworks\helengine --filter "FullyQualifiedName~WiiRuntimeSourceTests.FailureScreen_WritesHexCodeDirectlyIntoBothExternalFramebuffers" -v minimal
```

Expected: FAIL because `WiiFailureScreen.hpp` and `WiiFailureScreen.cpp` do not exist.

- [ ] **Step 3: Add the typed failure-code mapping**

Create `src/platform/wii/WiiFailureCode.hpp` with:

```cpp
#pragma once

#include <cstdint>

namespace helengine::wii {
    /// Identifies the last risky Wii runtime boundary entered before a visible failure.
    enum class WiiFailureCode : uint16_t {
        /// No more specific runtime boundary was assigned.
        Unknown = 0xE000U,

        /// The generated core object was being constructed.
        CoreConstruction = 0xA001U,

        /// The generated core initialization options were being read.
        CoreOptions = 0xA002U,

        /// Packaged disc storage was being initialized.
        PackagedStorage = 0xA003U,

        /// Packaged content paths and the scene catalog were being configured.
        ContentConfiguration = 0xA004U,

        /// Platform bridge services were being constructed.
        BridgeConstruction = 0xA005U,

        /// The primary render window was being registered.
        PrimaryWindow = 0xA006U,

        /// The generated core initialization call was active.
        CoreInitialization = 0xA007U,

        /// Generated runtime modules were being registered.
        RuntimeModuleRegistration = 0xA008U,

        /// The packaged startup scene was being queued.
        SceneQueue = 0xB001U,

        /// The update path rejected invalid initialized state.
        UpdatePrecondition = 0xB002U,

        /// The 2D frame capture was beginning.
        BeginFrame = 0xB003U,

        /// The generated core update call was active.
        CoreUpdate = 0xB004U,

        /// Released render assets were being flushed after update.
        ReleaseFlush = 0xB005U,

        /// The draw path rejected invalid initialized state.
        DrawPrecondition = 0xC001U,

        /// The generated core draw call was active.
        CoreDraw = 0xC002U,

        /// Captured 2D commands were being submitted to the Wii renderer.
        RenderCapturedCommands = 0xC003U
    };
}
```

- [ ] **Step 4: Add the direct-framebuffer class interface**

Create `src/platform/wii/WiiFailureScreen.hpp` with:

```cpp
#pragma once

#include <cstdint>

#include <gccore.h>

#include "platform/wii/WiiFailureCode.hpp"

namespace helengine::wii {
    /// Draws a compact hexadecimal failure code directly into Wii external framebuffers.
    class WiiFailureScreen {
    public:
        /// Writes the supplied code into both external framebuffers without using GX drawing, fonts, or assets.
        static void WriteCode(const GXRModeObj* renderMode, void* const frameBuffers[2], WiiFailureCode code);

    private:
        /// Returns the three-bit pixel pattern for one row of a hexadecimal glyph.
        static uint8_t GetGlyphRow(uint8_t hexadecimalDigit, uint8_t row);
    };
}
```

- [ ] **Step 5: Implement the fixed packed-framebuffer glyph writer**

Create `src/platform/wii/WiiFailureScreen.cpp` with:

```cpp
#include "platform/wii/WiiFailureScreen.hpp"

namespace helengine::wii {
    namespace {
        constexpr uint32_t ForegroundColor = 0xEB80EB80U;
        constexpr uint32_t BackgroundColor = 0x10801080U;
        constexpr uint32_t HorizontalScale = 3U;
        constexpr uint32_t VerticalScale = 4U;
        constexpr uint32_t GlyphWidth = 3U;
        constexpr uint32_t GlyphHeight = 5U;
        constexpr uint32_t GlyphGap = 2U;
        constexpr uint32_t Margin = 8U;
        constexpr uint32_t DigitCount = 4U;
    }

    /// Writes the supplied code into both external framebuffers without using GX drawing, fonts, or assets.
    void WiiFailureScreen::WriteCode(const GXRModeObj* renderMode, void* const frameBuffers[2], WiiFailureCode code) {
        if (renderMode == nullptr || frameBuffers == nullptr || frameBuffers[0] == nullptr || frameBuffers[1] == nullptr) {
            return;
        }

        const uint32_t frameBufferWordWidth = static_cast<uint32_t>(renderMode->fbWidth) / 2U;
        const uint32_t frameBufferHeight = static_cast<uint32_t>(renderMode->xfbHeight);
        const uint32_t overlayWidth = (DigitCount * GlyphWidth * HorizontalScale) + ((DigitCount - 1U) * GlyphGap);
        const uint32_t overlayHeight = GlyphHeight * VerticalScale;
        if (frameBufferWordWidth <= Margin + overlayWidth || frameBufferHeight <= Margin + overlayHeight) {
            return;
        }

        const uint16_t numericCode = static_cast<uint16_t>(code);
        for (uint32_t frameBufferIndex = 0U; frameBufferIndex < 2U; frameBufferIndex++) {
            volatile uint32_t* const frameBufferWords = static_cast<volatile uint32_t*>(frameBuffers[frameBufferIndex]);
            for (uint32_t y = 0U; y < overlayHeight; y++) {
                volatile uint32_t* const row = frameBufferWords + ((Margin + y) * frameBufferWordWidth) + Margin;
                for (uint32_t x = 0U; x < overlayWidth; x++) {
                    row[x] = BackgroundColor;
                }
            }

            for (uint32_t digitIndex = 0U; digitIndex < DigitCount; digitIndex++) {
                const uint32_t shift = (DigitCount - 1U - digitIndex) * 4U;
                const uint8_t hexadecimalDigit = static_cast<uint8_t>((numericCode >> shift) & 0x0FU);
                const uint32_t digitStartX = Margin + (digitIndex * ((GlyphWidth * HorizontalScale) + GlyphGap));
                for (uint32_t glyphRow = 0U; glyphRow < GlyphHeight; glyphRow++) {
                    const uint8_t glyphBits = GetGlyphRow(hexadecimalDigit, static_cast<uint8_t>(glyphRow));
                    for (uint32_t verticalOffset = 0U; verticalOffset < VerticalScale; verticalOffset++) {
                        volatile uint32_t* const row = frameBufferWords + ((Margin + (glyphRow * VerticalScale) + verticalOffset) * frameBufferWordWidth) + digitStartX;
                        for (uint32_t glyphColumn = 0U; glyphColumn < GlyphWidth; glyphColumn++) {
                            const bool foreground = (glyphBits & (1U << (GlyphWidth - 1U - glyphColumn))) != 0U;
                            const uint32_t pixelColor = foreground ? ForegroundColor : BackgroundColor;
                            for (uint32_t horizontalOffset = 0U; horizontalOffset < HorizontalScale; horizontalOffset++) {
                                row[(glyphColumn * HorizontalScale) + horizontalOffset] = pixelColor;
                            }
                        }
                    }
                }
            }
        }
    }

    /// Returns the three-bit pixel pattern for one row of a hexadecimal glyph.
    uint8_t WiiFailureScreen::GetGlyphRow(uint8_t hexadecimalDigit, uint8_t row) {
        static constexpr uint8_t GlyphRows[16][5] = {
            { 0x07U, 0x05U, 0x05U, 0x05U, 0x07U },
            { 0x02U, 0x06U, 0x02U, 0x02U, 0x07U },
            { 0x07U, 0x01U, 0x07U, 0x04U, 0x07U },
            { 0x07U, 0x01U, 0x07U, 0x01U, 0x07U },
            { 0x05U, 0x05U, 0x07U, 0x01U, 0x01U },
            { 0x07U, 0x04U, 0x07U, 0x01U, 0x07U },
            { 0x07U, 0x04U, 0x07U, 0x05U, 0x07U },
            { 0x07U, 0x01U, 0x02U, 0x02U, 0x02U },
            { 0x07U, 0x05U, 0x07U, 0x05U, 0x07U },
            { 0x07U, 0x05U, 0x07U, 0x01U, 0x07U },
            { 0x02U, 0x05U, 0x07U, 0x05U, 0x05U },
            { 0x06U, 0x05U, 0x06U, 0x05U, 0x06U },
            { 0x07U, 0x04U, 0x04U, 0x04U, 0x07U },
            { 0x06U, 0x05U, 0x05U, 0x05U, 0x06U },
            { 0x07U, 0x04U, 0x07U, 0x04U, 0x07U },
            { 0x07U, 0x04U, 0x07U, 0x04U, 0x04U }
        };
        return hexadecimalDigit < 16U && row < 5U ? GlyphRows[hexadecimalDigit][row] : 0U;
    }
}
```

- [ ] **Step 6: Compile the new source in every Wii host build**

Add the failure-screen source to `BASE_SOURCES`:

```make
BASE_SOURCES := \
	$(SOURCE_DIR)/main.cpp \
	$(SOURCE_DIR)/platform/wii/WiiApplication.cpp \
	$(SOURCE_DIR)/platform/wii/WiiFailureScreen.cpp
```

- [ ] **Step 7: Run the focused test and verify GREEN**

Run the Step 2 command again.

Expected: PASS with one matching test.

- [ ] **Step 8: Commit the renderer unit**

```powershell
git add -- Makefile builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiFailureCode.hpp src/platform/wii/WiiFailureScreen.hpp src/platform/wii/WiiFailureScreen.cpp
git commit -m "Add Wii direct framebuffer failure codes"
```

### Task 2: Integrate checkpoints and deterministic black startup

**Files:**
- Modify: `src/platform/wii/WiiApplication.hpp`
- Modify: `src/platform/wii/WiiApplication.cpp`
- Modify: `src/platform/wii/WiiBootHost.cpp`
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing integration source-contract tests**

Add these tests to `WiiRuntimeSourceTests`:

```csharp
/// <summary>
/// Ensures runtime failures retain the last typed checkpoint and overlay it only after GX has completed the red display copy.
/// </summary>
[Fact]
public void FailureScreen_UsesTrackedCheckpointOnlyAfterFailureDisplayCopyCompletes() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.hpp"));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));
    string failureCodeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiFailureCode.hpp"));

    Assert.Contains("WiiFailureCode FailureCode;", applicationHeaderSource, StringComparison.Ordinal);
    Assert.Contains("bool FailureActive;", applicationHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void SetFailureCheckpoint(WiiFailureCode code);", applicationHeaderSource, StringComparison.Ordinal);
    Assert.Contains("FailureCode(WiiFailureCode::Unknown)", applicationSource, StringComparison.Ordinal);
    Assert.Contains("FailureActive(false)", applicationSource, StringComparison.Ordinal);
    Assert.Contains("FailureActive = true;", applicationSource, StringComparison.Ordinal);
    Assert.Contains("SetFailureCheckpoint(WiiFailureCode::PackagedStorage);", applicationSource, StringComparison.Ordinal);
    Assert.Contains("SetFailureCheckpoint(WiiFailureCode::CoreUpdate);", applicationSource, StringComparison.Ordinal);
    Assert.Contains("SetFailureCheckpoint(WiiFailureCode::CoreDraw);", applicationSource, StringComparison.Ordinal);
    Assert.Contains("PackagedStorage = 0xA003U", failureCodeSource, StringComparison.Ordinal);
    Assert.Contains("CoreUpdate = 0xB004U", failureCodeSource, StringComparison.Ordinal);
    Assert.Contains("CoreDraw = 0xC002U", failureCodeSource, StringComparison.Ordinal);

    int drawDoneIndex = applicationSource.IndexOf("GX_DrawDone();", StringComparison.Ordinal);
    int failureGuardIndex = applicationSource.IndexOf("if (FailureActive) {", drawDoneIndex, StringComparison.Ordinal);
    int overlayIndex = applicationSource.IndexOf("WiiFailureScreen::WriteCode(RenderMode, FrameBuffers, FailureCode);", failureGuardIndex, StringComparison.Ordinal);
    Assert.True(drawDoneIndex >= 0 && failureGuardIndex > drawDoneIndex && overlayIndex > failureGuardIndex);
}

/// <summary>
/// Ensures Wii video output never exposes newly allocated framebuffer memory before a deliberate black clear.
/// </summary>
[Fact]
public void FailureScreen_ClearsAllStartupFramebuffersBeforeEnablingVideoOutput() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));
    string bootHostSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiBootHost.cpp"));

    int applicationFirstClearIndex = applicationSource.IndexOf("VIDEO_ClearFrameBuffer(RenderMode, FrameBuffers[0], COLOR_BLACK);", StringComparison.Ordinal);
    int applicationSecondClearIndex = applicationSource.IndexOf("VIDEO_ClearFrameBuffer(RenderMode, FrameBuffers[1], COLOR_BLACK);", StringComparison.Ordinal);
    int applicationEnableIndex = applicationSource.IndexOf("VIDEO_SetBlack(FALSE);", StringComparison.Ordinal);
    Assert.True(applicationFirstClearIndex >= 0 && applicationSecondClearIndex > applicationFirstClearIndex && applicationEnableIndex > applicationSecondClearIndex);

    int bootHostClearIndex = bootHostSource.IndexOf("VIDEO_ClearFrameBuffer(RenderMode, FrameBuffer, COLOR_BLACK);", StringComparison.Ordinal);
    int bootHostEnableIndex = bootHostSource.IndexOf("VIDEO_SetBlack(FALSE);", StringComparison.Ordinal);
    Assert.True(bootHostClearIndex >= 0 && bootHostEnableIndex > bootHostClearIndex);
}
```

- [ ] **Step 2: Run the two integration tests and verify RED**

Run:

```powershell
dotnet test builder.tests\helengine.wii.builder.tests.csproj -p:HelEngineRoot=C:\dev\helworks\helengine --filter "FullyQualifiedName~WiiRuntimeSourceTests.FailureScreen_UsesTrackedCheckpointOnlyAfterFailureDisplayCopyCompletes|FullyQualifiedName~WiiRuntimeSourceTests.FailureScreen_ClearsAllStartupFramebuffersBeforeEnablingVideoOutput" -v minimal
```

Expected: both tests FAIL because the application does not yet track a failure code, invoke the overlay, or clear the startup framebuffers.

- [ ] **Step 3: Add failure state to `WiiApplication`**

Include the typed code in `WiiApplication.hpp`:

```cpp
#include "platform/wii/WiiBootPhase.hpp"
#include "platform/wii/WiiFailureCode.hpp"
```

Add the checkpoint setter after `FailBootPhase`:

```cpp
/// Records the runtime boundary that should be shown if the current operation fails.
void SetFailureCheckpoint(WiiFailureCode code);
```

Add these fields after `BootPhase`:

```cpp
/// Stores the last risky runtime boundary entered for direct failure reporting.
WiiFailureCode FailureCode;

/// Tracks whether presentation should overlay the current failure code.
bool FailureActive;
```

Initialize them in `WiiApplication.cpp`:

```cpp
, BootPhase(WiiBootPhase::NativeStartup)
, FailureCode(WiiFailureCode::Unknown)
, FailureActive(false)
, EngineInitialized(false)
```

Include the renderer:

```cpp
#include "platform/wii/WiiFailureScreen.hpp"
```

Mark failure and implement the setter:

```cpp
/// Marks the current boot phase as failed and updates the visible clear color.
void WiiApplication::FailBootPhase(WiiBootPhase phase, GXColor color) {
    BootPhase = phase;
    ClearColor = color;
    FailureActive = true;
}

/// Records the runtime boundary that should be shown if the current operation fails.
void WiiApplication::SetFailureCheckpoint(WiiFailureCode code) {
    FailureCode = code;
}
```

- [ ] **Step 4: Assign checkpoints immediately before risky operations**

Add these assignments in `InitializeEngineCore`, preserving the existing black `SetBootPhase` calls:

```cpp
SetFailureCheckpoint(WiiFailureCode::CoreConstruction);
EngineCore = new Core();

SetFailureCheckpoint(WiiFailureCode::CoreOptions);
CoreInitializationOptions* options = EngineCore->get_InitializationOptions();

SetFailureCheckpoint(WiiFailureCode::PackagedStorage);
if (!WiiSceneBootstrap::InitializePackagedStorage()) {
```

Set `ContentConfiguration` before resolving content roots, creating the content stream source, creating the scene catalog, and reading the startup scene id in both packaged and direct-DOL branches. Then assign:

```cpp
SetFailureCheckpoint(WiiFailureCode::BridgeConstruction);
EngineRenderManager3D = new WiiRenderManager3D();

SetFailureCheckpoint(WiiFailureCode::PrimaryWindow);
EngineRenderManager3D->AddWindow(0, logicalFrameWidth, logicalFrameHeight);

SetFailureCheckpoint(WiiFailureCode::CoreInitialization);
EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputManager, EnginePlatformInfo, options);

#if HELENGINE_WII_HAS_GENERATED_RUNTIME_MODULE_REGISTRATION
SetFailureCheckpoint(WiiFailureCode::RuntimeModuleRegistration);
RegisterGeneratedRuntimeModules(EngineCore);
#endif
```

Before validating and queueing the startup scene, assign:

```cpp
SetFailureCheckpoint(WiiFailureCode::SceneQueue);
```

In `UpdateEngineCore`, assign `UpdatePrecondition` before the initial validity check, `BeginFrame` before `EngineRenderManager2D->BeginFrame()`, `CoreUpdate` before `EngineCore->Update(1.0 / 60.0)`, and `ReleaseFlush` before the released texture and asset flushes.

In `DrawEngineCore`, assign `DrawPrecondition` before the initial validity check, `CoreDraw` before `EngineCore->Draw()`, and `RenderCapturedCommands` before `EngineRenderManager2D->RenderCapturedCommands(...)`.

- [ ] **Step 5: Overlay the code only after a completed failed display copy**

Extend `PresentFrame` immediately after `GX_DrawDone()`:

```cpp
GX_CopyDisp(FrameBuffers[FrameBufferIndex], GX_TRUE);
GX_DrawDone();
if (FailureActive) {
    WiiFailureScreen::WriteCode(RenderMode, FrameBuffers, FailureCode);
}
VIDEO_SetNextFramebuffer(FrameBuffers[FrameBufferIndex]);
```

- [ ] **Step 6: Clear allocated framebuffers before enabling video**

In `WiiApplication::InitializeVideo`, add these calls after successful allocation and before `VIDEO_Configure`:

```cpp
VIDEO_ClearFrameBuffer(RenderMode, FrameBuffers[0], COLOR_BLACK);
VIDEO_ClearFrameBuffer(RenderMode, FrameBuffers[1], COLOR_BLACK);
```

In `WiiBootHost::InitializeVideo`, add this call after successful allocation and before `VIDEO_Configure`:

```cpp
VIDEO_ClearFrameBuffer(RenderMode, FrameBuffer, COLOR_BLACK);
```

- [ ] **Step 7: Run the integration tests and complete Wii runtime source suite**

Run the Step 2 command again, then:

```powershell
dotnet test builder.tests\helengine.wii.builder.tests.csproj -p:HelEngineRoot=C:\dev\helworks\helengine --filter "FullyQualifiedName~WiiRuntimeSourceTests" -v minimal
```

Expected: all matching tests PASS.

- [ ] **Step 8: Commit the integrated failure screen**

Review `git diff --check` and confirm the temporary audio text files are not staged. Then run:

```powershell
git add -- builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiApplication.hpp src/platform/wii/WiiApplication.cpp src/platform/wii/WiiBootHost.cpp
git commit -m "Show Wii runtime failure checkpoint"
```

This commit intentionally includes the previously uncommitted black phase-color changes because they are part of the approved black-until-boot behavior.

### Task 3: Build and verify the hardware diagnostic image

**Files:**
- Build from: `C:\dev\helprojs\demodisc\project.heproj`
- Produce: `C:\dev\helprojs\demodisc\output\wii-failure-code-20260729\game.iso`
- Produce: `C:\dev\helprojs\demodisc\output\wii-failure-code-20260729\game.wbfs`

- [ ] **Step 1: Run the complete builder test project**

Run:

```powershell
dotnet test C:\dev\helworks\helengine-wii\builder.tests\helengine.wii.builder.tests.csproj -p:HelEngineRoot=C:\dev\helworks\helengine -v minimal
```

Expected: all tests PASS with zero failures.

- [ ] **Step 2: Build a fresh Demo Disc Wii image**

Run:

```powershell
dotnet run --project C:\dev\helworks\helengine\tools\build-waiter\helengine.buildwaiter.csproj -- --output C:\dev\helprojs\demodisc\output\wii-failure-code-20260729 --require game.iso -- powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\scripts\build-platform.ps1 -Project C:\dev\helprojs\demodisc\project.heproj -Platform wii -Output C:\dev\helprojs\demodisc\output\wii-failure-code-20260729
```

Expected: exit code 0 and a fresh non-empty `game.iso`.

- [ ] **Step 3: Verify the ISO structure and IOS56 metadata**

Run:

```powershell
wit verify --verbose C:\dev\helprojs\demodisc\output\wii-failure-code-20260729\game.iso
wit dump --long C:\dev\helprojs\demodisc\output\wii-failure-code-20260729\game.iso | Select-String -Pattern "IOS|Partition|main.dol|FST"
```

Expected: WIT reports `+OK`, the image has a DATA partition, and its TMD requests IOS56.

- [ ] **Step 4: Produce and verify the USB Loader GX WBFS**

If the platform build did not already produce `game.wbfs`, run:

```powershell
wit copy C:\dev\helprojs\demodisc\output\wii-failure-code-20260729\game.iso C:\dev\helprojs\demodisc\output\wii-failure-code-20260729\game.wbfs --wbfs --overwrite
```

Then run:

```powershell
wit verify --verbose C:\dev\helprojs\demodisc\output\wii-failure-code-20260729\game.wbfs
wit dump --long C:\dev\helprojs\demodisc\output\wii-failure-code-20260729\game.wbfs | Select-String -Pattern "IOS|Partition|main.dol|FST"
```

Expected: WIT reports `+OK` and the WBFS retains IOS56 metadata.

- [ ] **Step 5: Hand off the exact hardware observation**

Copy `game.wbfs` to the normal USB Loader GX location without changing any per-game settings. Expected startup behavior:

- black until the game renders or fails
- no green uninitialized frame
- on failure, a red screen containing a white four-character code near the upper-left corner

Record the code exactly. `A003` would confirm the current leading hypothesis: packaged-storage initialization is failing around the redundant DVD mount/raw-read/partition reopen path inherited from the emulator-oriented Wii bootstrap.
