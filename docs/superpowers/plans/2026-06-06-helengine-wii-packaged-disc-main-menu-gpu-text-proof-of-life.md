# Helengine Wii Packaged-Disc Main Menu GPU Text Proof-of-Life Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the packaged-disc Wii build render all authored main-menu text in Dolphin using cooked font assets and a GPU-only GX glyph pass.

**Architecture:** Keep the existing packaged-disc scene bootstrap and 2D drawable capture path, then add the smallest native GX text path on top of it. The implementation should first guarantee that builder-produced packaged native DOLs actually compile in packaged-disc mode, then add a Wii-native runtime texture for cooked font atlases, and finally render queued `ITextDrawable2D` instances as textured glyph quads after `EngineCore->Draw()`.

**Tech Stack:** C++17, libogc GX, Wii native host, generated-core runtime headers, C# builder/executor code, xUnit source-audit tests, shared `build-platform.ps1` wrapper, Dolphin

---

## File Structure

**Create:**
- `src/platform/wii/WiiRuntimeTexture.hpp`
- `src/platform/wii/WiiRuntimeTexture.cpp`

**Modify:**
- `builder/WiiDockerNativeBuildExecutor.cs`
- `builder.tests/WiiDockerNativeBuildExecutorTests.cs`
- `builder.tests/WiiRuntimeSourceTests.cs`
- `src/platform/wii/WiiRenderManager2D.hpp`
- `src/platform/wii/WiiRenderManager2D.cpp`
- `src/platform/wii/WiiApplication.cpp`

**Reference while implementing:**
- `C:\dev\helworks\helengine-gc\src\platform\gamecube\GameCubeRuntimeTexture.hpp`
- `C:\dev\helworks\helengine-gc\src\platform\gamecube\GameCubeRuntimeTexture.cpp`
- `C:\dev\helworks\helengine-gc\src\platform\gamecube\GameCubeRasterRenderer.cpp`

**Responsibilities:**
- `WiiDockerNativeBuildExecutor.cs`: force packaged-disc compile mode for builder-produced native DOLs.
- `WiiRuntimeTexture.*`: own one GX texture object plus encoded texture memory for packaged font atlases.
- `WiiRenderManager2D.*`: build/release native runtime textures, keep capture queues, and render all queued text drawables as GX glyph quads.
- `WiiApplication.cpp`: invoke the native text pass after generated draw submission and before frame presentation.
- `WiiRuntimeSourceTests.cs`: lock the GPU-text contract so later work cannot silently regress into “capture only” behavior.
- `WiiDockerNativeBuildExecutorTests.cs`: lock the packaged-disc boot-mode environment for shared wrapper builds.

### Task 1: Force packaged-disc native builds in the shared Wii executor

**Files:**
- Modify: `builder.tests/WiiDockerNativeBuildExecutorTests.cs`
- Modify: `builder/WiiDockerNativeBuildExecutor.cs`
- Test: `builder.tests/WiiDockerNativeBuildExecutorTests.cs`

- [ ] **Step 1: Write the failing executor test**

```csharp
/// <summary>
/// Ensures packaged Wii native builds explicitly opt into the packaged-disc boot path.
/// </summary>
[Fact]
public void CreateStartInfo_PackagedBuild_ExportsPackagedDiscBootMode() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    WiiBuilderPaths paths = new(
        repositoryRootPath,
        Path.Combine(repositoryRootPath, "tmp", "generated-core"),
        Path.Combine(repositoryRootPath, "tmp", "staged-content"),
        Path.Combine(repositoryRootPath, "tmp", "disc"),
        Path.Combine(repositoryRootPath, "tmp", "game.iso"),
        Path.Combine(repositoryRootPath, "tmp", "native", "helengine_wii.dol"));

    MethodInfo createStartInfoMethod = typeof(WiiDockerNativeBuildExecutor).GetMethod(
        "CreateStartInfo",
        BindingFlags.Static | BindingFlags.NonPublic);

    Assert.NotNull(createStartInfoMethod);

    ProcessStartInfo startInfo = (ProcessStartInfo)createStartInfoMethod.Invoke(null, [paths]);

    Assert.Contains("HELENGINE_WII_BOOT_MODE=packaged-disc", startInfo.ArgumentList);
    Assert.Contains("make", startInfo.ArgumentList);
    Assert.Contains("clean", startInfo.ArgumentList);
    Assert.Contains("all", startInfo.ArgumentList);
}
```

- [ ] **Step 2: Run the executor test to verify it fails**

Run:

```bash
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~WiiDockerNativeBuildExecutorTests"
```

Expected: FAIL because `HELENGINE_WII_BOOT_MODE=packaged-disc` is not yet present in `ProcessStartInfo.ArgumentList`.

- [ ] **Step 3: Add the packaged-disc boot-mode environment to the Docker start info**

```csharp
startInfo.ArgumentList.Add("-e");
startInfo.ArgumentList.Add("HELENGINE_CORE_CPP_ROOT=" + generatedCoreContainerPath);
startInfo.ArgumentList.Add("-e");
startInfo.ArgumentList.Add("HELENGINE_WII_BOOT_MODE=packaged-disc");
startInfo.ArgumentList.Add("helengine-wii");
startInfo.ArgumentList.Add("make");
startInfo.ArgumentList.Add("clean");
startInfo.ArgumentList.Add("all");
```

- [ ] **Step 4: Re-run the executor test to verify it passes**

Run:

```bash
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~WiiDockerNativeBuildExecutorTests"
```

Expected: PASS with the new environment variable visible in the reflected `ArgumentList`.

- [ ] **Step 5: Commit**

```bash
rtk git add builder/WiiDockerNativeBuildExecutor.cs builder.tests/WiiDockerNativeBuildExecutorTests.cs
rtk git commit -m "fix: force packaged-disc boot for wii native builds"
```

### Task 2: Add a Wii-native runtime texture for packaged font atlases

**Files:**
- Create: `src/platform/wii/WiiRuntimeTexture.hpp`
- Create: `src/platform/wii/WiiRuntimeTexture.cpp`
- Modify: `src/platform/wii/WiiRenderManager2D.hpp`
- Modify: `src/platform/wii/WiiRenderManager2D.cpp`
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Test: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing source-audit test for native texture upload**

```csharp
/// <summary>
/// Ensures the Wii 2D bridge can turn packaged font atlas texture assets into native GX runtime textures.
/// </summary>
[Fact]
public void PackagedGpuText_DeclaresWiiRuntimeTextureAndRawTextureUploadPath() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string runtimeTextureHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRuntimeTexture.hpp");
    string runtimeTextureSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRuntimeTexture.cpp");
    string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.hpp"));
    string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.cpp"));

    Assert.True(File.Exists(runtimeTextureHeaderPath), "Expected WiiRuntimeTexture.hpp to exist.");
    Assert.True(File.Exists(runtimeTextureSourcePath), "Expected WiiRuntimeTexture.cpp to exist.");

    string runtimeTextureHeaderSource = File.ReadAllText(runtimeTextureHeaderPath);
    string runtimeTextureSource = File.ReadAllText(runtimeTextureSourcePath);

    Assert.Contains("class WiiRuntimeTexture final : public RuntimeTexture", runtimeTextureHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void LoadFromRaw(TextureAsset* data);", runtimeTextureHeaderSource, StringComparison.Ordinal);
    Assert.Contains("GXTexObj* GetNativeTextureObject();", runtimeTextureHeaderSource, StringComparison.Ordinal);
    Assert.Contains("TextureAssetColorFormat::Rgba32", runtimeTextureSource, StringComparison.Ordinal);
    Assert.Contains("GX_InitTexObj(", runtimeTextureSource, StringComparison.Ordinal);
    Assert.Contains("GX_InitTexObjFilterMode(", runtimeTextureSource, StringComparison.Ordinal);
    Assert.Contains("DCFlushRange(", runtimeTextureSource, StringComparison.Ordinal);
    Assert.Contains("RuntimeTexture* WiiRenderManager2D::BuildTextureFromRaw(TextureAsset* data)", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("new WiiRuntimeTexture()", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("runtimeTexture->LoadFromRaw(data);", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("void WiiRenderManager2D::ReleaseTexture(RuntimeTexture* texture)", renderManagerSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the source-audit test to verify it fails**

Run:

```bash
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~WiiRuntimeSourceTests"
```

Expected: FAIL because `WiiRuntimeTexture.*` does not exist and `BuildTextureFromRaw` still throws.

- [ ] **Step 3: Implement the Wii runtime texture and raw font-atlas upload path**

Use the GameCube runtime texture as the structural reference, but keep the Wii scope minimal and text-focused.

`src/platform/wii/WiiRuntimeTexture.hpp`

```cpp
#pragma once

#include <cstddef>

#include <gccore.h>

#include "RuntimeTexture.hpp"

class TextureAsset;

namespace helengine::wii {
    /// Stores one Wii-native GX texture object plus its encoded texture memory.
    class WiiRuntimeTexture final : public RuntimeTexture {
    public:
        /// Creates one empty Wii runtime texture.
        WiiRuntimeTexture();

        /// Releases owned Wii texture memory.
        ~WiiRuntimeTexture() override;

        /// Encodes one logical texture asset into native GX texture memory.
        void LoadFromRaw(TextureAsset* data);

        /// Returns whether a native GX texture object was initialized.
        bool HasNativeTextureObject() const;

        /// Returns the native GX texture object used during glyph rendering.
        GXTexObj* GetNativeTextureObject();

    private:
        /// Releases any previously allocated native texture memory.
        void ResetNativeTextureData();

        /// Encodes one logical RGBA32 atlas into tiled GX RGB5A3 memory.
        void EncodeRgba32ToRgb5A3(TextureAsset* data);

        void* NativeTextureData;
        std::size_t NativeTextureDataSize;
        GXTexObj NativeTextureObject;
        bool NativeTextureObjectInitialized;
    };
}
```

`src/platform/wii/WiiRuntimeTexture.cpp`

```cpp
#include "platform/wii/WiiRuntimeTexture.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <malloc.h>
#include <ogc/cache.h>

#include "TextureAsset.hpp"
#include "TextureAssetColorFormat.hpp"
#include "runtime/native_exceptions.hpp"

namespace {
    static uint16_t Convert8To5(uint8_t value) {
        return static_cast<uint16_t>((static_cast<uint32_t>(value) * 31U + 127U) / 255U);
    }

    static uint16_t Convert8To4(uint8_t value) {
        return static_cast<uint16_t>((static_cast<uint32_t>(value) * 15U + 127U) / 255U);
    }

    static uint16_t Convert8To3(uint8_t value) {
        return static_cast<uint16_t>((static_cast<uint32_t>(value) * 7U + 127U) / 255U);
    }

    static uint16_t EncodeRgb5A3Pixel(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
        if (alpha >= 224U) {
            return static_cast<uint16_t>(0x8000U | (Convert8To5(red) << 10) | (Convert8To5(green) << 5) | Convert8To5(blue));
        }

        return static_cast<uint16_t>((Convert8To3(alpha) << 12) | (Convert8To4(red) << 8) | (Convert8To4(green) << 4) | Convert8To4(blue));
    }
}

namespace helengine::wii {
    WiiRuntimeTexture::WiiRuntimeTexture()
        : RuntimeTexture()
        , NativeTextureData(nullptr)
        , NativeTextureDataSize(0U)
        , NativeTextureObject {}
        , NativeTextureObjectInitialized(false) {
    }

    WiiRuntimeTexture::~WiiRuntimeTexture() {
        ResetNativeTextureData();
    }

    void WiiRuntimeTexture::LoadFromRaw(TextureAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        } else if (data->ColorFormat != TextureAssetColorFormat::Rgba32) {
            throw new InvalidOperationException("Wii text proof-of-life currently supports only RGBA32 packaged font atlas textures.");
        }

        ResetNativeTextureData();
        EncodeRgba32ToRgb5A3(data);
    }

    void WiiRuntimeTexture::EncodeRgba32ToRgb5A3(TextureAsset* data) {
        const uint32_t width = data->Width;
        const uint32_t height = data->Height;
        const uint32_t paddedWidth = (width + 3U) & ~3U;
        const uint32_t paddedHeight = (height + 3U) & ~3U;

        NativeTextureDataSize = static_cast<std::size_t>(paddedWidth) * static_cast<std::size_t>(paddedHeight) * 2U;
        NativeTextureData = memalign(32, NativeTextureDataSize);
        if (NativeTextureData == nullptr) {
            throw new InvalidOperationException("Could not allocate Wii texture memory.");
        }

        std::memset(NativeTextureData, 0, NativeTextureDataSize);
        uint16_t* destination = static_cast<uint16_t*>(NativeTextureData);
        for (uint32_t blockY = 0; blockY < paddedHeight; blockY += 4U) {
            for (uint32_t blockX = 0; blockX < paddedWidth; blockX += 4U) {
                for (uint32_t innerY = 0; innerY < 4U; innerY++) {
                    for (uint32_t innerX = 0; innerX < 4U; innerX++) {
                        const uint32_t sampleX = std::min(blockX + innerX, width - 1U);
                        const uint32_t sampleY = std::min(blockY + innerY, height - 1U);
                        const std::size_t sourceOffset = (static_cast<std::size_t>(sampleY) * static_cast<std::size_t>(width) + sampleX) * 4U;
                        *destination++ = EncodeRgb5A3Pixel(
                            (*data->Colors)[static_cast<int32_t>(sourceOffset + 0U)],
                            (*data->Colors)[static_cast<int32_t>(sourceOffset + 1U)],
                            (*data->Colors)[static_cast<int32_t>(sourceOffset + 2U)],
                            (*data->Colors)[static_cast<int32_t>(sourceOffset + 3U)]);
                    }
                }
            }
        }

        DCFlushRange(NativeTextureData, NativeTextureDataSize);
        GX_InitTexObj(&NativeTextureObject, NativeTextureData, paddedWidth, paddedHeight, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GX_InitTexObjFilterMode(&NativeTextureObject, GX_LINEAR, GX_LINEAR);
        NativeTextureObjectInitialized = true;
        this->set_Width(static_cast<int32_t>(width));
        this->set_Height(static_cast<int32_t>(height));
    }
}
```

`src/platform/wii/WiiRenderManager2D.cpp`

```cpp
RuntimeTexture* WiiRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
    if (data == nullptr) {
        throw new ArgumentNullException("data");
    }

    WiiRuntimeTexture* runtimeTexture = new WiiRuntimeTexture();
    runtimeTexture->LoadFromRaw(data);
    return runtimeTexture;
}

void WiiRenderManager2D::ReleaseTexture(RuntimeTexture* texture) {
    if (texture == nullptr) {
        throw new ArgumentNullException("texture");
    }

    texture->Dispose();
    delete texture;
}
```

- [ ] **Step 4: Re-run the source-audit test to verify it passes**

Run:

```bash
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~WiiRuntimeSourceTests"
```

Expected: PASS with the new runtime texture files and `BuildTextureFromRaw` path in place.

- [ ] **Step 5: Commit**

```bash
rtk git add src/platform/wii/WiiRuntimeTexture.hpp src/platform/wii/WiiRuntimeTexture.cpp src/platform/wii/WiiRenderManager2D.hpp src/platform/wii/WiiRenderManager2D.cpp builder.tests/WiiRuntimeSourceTests.cs
rtk git commit -m "feat: add wii gx runtime texture upload for menu fonts"
```

### Task 3: Render captured menu text as GX glyph quads

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Modify: `src/platform/wii/WiiRenderManager2D.hpp`
- Modify: `src/platform/wii/WiiRenderManager2D.cpp`
- Modify: `src/platform/wii/WiiApplication.cpp`
- Test: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing source-audit test for the glyph pass and frame integration**

```csharp
/// <summary>
/// Ensures the Wii packaged-disc text proof renders queued text drawables through a native GX glyph pass.
/// </summary>
[Fact]
public void PackagedGpuText_RendersQueuedGlyphsThroughNativeGxPass() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.hpp"));
    string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.cpp"));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));

    Assert.Contains("void RenderCapturedText(uint16_t frameWidth, uint16_t frameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("TextLayoutUtils::WrapText(", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("TextLayoutAlignmentUtils::MeasureVisibleLineWidth(", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("GX_LoadTexObj(", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("GX_Begin(GX_QUADS, GX_VTXFMT0, 4);", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("font->get_Texture()", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("TextQueue", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("EngineRenderManager2D->RenderCapturedText(", applicationSource, StringComparison.Ordinal);
    Assert.Contains("EngineCore->Draw();", applicationSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the source-audit test to verify it fails**

Run:

```bash
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~WiiRuntimeSourceTests"
```

Expected: FAIL because there is still no native text pass or application integration point.

- [ ] **Step 3: Implement the text-only GX overlay pass and call it after generated draw capture**

`src/platform/wii/WiiRenderManager2D.hpp`

```cpp
/// Renders all captured text draw requests through the Wii GX overlay path.
void RenderCapturedText(uint16_t frameWidth, uint16_t frameHeight);
```

`src/platform/wii/WiiRenderManager2D.cpp`

```cpp
#include "TextLayoutAlignmentUtils.hpp"
#include "TextLayoutUtils.hpp"
#include "TextAlignment.hpp"
#include "TextComponent.hpp"
#include "platform/wii/WiiRuntimeTexture.hpp"

void WiiRenderManager2D::RenderCapturedText(uint16_t frameWidth, uint16_t frameHeight) {
    if (TextQueue.empty()) {
        return;
    }

    Mtx44 projection {};
    guOrtho(projection, 0.0f, static_cast<f32>(frameHeight), 0.0f, static_cast<f32>(frameWidth), 0.0f, 1.0f);
    GX_LoadProjectionMtx(projection, GX_ORTHOGRAPHIC);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
    GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetNumTevStages(1);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);

    for (const WiiTextDrawCommand& command : TextQueue) {
        ITextDrawable2D* drawable = command.Drawable;
        if (drawable == nullptr || drawable->get_Parent() == nullptr || !drawable->get_Parent()->get_IsHierarchyEnabled()) {
            continue;
        }

        FontAsset* font = drawable->get_Font();
        if (font == nullptr || font->get_Texture() == nullptr) {
            continue;
        }

        WiiRuntimeTexture* texture = static_cast<WiiRuntimeTexture*>(font->get_Texture());
        if (texture == nullptr || !texture->HasNativeTextureObject()) {
            continue;
        }

        GX_LoadTexObj(texture->GetNativeTextureObject(), GX_TEXMAP0);

        std::string content = drawable->get_Text();
        const double fontScale = std::max(static_cast<double>(drawable->get_FontScale()), 0.0001);
        if (drawable->get_WrapText()) {
            content = TextLayoutUtils::WrapText(content, font, std::max(1, static_cast<int32_t>(std::lround(drawable->get_Size().X / fontScale))));
        }

        std::vector<double> lineOffsets;
        std::stringstream lineStream(content);
        std::string line;
        while (std::getline(lineStream, line, '\n')) {
            const double visibleWidth = TextLayoutAlignmentUtils::MeasureVisibleLineWidth(line, font, fontScale, texture->get_Width());
            lineOffsets.push_back(TextLayoutAlignmentUtils::ResolveHorizontalOffset(drawable->get_Alignment(), drawable->get_Size().X, visibleWidth));
        }

        // Reuse the GameCube text path structure: advance through content, bind the atlas once per drawable,
        // and emit one textured quad per visible glyph.
        // Each quad must call GX_Position3f32, GX_Color4u8, and GX_TexCoord2f32 for four vertices.
    }
}
```

`src/platform/wii/WiiApplication.cpp`

```cpp
bool WiiApplication::DrawEngineCore() {
    // existing guards

    try {
        SetBootPhase(WiiBootPhase::CoreDraw, GXColor { 0x00, 0x60, 0x00, 0xFF });
        EngineCore->Draw();
        EngineRenderManager2D->RenderCapturedText(
            static_cast<uint16_t>(RenderMode->fbWidth),
            static_cast<uint16_t>(RenderMode->efbHeight));
        DrawFrameLogCount++;
        DrawCompletedSincePresent = true;
        VerifiedFrameCount++;
        return true;
    } catch (...) {
        // existing failure behavior
    }
}
```

- [ ] **Step 4: Re-run the source-audit test to verify it passes**

Run:

```bash
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~WiiRuntimeSourceTests"
```

Expected: PASS with the native glyph pass and application draw integration visible in source.

- [ ] **Step 5: Commit**

```bash
rtk git add src/platform/wii/WiiRenderManager2D.hpp src/platform/wii/WiiRenderManager2D.cpp src/platform/wii/WiiApplication.cpp builder.tests/WiiRuntimeSourceTests.cs
rtk git commit -m "feat: render packaged wii menu text with gx glyph quads"
```

### Task 4: Verify packaged-disc menu text proof-of-life end to end

**Files:**
- Verify: `builder.tests/WiiDockerNativeBuildExecutorTests.cs`
- Verify: `builder.tests/WiiRuntimeSourceTests.cs`
- Verify: `src/platform/wii/WiiRuntimeTexture.hpp`
- Verify: `src/platform/wii/WiiRuntimeTexture.cpp`
- Verify: `src/platform/wii/WiiRenderManager2D.hpp`
- Verify: `src/platform/wii/WiiRenderManager2D.cpp`
- Verify: `src/platform/wii/WiiApplication.cpp`

- [ ] **Step 1: Run the focused Wii test slice**

Run:

```bash
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~WiiDockerNativeBuildExecutorTests|FullyQualifiedName~WiiRuntimeSourceTests"
```

Expected: PASS for the executor and runtime source-audit coverage added in this plan.

- [ ] **Step 2: Run the broader Wii builder test suite**

Run:

```bash
rtk dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter "WiiRuntimeSourceTests|WiiRuntimeSceneManifestWriterTests|WiiPackagedBuildWorkspaceTests|WiiDockerNativeBuildExecutorTests|WiiWiimmsIsoToolsImagePackagerTests|WiiLooseContentStagerTests"
```

Expected: PASS with no regression in packaged-disc staging, runtime manifest wiring, or image packaging.

- [ ] **Step 3: Build the packaged Wii output through the shared wrapper**

Run:

```powershell
rtk proxy powershell.exe -NoProfile -ExecutionPolicy Bypass -File ..\helengine\artifacts\build-platform.ps1 `
  -Project ..\helprojs\city\project.heproj `
  -Platform wii `
  -Output ..\helprojs\city\wii-build
```

Expected: PASS and emit:
- `..\helprojs\city\wii-build\game.iso`
- `..\helprojs\city\wii-build\native\helengine_wii.dol`
- `..\helprojs\city\wii-build\disc\sys\main.dol`

- [ ] **Step 4: Launch the packaged-disc image in Dolphin and verify visible authored menu text**

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\dev\helworks\emus\dolphin-2603a-x64\Dolphin-x64\Dolphin.exe' -b -e '..\helprojs\city\wii-build\game.iso'"
```

Expected:
- Dolphin opens the packaged-disc image instead of a loose `DOL`
- the title stays alive long enough to reach the main menu scene
- readable authored title and button text appear on screen
- no CPU-rendered fallback path is involved

- [ ] **Step 5: Commit**

```bash
rtk git add builder/WiiDockerNativeBuildExecutor.cs builder.tests/WiiDockerNativeBuildExecutorTests.cs builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiRuntimeTexture.hpp src/platform/wii/WiiRuntimeTexture.cpp src/platform/wii/WiiRenderManager2D.hpp src/platform/wii/WiiRenderManager2D.cpp src/platform/wii/WiiApplication.cpp
rtk git commit -m "feat: prove packaged wii menu text renders in dolphin"
```
