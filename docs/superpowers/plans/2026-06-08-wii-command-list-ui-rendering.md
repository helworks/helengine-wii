# Wii Command-List UI Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Wii text-only 2D overlay path with a shared command-list-driven UI renderer that draws textured quads, glyphs, rounded rects, and clip rect transitions so the packaged Wii main menu matches the authored layout.

**Architecture:** Keep `WiiRenderManager2D` as the platform bridge, but make it build and execute the generated `RenderCommandList2D` model instead of directly drawing only captured text. Use `RenderCommandListBuilder2D` as the shared producer, map clip pushes and pops onto `GX_SetScissor`, and execute textured quads and rounded rects through explicit GX command helpers.

**Tech Stack:** C# xUnit source-contract tests, C++20 Wii platform code, libogc GX, generated runtime `RenderCommandList2D` / `RenderCommandListBuilder2D`, Docker Wii native build, local Dolphin packaging scripts.

---

### Task 1: Lock The Wii Renderer To The Shared 2D Command Model

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Modify: `src/platform/wii/WiiRenderManager2D.hpp`
- Modify: `src/platform/wii/WiiApplication.cpp`
- Modify: `src/platform/wii/WiiRenderManager2D.cpp`

- [ ] **Step 1: Write the failing source-contract test**

Add a new test beside the existing packaged GPU text assertions in `builder.tests/WiiRuntimeSourceTests.cs`:

```csharp
/// <summary>
/// Ensures the Wii 2D renderer now uses the shared command-list path for non-text UI, including clip transitions.
/// </summary>
[Fact]
public void PackagedGpuText_UsesSharedCommandListAndClipAwareNonTextUiPath() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));
    string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.hpp"));
    string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.cpp"));

    Assert.Contains("RenderCapturedCommands(", applicationSource, StringComparison.Ordinal);
    Assert.Contains("RenderCommandList2D", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("RenderCommandListBuilder2D", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void RenderCapturedCommands(uint16_t frameWidth, uint16_t frameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void ExecuteCommandList(RenderCommandList2D* commandList, uint16_t frameWidth, uint16_t frameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void ExecuteRoundedRectCommand(RenderCommandList2D* commandList, int32_t payloadIndex, uint16_t frameWidth, uint16_t frameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void ApplyClipRect(const float4& clipRect, uint16_t frameWidth, uint16_t frameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("#include \"RenderCommand2DType.hpp\"", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("#include \"RenderCommandList2D.hpp\"", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("#include \"RenderCommandListBuilder2D.hpp\"", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("RenderCommandListBuilder commandListBuilder {};", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("ExecuteCommandList(commandList, frameWidth, frameHeight);", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("case RenderCommand2DType::ClipPush:", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("case RenderCommand2DType::ClipPop:", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("case RenderCommand2DType::TexturedQuad:", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("case RenderCommand2DType::GlyphQuad:", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("case RenderCommand2DType::RoundedRect:", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("GX_SetScissor(", renderManagerSource, StringComparison.Ordinal);
    Assert.DoesNotContain("void RenderCapturedText(uint16_t frameWidth, uint16_t frameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the targeted test to verify it fails**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter PackagedGpuText_UsesSharedCommandListAndClipAwareNonTextUiPath
```

Expected: `FAIL` because `WiiRenderManager2D` still exposes `RenderCapturedText` and does not yet reference `RenderCommandList2D` or `RenderCommandListBuilder2D`.

- [ ] **Step 3: Write the minimal API rename and command-list skeleton**

In `src/platform/wii/WiiRenderManager2D.hpp`, replace the text-only entrypoint and add the command-list execution surface:

```cpp
class RenderCommandList2D;
class RenderCommandListBuilder2D;
enum class RenderCommand2DType;

/// Renders the current shared 2D command stream through the Wii GX overlay path.
void RenderCapturedCommands(uint16_t frameWidth, uint16_t frameHeight);

/// Executes one generated 2D command list in render order.
void ExecuteCommandList(RenderCommandList2D* commandList, uint16_t frameWidth, uint16_t frameHeight);

/// Executes one textured-quad payload from the shared 2D command list.
void ExecuteTexturedQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, uint16_t frameWidth, uint16_t frameHeight);

/// Executes one glyph-quad payload from the shared 2D command list.
void ExecuteGlyphQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, uint16_t frameWidth, uint16_t frameHeight);

/// Executes one rounded-rectangle payload from the shared 2D command list.
void ExecuteRoundedRectCommand(RenderCommandList2D* commandList, int32_t payloadIndex, uint16_t frameWidth, uint16_t frameHeight);

/// Applies one clip rect from the shared 2D command stream to the active GX scissor state.
void ApplyClipRect(const float4& clipRect, uint16_t frameWidth, uint16_t frameHeight);

/// Restores the full-frame clip state after command execution completes.
void ResetClipRect(uint16_t frameWidth, uint16_t frameHeight);
```

In `src/platform/wii/WiiApplication.cpp`, switch the call site:

```cpp
EngineRenderManager2D->RenderCapturedCommands(
    static_cast<uint16_t>(RenderMode->fbWidth),
    static_cast<uint16_t>(RenderMode->efbHeight));
```

In `src/platform/wii/WiiRenderManager2D.cpp`, add the generated command includes and a no-op execution shell:

```cpp
#include "RenderCommand2DType.hpp"
#include "RenderCommandList2D.hpp"
#include "RenderCommandListBuilder2D.hpp"

void WiiRenderManager2D::RenderCapturedCommands(uint16_t frameWidth, uint16_t frameHeight) {
    Core* core = Core::get_Instance();
    if (core == nullptr || core->get_ObjectManager() == nullptr) {
        return;
    }

    List<ICamera*>* cameras = core->get_ObjectManager()->get_Cameras();
    RenderCommandListBuilder commandListBuilder {};
    for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++) {
        CameraComponent* camera = he_cpp_try_cast<CameraComponent>((*cameras)[cameraIndex]);
        if (camera == nullptr || camera->get_Parent() == nullptr || !camera->get_Parent()->get_IsHierarchyEnabled()) {
            continue;
        }

        RenderCommandList2D* commandList = commandListBuilder.Build(camera->get_RenderQueue2D());
        ExecuteCommandList(commandList, frameWidth, frameHeight);
    }

    ResetClipRect(frameWidth, frameHeight);
}
```

- [ ] **Step 4: Run the targeted test to verify it passes**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter PackagedGpuText_UsesSharedCommandListAndClipAwareNonTextUiPath
```

Expected: `PASS`

- [ ] **Step 5: Commit**

```bash
git add builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiApplication.cpp src/platform/wii/WiiRenderManager2D.hpp src/platform/wii/WiiRenderManager2D.cpp
git commit -m "Add Wii command-list rendering skeleton"
```

### Task 2: Execute Clip Pushes, Pops, Textured Quads, And Glyph Quads

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Modify: `src/platform/wii/WiiRenderManager2D.hpp`
- Modify: `src/platform/wii/WiiRenderManager2D.cpp`
- Test: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing source-contract assertions for clip-aware quad execution**

Extend `PackagedGpuText_UsesSharedCommandListAndClipAwareNonTextUiPath` with these assertions:

```csharp
Assert.Contains("std::vector<float4> clipStack;", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("clipStack.push_back(commandList->GetClipPushRect(payloadIndex));", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("clipStack.pop_back();", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("ApplyClipRect(clipStack.back(), frameWidth, frameHeight);", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("ExecuteTexturedQuadCommand(commandList, payloadIndex, frameWidth, frameHeight);", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("ExecuteGlyphQuadCommand(commandList, payloadIndex, frameWidth, frameHeight);", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("commandList->GetTexturedQuadTexture(payloadIndex)", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("commandList->GetGlyphQuadTexture(payloadIndex)", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("static_cast<WiiRuntimeTexture*>(texture)", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("DrawTexturedQuad2D(", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("const int32_t scissorLeft = std::max(0, static_cast<int32_t>(std::lround(clipRect.X)));", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("GX_SetScissor(scissorLeft, scissorTop, scissorWidth, scissorHeight);", renderManagerSource, StringComparison.Ordinal);
```

- [ ] **Step 2: Run the targeted test to verify it fails**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter PackagedGpuText_UsesSharedCommandListAndClipAwareNonTextUiPath
```

Expected: `FAIL` because clip-stack handling and textured-quad execution are not implemented yet.

- [ ] **Step 3: Write the minimal clip-stack and quad execution implementation**

In `src/platform/wii/WiiRenderManager2D.cpp`, implement ordered command execution for push/pop/quads/glyphs:

```cpp
void WiiRenderManager2D::ExecuteCommandList(RenderCommandList2D* commandList, uint16_t frameWidth, uint16_t frameHeight) {
    if (commandList == nullptr) {
        return;
    }

    std::vector<float4> clipStack;
    clipStack.push_back(float4(0.0f, 0.0f, static_cast<float>(frameWidth), static_cast<float>(frameHeight)));
    ApplyClipRect(clipStack.back(), frameWidth, frameHeight);

    for (int32_t commandIndex = 0; commandIndex < commandList->get_Count(); commandIndex++) {
        const RenderCommand2DType commandType = commandList->GetCommandType(commandIndex);
        switch (commandType) {
        case RenderCommand2DType::ClipPush: {
            const int32_t payloadIndex = commandList->GetClipPushPayloadIndex(commandIndex);
            clipStack.push_back(commandList->GetClipPushRect(payloadIndex));
            ApplyClipRect(clipStack.back(), frameWidth, frameHeight);
            break;
        }
        case RenderCommand2DType::ClipPop:
            if (clipStack.size() > 1U) {
                clipStack.pop_back();
            }
            ApplyClipRect(clipStack.back(), frameWidth, frameHeight);
            break;
        case RenderCommand2DType::TexturedQuad: {
            const int32_t payloadIndex = commandList->GetTexturedQuadPayloadIndex(commandIndex);
            ExecuteTexturedQuadCommand(commandList, payloadIndex, frameWidth, frameHeight);
            break;
        }
        case RenderCommand2DType::GlyphQuad: {
            const int32_t payloadIndex = commandList->GetGlyphQuadPayloadIndex(commandIndex);
            ExecuteGlyphQuadCommand(commandList, payloadIndex, frameWidth, frameHeight);
            DidSubmitGlyph = true;
            break;
        }
        case RenderCommand2DType::RoundedRect:
            break;
        }
    }
}

void WiiRenderManager2D::ApplyClipRect(const float4& clipRect, uint16_t frameWidth, uint16_t frameHeight) {
    const int32_t scissorLeft = std::max(0, static_cast<int32_t>(std::lround(clipRect.X)));
    const int32_t scissorTop = std::max(0, static_cast<int32_t>(std::lround(clipRect.Y)));
    const int32_t scissorRight = std::min(static_cast<int32_t>(frameWidth), static_cast<int32_t>(std::lround(clipRect.X + clipRect.Z)));
    const int32_t scissorBottom = std::min(static_cast<int32_t>(frameHeight), static_cast<int32_t>(std::lround(clipRect.Y + clipRect.W)));
    const int32_t scissorWidth = std::max(0, scissorRight - scissorLeft);
    const int32_t scissorHeight = std::max(0, scissorBottom - scissorTop);
    GX_SetScissor(scissorLeft, scissorTop, scissorWidth, scissorHeight);
}

void WiiRenderManager2D::ResetClipRect(uint16_t frameWidth, uint16_t frameHeight) {
    GX_SetScissor(0, 0, frameWidth, frameHeight);
}

void WiiRenderManager2D::ExecuteTexturedQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, uint16_t frameWidth, uint16_t frameHeight) {
    RuntimeTexture* texture = commandList->GetTexturedQuadTexture(payloadIndex);
    WiiRuntimeTexture* wiiTexture = static_cast<WiiRuntimeTexture*>(texture);
    if (wiiTexture == nullptr || !wiiTexture->HasNativeTextureObject()) {
        return;
    }

    const float4 bounds = commandList->GetTexturedQuadBounds(payloadIndex);
    const float4 sourceRect = commandList->GetTexturedQuadSourceRect(payloadIndex);
    const byte4 color = commandList->GetTexturedQuadColor(payloadIndex);
    ConfigureTextPipeline(frameWidth, frameHeight);
    DrawTexturedQuad2D(bounds.X, bounds.Y, bounds.Z, bounds.W, sourceRect, color, wiiTexture);
}

void WiiRenderManager2D::ExecuteGlyphQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, uint16_t frameWidth, uint16_t frameHeight) {
    RuntimeTexture* texture = commandList->GetGlyphQuadTexture(payloadIndex);
    WiiRuntimeTexture* wiiTexture = static_cast<WiiRuntimeTexture*>(texture);
    if (wiiTexture == nullptr || !wiiTexture->HasNativeTextureObject()) {
        return;
    }

    const float4 bounds = commandList->GetGlyphQuadBounds(payloadIndex);
    const float4 sourceRect = commandList->GetGlyphQuadSourceRect(payloadIndex);
    const byte4 color = commandList->GetGlyphQuadColor(payloadIndex);
    ConfigureTextPipeline(frameWidth, frameHeight);
    DrawTexturedQuad2D(bounds.X, bounds.Y, bounds.Z, bounds.W, sourceRect, color, wiiTexture);
}
```

- [ ] **Step 4: Run the targeted test and the packaged slice**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter PackagedGpuText_UsesSharedCommandListAndClipAwareNonTextUiPath
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter Packaged
```

Expected:

- first command: `PASS`
- second command: `PASS`

- [ ] **Step 5: Commit**

```bash
git add builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiRenderManager2D.hpp src/platform/wii/WiiRenderManager2D.cpp
git commit -m "Execute Wii clip and quad command list entries"
```

### Task 3: Render Rounded Rect Fills And Borders Through GX

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Modify: `src/platform/wii/WiiRenderManager2D.hpp`
- Modify: `src/platform/wii/WiiRenderManager2D.cpp`

- [ ] **Step 1: Write the failing rounded-rect source-contract assertions**

Extend `PackagedGpuText_UsesSharedCommandListAndClipAwareNonTextUiPath` with:

```csharp
Assert.Contains("const float4 bounds = commandList->GetRoundedRectBounds(payloadIndex);", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("const float radius = commandList->GetRoundedRectRadius(payloadIndex);", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("const float borderThickness = commandList->GetRoundedRectBorderThickness(payloadIndex);", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("const RoundedRectCorners corners = commandList->GetRoundedRectCorners(payloadIndex);", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("const byte4 fillColor = commandList->GetRoundedRectFillColor(payloadIndex);", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("const byte4 borderColor = commandList->GetRoundedRectBorderColor(payloadIndex);", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("DrawSolidQuad2D(bounds.X, bounds.Y, bounds.Z, bounds.W, borderColor);", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("DrawSolidQuad2D(bounds.X + inset, bounds.Y + inset, innerWidth, innerHeight, fillColor);", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("const double inset = std::max(0.0, static_cast<double>(borderThickness));", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("const double clampedRadius = std::max(0.0, static_cast<double>(radius));", renderManagerSource, StringComparison.Ordinal);
```

- [ ] **Step 2: Run the targeted test to verify it fails**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter PackagedGpuText_UsesSharedCommandListAndClipAwareNonTextUiPath
```

Expected: `FAIL` because rounded-rect command execution is still empty.

- [ ] **Step 3: Write the minimal rounded-rect implementation**

In `src/platform/wii/WiiRenderManager2D.cpp`, implement the first-pass rounded-rect renderer around solid quads:

```cpp
void WiiRenderManager2D::ExecuteRoundedRectCommand(RenderCommandList2D* commandList, int32_t payloadIndex, uint16_t frameWidth, uint16_t frameHeight) {
    const float4 bounds = commandList->GetRoundedRectBounds(payloadIndex);
    const float radius = commandList->GetRoundedRectRadius(payloadIndex);
    const float borderThickness = commandList->GetRoundedRectBorderThickness(payloadIndex);
    const RoundedRectCorners corners = commandList->GetRoundedRectCorners(payloadIndex);
    const byte4 fillColor = commandList->GetRoundedRectFillColor(payloadIndex);
    const byte4 borderColor = commandList->GetRoundedRectBorderColor(payloadIndex);

    const double clampedRadius = std::max(0.0, static_cast<double>(radius));
    const double inset = std::max(0.0, static_cast<double>(borderThickness));
    const float innerX = static_cast<float>(bounds.X + inset);
    const float innerY = static_cast<float>(bounds.Y + inset);
    const float innerWidth = static_cast<float>(std::max(0.0, static_cast<double>(bounds.Z) - (inset * 2.0)));
    const float innerHeight = static_cast<float>(std::max(0.0, static_cast<double>(bounds.W) - (inset * 2.0)));

    ConfigureSolidColorPipeline(frameWidth, frameHeight);

    DrawSolidQuad2D(bounds.X, bounds.Y, bounds.Z, bounds.W, borderColor);
    if (innerWidth > 0.0f && innerHeight > 0.0f) {
        DrawSolidQuad2D(innerX, innerY, innerWidth, innerHeight, fillColor);
    }

    if (clampedRadius > 0.0 && corners != RoundedRectCorners::None) {
        const float cornerSize = static_cast<float>(std::min(clampedRadius, static_cast<double>(std::min(bounds.Z, bounds.W) * 0.5f)));
        DrawSolidQuad2D(bounds.X, bounds.Y, cornerSize, cornerSize, fillColor);
        DrawSolidQuad2D(bounds.X + bounds.Z - cornerSize, bounds.Y, cornerSize, cornerSize, fillColor);
        DrawSolidQuad2D(bounds.X, bounds.Y + bounds.W - cornerSize, cornerSize, cornerSize, fillColor);
        DrawSolidQuad2D(bounds.X + bounds.Z - cornerSize, bounds.Y + bounds.W - cornerSize, cornerSize, cornerSize, fillColor);
    }
}
```

Also wire rounded rect dispatch into `ExecuteCommandList`:

```cpp
case RenderCommand2DType::RoundedRect: {
    const int32_t payloadIndex = commandList->GetRoundedRectPayloadIndex(commandIndex);
    ExecuteRoundedRectCommand(commandList, payloadIndex, frameWidth, frameHeight);
    break;
}
```

- [ ] **Step 4: Run the source test, packaged slice, native build, and visible verification**

Run:

```powershell
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter PackagedGpuText_UsesSharedCommandListAndClipAwareNonTextUiPath
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter Packaged
rtk proxy docker run --rm -v C:/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii -e HELENGINE_WII_BOOT_MODE=packaged-disc helengine-wii sh -lc "make 2>&1 | tail -c 12000"
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-wii\tmp\launch_wii_iso_in_dolphin.ps1 -IsoPath C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\city-self-apploader-v26.iso
```

Expected:

- both `dotnet test` commands: `PASS`
- Docker build: `elf2dol build/helengine_wii.elf build/helengine_wii.dol`
- visible Dolphin session: menu background and panel structure now match the authored layout instead of text-only output

- [ ] **Step 5: Commit**

```bash
git add builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiRenderManager2D.hpp src/platform/wii/WiiRenderManager2D.cpp
git commit -m "Render Wii rounded rect UI commands"
```

### Task 4: Repackage The ISO And Capture Proof

**Files:**
- Modify: `src/platform/wii/WiiApplication.cpp`
- Modify: `src/platform/wii/WiiRenderManager2D.cpp`
- Test: `tmp/self-apploader-package-v3/*`

- [ ] **Step 1: Keep the existing render diagnostics but align them to the command-list path**

If the existing diagnostic markers still help, keep them after command execution rather than before it:

```cpp
if (DidSubmitGlyph) {
    ConfigureSolidColorPipeline(frameWidth, frameHeight);
    DrawSolidQuad2D(40.0f, 8.0f, 24.0f, 24.0f, byte4 { 0x00, 0x40, 0xFF, 0xFF });
    ResetClipRect(frameWidth, frameHeight);
}
```

Do not restore any text-only fallback branches in `RenderCapturedCommands`.

- [ ] **Step 2: Rebuild the packaged ISO from the fresh DOL**

Run:

```powershell
rtk dotnet C:\dev\helworks\helengine-wii\builder\bin\Debug\net9.0-windows\helengine.wii.builder.dll --write-disc-layout C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\staging C:\dev\helworks\helengine-wii\build\helengine_wii.dol C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\disc-refresh-v27 RCIE01 city
$env:HELENGINE_WII_WIT_PATH='C:\dev\helworks\helengine-wii\tmp\tools\wit-v3.05a-r8638-cygwin64\bin\wit.exe'
rtk dotnet C:\dev\helworks\helengine-wii\builder\bin\Debug\net9.0-windows\helengine.wii.builder.dll --package-image C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\disc-refresh-v27 C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\city-self-apploader-v27.iso
```

Expected:

- first command prints the extracted disc root path
- second command prints `C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\city-self-apploader-v27.iso`

- [ ] **Step 3: Capture local proof**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-wii\tmp\capture_dolphin_warning_windows_isolated.ps1 -IsoPath C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\city-self-apploader-v27.iso -UserDir C:\dev\helworks\helengine-wii\tmp\dolphin-warning-isolated-user-v27 -OutputDirectory C:\dev\helworks\helengine-wii\tmp\packaged-disc-proof-life\warning-windows-isolated-v27 -DurationSeconds 15
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-wii\tmp\collect_dolphin_window_text.ps1 -IsoPath C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\city-self-apploader-v27.iso -UserDir C:\dev\helworks\helengine-wii\tmp\dolphin-window-text-user-v27 -OutputPath C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\dolphin-window-text-report-v27.txt -DurationSeconds 10
```

Expected:

- screenshot output includes `window-00.png` for the game window
- text report shows stable `Render2D trace` lines with no new draw exceptions

- [ ] **Step 4: Confirm the authored menu now matches**

Inspect:

```text
C:\dev\helworks\helengine-wii\tmp\packaged-disc-proof-life\warning-windows-isolated-v27\window-00.png
C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\dolphin-window-text-report-v27.txt
```

Expected visible result:

- menu background color present
- menu item panels visible
- selection styling visible
- `Demo Scenes`, `Physics Scenes`, `Options`, `wii`, and `wii-headless` still rendered

- [ ] **Step 5: Commit**

```bash
git add src/platform/wii/WiiApplication.cpp src/platform/wii/WiiRenderManager2D.cpp builder.tests/WiiRuntimeSourceTests.cs
git commit -m "Match Wii menu visuals with shared UI command list"
```
