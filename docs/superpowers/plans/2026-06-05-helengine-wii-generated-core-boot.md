# Helengine Wii Generated-Core Boot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first Wii runtime slice that compiles generated Helengine core output, initializes `Core`, and runs deterministic `Update()` / `Draw()` frames on the Wii host.

**Architecture:** Use `helengine-gc` as the reference, but keep Wii platform code Wii-owned. Add Wii support to `csharpcodegen` first so the generated config emits `HE_CPP_PLATFORM_WII`, then update `helengine-wii` to validate and compile that generated output. The native Wii runtime will introduce Wii-specific application, input, and render bridge classes; it will not add builder, asset cooking, disc packaging, or full GX scene rendering in this slice.

**Tech Stack:** C#/.NET tests for `csharpcodegen`, C++20, devkitPPC, libogc Wii rules, GNU Make, Docker, Dolphin

---

## File Structure

- Modify: `C:\dev\helworks\csharpcodegen\cs2.cpp\model\CPPPlatformProfile.cs`
  Purpose: add the Wii headless platform profile factory.
- Modify: `C:\dev\helworks\csharpcodegen\cs2.cpp\model\CPPConversionOptions.cs`
  Purpose: add a `CreateWiiDefault()` helper for tests and future platform callers.
- Modify: `C:\dev\helworks\csharpcodegen\cs2.cpp\CPPConversionPresetCatalog.cs`
  Purpose: add the `wii-core-boot` preset.
- Modify: `C:\dev\helworks\csharpcodegen\codegen\CodegenCliOptionsBuilder.cs`
  Purpose: map `--platform wii` to the Wii headless profile.
- Modify: `C:\dev\helworks\csharpcodegen\cs2.cpp.tests\CPPConversionPresetCatalogTests.cs`
  Purpose: cover Wii preset resolution.
- Modify: `C:\dev\helworks\csharpcodegen\cs2.cpp.tests\CPPGeneratedConfigWriterTests.cs`
  Purpose: cover generated Wii config defines.
- Modify: `Makefile`
  Purpose: compile the generated core translation unit and Wii bridge sources when `HELENGINE_CORE_CPP_ROOT` is set.
- Modify: `src/main.cpp`
  Purpose: enter the Wii application runtime instead of the pink-screen-only boot host.
- Create: `src/platform/wii/WiiApplication.hpp`
  Purpose: declare Wii host startup, generated-core boot, and frame-loop ownership.
- Create: `src/platform/wii/WiiApplication.cpp`
  Purpose: initialize VI/GX, construct generated core services, run update/draw frames, and present diagnostic frames.
- Create: `src/platform/wii/WiiBootPhase.hpp`
  Purpose: identify boot phases used for deterministic diagnostic colors.
- Create: `src/platform/wii/WiiInputManager.hpp`
  Purpose: declare the generated input backend bridge.
- Create: `src/platform/wii/WiiInputManager.cpp`
  Purpose: return one no-dependency Wii input frame, using controller state only when available.
- Create: `src/platform/wii/WiiRenderManager2D.hpp`
  Purpose: declare a minimal generated 2D render bridge that captures overlay submissions.
- Create: `src/platform/wii/WiiRenderManager2D.cpp`
  Purpose: implement first-slice 2D draw capture and fail explicitly for texture loading.
- Create: `src/platform/wii/WiiRenderManager3D.hpp`
  Purpose: declare a minimal generated 3D render bridge that can satisfy `Core::Draw()`.
- Create: `src/platform/wii/WiiRenderManager3D.cpp`
  Purpose: implement first-slice draw orchestration without native mesh rendering.
- Modify: `README.md`
  Purpose: document codegen, generated-core build, and Dolphin verification commands.
- Keep: `src/platform/wii/WiiBootHost.hpp`
  Purpose: leave the original bootstrap class untouched until the Wii application has replaced it in `main.cpp`.
- Keep: `src/platform/wii/WiiBootHost.cpp`
  Purpose: leave the original pink-screen host as historical fallback source for this slice; remove it in a later cleanup only after runtime parity is stable.

### Task 1: Add Failing Codegen Tests For Wii Platform Support

**Files:**
- Modify: `C:\dev\helworks\csharpcodegen\cs2.cpp.tests\CPPConversionPresetCatalogTests.cs`
- Modify: `C:\dev\helworks\csharpcodegen\cs2.cpp.tests\CPPGeneratedConfigWriterTests.cs`

- [ ] **Step 1: Add Wii preset tests**

Append these test methods to `CPPConversionPresetCatalogTests`:

```csharp
/// <summary>
/// Ensures the Wii core-boot preset resolves to the expected compiler, platform, feature, and restriction settings.
/// </summary>
[Fact]
public void Resolve_WiiCoreBoot_UsesNamedPresetProfiles() {
    CPPConversionPreset preset = new CPPConversionPresetCatalog().Resolve("wii-core-boot");

    Assert.Equal("wii-core-boot", preset.Id);
    Assert.Equal("gcc", preset.CompilerProfile.Name);
    Assert.Equal("wii-headless", preset.PlatformProfile.Name);
    Assert.Equal(CPPPlatformKind.WiiHeadless, preset.PlatformProfile.Kind);
    Assert.Equal("stl-lite", preset.RuntimeProfile.Name);
    Assert.Equal(CPPFeatureMode.Disabled, preset.BuildFeatureProfile.GetMode("shaders", CPPFeatureMode.Auto));
    Assert.Equal(CPPFeatureMode.Disabled, preset.BuildFeatureProfile.GetMode("debug_overlay", CPPFeatureMode.Auto));
    Assert.Equal("wii-core-boot", preset.RestrictionProfile.Name);
    Assert.True(preset.RestrictionProfile.ForbidShaders);
    Assert.True(preset.RestrictionProfile.ForbidRuntimeJson);
    Assert.True(preset.RestrictionProfile.ForbidReflectionLikeRuntime);
    Assert.True(preset.RestrictionProfile.ForbidDebugOnlySystems);
}

/// <summary>
/// Ensures the Wii preset uses the same native column-vector generated math convention as the GX GameCube target.
/// </summary>
[Fact]
public void Resolve_WiiCoreBoot_UsesNativeColumnVectorMathConvention() {
    CPPConversionPreset preset = new CPPConversionPresetCatalog().Resolve("wii-core-boot");

    Assert.Equal(CPPGeneratedMathConventionKind.NativeColumnVector, preset.PlatformProfile.GeneratedMathConvention);
}

/// <summary>
/// Ensures the Wii core-boot preset carries only the reflection-disable preprocessor symbols required by the stripped runtime.
/// </summary>
[Fact]
public void ApplyTo_WiiCoreBoot_AddsReflectionDisableSymbols() {
    CPPConversionOptions options = new CPPConversionOptions {
        PresetId = "wii-core-boot"
    };

    new CPPConversionPresetCatalog().ApplyTo(options);

    Assert.False(options.IncludeProjectDefinedPreprocessorSymbols);
    Assert.DoesNotContain("HELENGINE_RUNTIME_MATERIAL_RESOLUTION_COOKED_PLATFORM_OWNED", options.AdditionalPreprocessorSymbols);
    Assert.Contains("HELENGINE_CODEGEN_DISABLE_RUNTIME_SCRIPT_REFLECTION", options.AdditionalPreprocessorSymbols);
    Assert.Contains("HELENGINE_CODEGEN_DISABLE_MENU_REFLECTION", options.AdditionalPreprocessorSymbols);
}
```

- [ ] **Step 2: Add Wii generated config test**

Append this test method to `CPPGeneratedConfigWriterTests`:

```csharp
/// <summary>
/// Ensures the generated config writer emits GCC and Wii metadata for Wii-targeted conversions.
/// </summary>
[Fact]
public void Write_WithWiiProfile_WritesWiiDefines() {
    CPPConversionOptions options = CPPConversionOptions.CreateWiiDefault();
    CPPConversionReport report = new CPPConversionReport();
    CPPRuntimeRequirementRegistrar registrar = new CPPRuntimeRequirementRegistrar(new CPPRuntimeRequirementCatalog(), report);
    registrar.RegisterDefaults(options);

    string outputFolder = Path.Combine(Path.GetTempPath(), "cs2.cpp.tests", Guid.NewGuid().ToString("N"));
    string filePath = CPPGeneratedConfigWriter.Write(outputFolder, options, registrar);
    string output = File.ReadAllText(filePath);

    Assert.Contains("#define HE_CPP_COMPILER_GCC 1", output);
    Assert.Contains("#define HE_CPP_PLATFORM_WII 1", output);
    Assert.Contains("#define HE_CPP_RUNTIME_STL_LITE 1", output);
    Assert.Contains("#define HE_CPP_PLATFORM_IS_LITTLE_ENDIAN 0", output);
    Assert.Contains("#define HE_CPP_PLATFORM_IS_WINDOWS_HOST 0", output);
}
```

- [ ] **Step 3: Run the new tests and verify they fail**

Run:

```bash
rtk dotnet test C:\dev\helworks\csharpcodegen\cs2.cpp.tests\cs2.cpp.tests.csproj --filter "FullyQualifiedName~WiiCoreBoot|FullyQualifiedName~Write_WithWiiProfile" --no-restore
```

Expected: failures mention unknown preset `wii-core-boot` and missing `CPPConversionOptions.CreateWiiDefault`.

### Task 2: Implement Wii Codegen Platform Support

**Files:**
- Modify: `C:\dev\helworks\csharpcodegen\cs2.cpp\model\CPPPlatformProfile.cs`
- Modify: `C:\dev\helworks\csharpcodegen\cs2.cpp\model\CPPConversionOptions.cs`
- Modify: `C:\dev\helworks\csharpcodegen\cs2.cpp\CPPConversionPresetCatalog.cs`
- Modify: `C:\dev\helworks\csharpcodegen\codegen\CodegenCliOptionsBuilder.cs`

- [ ] **Step 1: Add the Wii platform profile factory**

Add this method to `CPPPlatformProfile` after `CreateGameCubeHeadless()`:

```csharp
/// <summary>
/// Creates the default Nintendo Wii headless development profile.
/// </summary>
/// <returns>The default Nintendo Wii platform profile.</returns>
public static CPPPlatformProfile CreateWiiHeadless() {
    return new CPPPlatformProfile {
        Kind = CPPPlatformKind.WiiHeadless,
        Name = "wii-headless",
        DefineName = "HE_CPP_PLATFORM_WII",
        IsLittleEndian = false,
        IsWindowsHost = false,
        GeneratedMathConvention = CPPGeneratedMathConventionKind.NativeColumnVector,
        PointerSizeInBytes = 4
    };
}
```

- [ ] **Step 2: Add the Wii conversion options factory**

Add this method to `CPPConversionOptions` after `CreateGameCubeDefault()`:

```csharp
/// <summary>
/// Creates the default option set for the first Wii headless milestone.
/// </summary>
/// <returns>The default Wii conversion options.</returns>
public static CPPConversionOptions CreateWiiDefault() {
    return new CPPConversionOptions {
        CompilerProfile = CPPCompilerProfile.CreateGcc(),
        PlatformProfile = CPPPlatformProfile.CreateWiiHeadless(),
        RuntimeProfile = CPPRuntimeProfile.CreateStlLite(),
        CollectDiagnostics = true,
        BuildFeatureProfile = CPPBuildFeatureProfile.CreateDefault(),
        LoadNativeRuntimeMetadata = true
    };
}
```

- [ ] **Step 3: Resolve the `wii-core-boot` preset id**

Add this branch to `CPPConversionPresetCatalog.Resolve` immediately after the `gamecube-core-boot` branch:

```csharp
if (string.Equals(presetId, "wii-core-boot", StringComparison.OrdinalIgnoreCase)) {
    return CreateWiiCoreBootPreset();
}
```

- [ ] **Step 4: Add the Wii preset factory**

Add this method to `CPPConversionPresetCatalog` after `CreateGameCubeCoreBootPreset()`:

```csharp
/// <summary>
/// Creates the first Wii preset used for generated-core boot validation.
/// </summary>
/// <returns>The resolved Wii core-boot preset.</returns>
static CPPConversionPreset CreateWiiCoreBootPreset() {
    CPPBuildFeatureProfile featureProfile = CPPBuildFeatureProfile.CreateDefault()
        .WithMode("shaders", CPPFeatureMode.Disabled)
        .WithMode("debug_overlay", CPPFeatureMode.Disabled);

    return new CPPConversionPreset {
        Id = "wii-core-boot",
        CompilerProfile = CPPCompilerProfile.CreateGcc(),
        PlatformProfile = CPPPlatformProfile.CreateWiiHeadless(),
        RuntimeProfile = CPPRuntimeProfile.CreateStlLite(),
        BuildFeatureProfile = featureProfile,
        RestrictionProfile = new CPPRestrictionProfile {
            Name = "wii-core-boot",
            ForbidShaders = true,
            ForbidRuntimeJson = true,
            ForbidReflectionLikeRuntime = true,
            ForbidDebugOnlySystems = true
        },
        IncludeProjectDefinedPreprocessorSymbols = false,
        AdditionalPreprocessorSymbols = new[] {
            "HELENGINE_CODEGEN_DISABLE_RUNTIME_SCRIPT_REFLECTION",
            "HELENGINE_CODEGEN_DISABLE_MENU_REFLECTION"
        }
    };
}
```

- [ ] **Step 5: Map the CLI `--platform wii` id**

Add this branch to `CodegenCliOptionsBuilder.CreatePlatformProfile` after the GameCube branch:

```csharp
} else if (string.Equals(platformId, "wii", StringComparison.OrdinalIgnoreCase)) {
    platformProfile = CPPPlatformProfile.CreateWiiHeadless();
```

- [ ] **Step 6: Run the targeted codegen tests**

Run:

```bash
rtk dotnet test C:\dev\helworks\csharpcodegen\cs2.cpp.tests\cs2.cpp.tests.csproj --filter "FullyQualifiedName~WiiCoreBoot|FullyQualifiedName~Write_WithWiiProfile" --no-restore
```

Expected: all targeted Wii tests pass.

- [ ] **Step 7: Run the existing GameCube preset/config tests for regression coverage**

Run:

```bash
rtk dotnet test C:\dev\helworks\csharpcodegen\cs2.cpp.tests\cs2.cpp.tests.csproj --filter "FullyQualifiedName~GameCubeCoreBoot|FullyQualifiedName~Write_WithGameCubeProfile" --no-restore
```

Expected: all targeted GameCube tests pass.

- [ ] **Step 8: Commit the codegen platform support**

Run:

```bash
rtk git -C C:\dev\helworks\csharpcodegen add cs2.cpp\model\CPPPlatformProfile.cs cs2.cpp\model\CPPConversionOptions.cs cs2.cpp\CPPConversionPresetCatalog.cs codegen\CodegenCliOptionsBuilder.cs cs2.cpp.tests\CPPConversionPresetCatalogTests.cs cs2.cpp.tests\CPPGeneratedConfigWriterTests.cs
rtk git -C C:\dev\helworks\csharpcodegen commit -m "Add Wii generated-core codegen preset"
```

Expected: commit succeeds with only the codegen files above.

### Task 3: Teach The Wii Makefile To Compile Generated Core

**Files:**
- Modify: `Makefile`

- [ ] **Step 1: Update the source lists and verification setting**

Replace the current `SOURCES` / `OBJECTS` block with this structure:

```makefile
HELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT ?= 0

BASE_SOURCES := \
	$(SOURCE_DIR)/main.cpp \
	$(SOURCE_DIR)/platform/wii/WiiApplication.cpp

GENERATED_BRIDGE_SOURCES :=
GENERATED_CORE_SOURCE :=
GENERATED_CORE_TRANSLATION_UNIT :=
GENERATED_CONFIG := $(HELENGINE_CORE_CPP_ROOT)/helcpp_config.hpp
```

- [ ] **Step 2: Replace the generated-core macro block**

Replace the existing `ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)` block with:

```makefile
ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CPPFLAGS += -DHELENGINE_WII_HAS_GENERATED_CORE=0
else
ifneq ($(wildcard $(HELENGINE_CORE_CPP_ROOT)/helengine_core_amalgamated.cpp),)
GENERATED_CORE_TRANSLATION_UNIT := helengine_core_amalgamated.cpp
else ifneq ($(wildcard $(HELENGINE_CORE_CPP_ROOT)/helengine_core_unity.cpp),)
GENERATED_CORE_TRANSLATION_UNIT := helengine_core_unity.cpp
else
$(error HELENGINE_CORE_CPP_ROOT does not contain helengine_core_amalgamated.cpp or helengine_core_unity.cpp)
endif
GENERATED_CORE_SOURCE := $(HELENGINE_CORE_CPP_ROOT)/$(GENERATED_CORE_TRANSLATION_UNIT)
ifeq ($(wildcard $(GENERATED_CONFIG)),)
$(error HELENGINE_CORE_CPP_ROOT does not contain helcpp_config.hpp)
endif
ifeq ($(shell tr -d '\r' < $(GENERATED_CONFIG) 2>/dev/null | grep -Ec '^#define HE_CPP_COMPILER_GCC 1$$'),0)
$(error HELENGINE_CORE_CPP_ROOT helcpp_config.hpp must define HE_CPP_COMPILER_GCC 1)
endif
ifeq ($(shell tr -d '\r' < $(GENERATED_CONFIG) 2>/dev/null | grep -Ec '^#define HE_CPP_PLATFORM_WII 1$$'),0)
$(error HELENGINE_CORE_CPP_ROOT helcpp_config.hpp must define HE_CPP_PLATFORM_WII 1)
endif
ifeq ($(shell tr -d '\r' < $(GENERATED_CONFIG) 2>/dev/null | grep -Ec '^#define HE_CPP_PLATFORM_IS_LITTLE_ENDIAN 0$$'),0)
$(error HELENGINE_CORE_CPP_ROOT helcpp_config.hpp must define HE_CPP_PLATFORM_IS_LITTLE_ENDIAN 0)
endif
ifeq ($(shell tr -d '\r' < $(GENERATED_CONFIG) 2>/dev/null | grep -Ec '^#define HE_CPP_PLATFORM_IS_WINDOWS_HOST 0$$'),0)
$(error HELENGINE_CORE_CPP_ROOT helcpp_config.hpp must define HE_CPP_PLATFORM_IS_WINDOWS_HOST 0)
endif
GENERATED_BRIDGE_SOURCES := \
	$(SOURCE_DIR)/platform/wii/WiiInputManager.cpp \
	$(SOURCE_DIR)/platform/wii/WiiRenderManager2D.cpp \
	$(SOURCE_DIR)/platform/wii/WiiRenderManager3D.cpp
CPPFLAGS += -DHELENGINE_WII_HAS_GENERATED_CORE=1 -I$(HELENGINE_CORE_CPP_ROOT)
endif
```

- [ ] **Step 3: Add the batch verification macro and object list**

Keep the existing Wii/Gekko defines and add:

```makefile
CPPFLAGS += -DHELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT=$(HELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT)

ALL_SOURCE_SOURCES := $(BASE_SOURCES) $(GENERATED_BRIDGE_SOURCES)
OBJECTS := $(patsubst $(SOURCE_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ALL_SOURCE_SOURCES))

ifneq ($(strip $(GENERATED_CORE_SOURCE)),)
OBJECTS += $(BUILD_DIR)/generated/$(GENERATED_CORE_TRANSLATION_UNIT:.cpp=.o)
endif
```

- [ ] **Step 4: Use the same C++ dialect as the generated GameCube build**

Change the `CXXFLAGS` standard line to:

```makefile
	-std=gnu++20 \
```

- [ ] **Step 5: Add the generated-core object rule**

Add this rule after the normal C++ object rule:

```makefile
$(BUILD_DIR)/generated/$(GENERATED_CORE_TRANSLATION_UNIT:.cpp=.o): $(GENERATED_CORE_SOURCE)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
```

- [ ] **Step 6: Verify the no-generated-core Makefile path still builds the current source list**

Run:

```bash
rtk docker run --rm -v /mnt/c/dev/helworks/helengine-wii:/workspace -w /workspace helengine-wii make clean all
```

Expected: build fails only because `WiiApplication.cpp` does not exist yet. The Makefile must not report generated-core validation errors when `HELENGINE_CORE_CPP_ROOT` is unset.

### Task 4: Add The Wii Application Host

**Files:**
- Modify: `src/main.cpp`
- Create: `src/platform/wii/WiiBootPhase.hpp`
- Create: `src/platform/wii/WiiApplication.hpp`
- Create: `src/platform/wii/WiiApplication.cpp`

- [ ] **Step 1: Replace the entry point**

Replace `src/main.cpp` with:

```cpp
#include "platform/wii/WiiApplication.hpp"

int main() {
    helengine::wii::WiiApplication application;
    return application.Run();
}
```

- [ ] **Step 2: Add boot phase enum**

Create `src/platform/wii/WiiBootPhase.hpp`:

```cpp
#pragma once

namespace helengine::wii {
    /// Identifies the visible runtime startup phase reported through diagnostic clear colors.
    enum class WiiBootPhase {
        /// Native video and GX setup has not completed yet.
        NativeStartup,

        /// The generated core object is being constructed.
        CoreConstruction,

        /// Core initialization options are being read and configured.
        CoreOptions,

        /// Wii bridge services are being constructed.
        BridgeConstruction,

        /// The generated core is receiving its initialization call.
        CoreInitialization,

        /// The generated core has initialized and the runtime frame loop is active.
        Running,

        /// The generated core update step is active.
        CoreUpdate,

        /// The generated core draw step is active.
        CoreDraw,

        /// The runtime failed and the visible frame should remain on a failure color.
        Failed
    };
}
```

- [ ] **Step 3: Add the Wii application interface**

Create `src/platform/wii/WiiApplication.hpp`:

```cpp
#pragma once

#include <cstdint>

#include <gccore.h>

#include "platform/wii/WiiBootPhase.hpp"

class Core;
class PlatformInfo;

namespace helengine::wii {
    class WiiInputManager;
    class WiiRenderManager2D;
    class WiiRenderManager3D;

    /// Owns Wii host startup, optional generated-core boot, and the steady-state frame loop.
    class WiiApplication {
    public:
        /// Creates the Wii application with no initialized native or engine state.
        WiiApplication();

        /// Releases generated-core bridge objects after the application loop finishes.
        ~WiiApplication();

        /// Initializes the native host and enters the steady-state frame loop.
        int Run();

    private:
        /// Initializes the VI display state and allocates the external framebuffers.
        bool InitializeVideo();

        /// Initializes GX for the host clear-and-present loop.
        bool InitializeGraphics();

        /// Initializes the generated engine core when generated sources are present in the build.
        bool InitializeEngineCore();

        /// Advances one engine frame when the generated core was initialized successfully.
        bool UpdateEngineCore();

        /// Draws one engine frame when the generated core was initialized successfully.
        bool DrawEngineCore();

        /// Presents one fallback or generated frame to the active framebuffer.
        void PresentFrame();

        /// Resolves the currently visible diagnostic color for the next presented frame.
        GXColor ResolvePresentedClearColor();

        /// Sets the current boot phase and visible clear color.
        void SetBootPhase(WiiBootPhase phase, GXColor color);

        /// Marks the current boot phase as failed and updates the visible clear color.
        void FailBootPhase(WiiBootPhase phase, GXColor color);

        /// Returns whether runtime verification has presented the requested number of generated frames.
        bool HasSatisfiedVerificationExitCondition() const;

        /// Stores the preferred video mode selected for the current console or emulator.
        GXRModeObj* RenderMode;

        /// Stores the allocated external framebuffers used for display output.
        void* FrameBuffers[2];

        /// Stores the index of the next external framebuffer that will receive the copied display image.
        uint32_t FrameBufferIndex;

        /// Stores the GX command FIFO allocation used by the renderer bootstrap.
        void* FifoBuffer;

        /// Stores the current fallback clear color for boot-state diagnostics.
        GXColor ClearColor;

        /// Stores the current host boot phase.
        WiiBootPhase BootPhase;

        /// Tracks whether the generated engine core finished initialization.
        bool EngineInitialized;

        /// Counts the number of frames presented after generated-core initialization succeeds.
        uint32_t PresentedFrameCount;

        /// Counts the number of generated-core frames that completed both update and draw.
        uint32_t VerifiedFrameCount;

        /// Tracks whether the current frame completed the generated update step before presentation.
        bool UpdateCompletedSincePresent;

        /// Tracks whether the current frame completed the generated draw step before presentation.
        bool DrawCompletedSincePresent;

#if HELENGINE_WII_HAS_GENERATED_CORE
        /// Stores the generated engine core instance when the build includes generated sources.
        Core* EngineCore;

        /// Stores the generated 3D render manager bridge.
        WiiRenderManager3D* EngineRenderManager3D;

        /// Stores the generated 2D render manager bridge.
        WiiRenderManager2D* EngineRenderManager2D;

        /// Stores the generated input manager bridge.
        WiiInputManager* EngineInputManager;

        /// Stores the platform descriptor passed into the generated core initialization contract.
        PlatformInfo* EnginePlatformInfo;
#endif
    };
}
```

- [ ] **Step 4: Add the Wii application implementation**

Create `src/platform/wii/WiiApplication.cpp` with the pink-screen-native path first:

```cpp
#include "platform/wii/WiiApplication.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <malloc.h>

#if HELENGINE_WII_HAS_GENERATED_CORE
#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "Exception.hpp"
#include "PlatformInfo.hpp"
#include "platform/wii/WiiInputManager.hpp"
#include "platform/wii/WiiRenderManager2D.hpp"
#include "platform/wii/WiiRenderManager3D.hpp"
#endif

namespace helengine::wii {
    namespace {
        constexpr std::size_t DefaultFifoSize = 256 * 1024;
        constexpr GXColor PinkClearColor { 0xFF, 0x69, 0xB4, 0xFF };
        constexpr GXColor FailureClearColor { 0xFF, 0x00, 0x00, 0xFF };
    }

    /// Creates the Wii application with no initialized native or engine state.
    WiiApplication::WiiApplication()
        : RenderMode(nullptr)
        , FrameBuffers { nullptr, nullptr }
        , FrameBufferIndex(0)
        , FifoBuffer(nullptr)
        , ClearColor(PinkClearColor)
        , BootPhase(WiiBootPhase::NativeStartup)
        , EngineInitialized(false)
        , PresentedFrameCount(0)
        , VerifiedFrameCount(0)
        , UpdateCompletedSincePresent(false)
        , DrawCompletedSincePresent(false)
#if HELENGINE_WII_HAS_GENERATED_CORE
        , EngineCore(nullptr)
        , EngineRenderManager3D(nullptr)
        , EngineRenderManager2D(nullptr)
        , EngineInputManager(nullptr)
        , EnginePlatformInfo(nullptr)
#endif
    {
    }

    /// Releases generated-core bridge objects after the application loop finishes.
    WiiApplication::~WiiApplication() {
#if HELENGINE_WII_HAS_GENERATED_CORE
        delete EngineInputManager;
        delete EngineRenderManager2D;
        delete EngineRenderManager3D;
        delete EnginePlatformInfo;
        delete EngineCore;
#endif

        if (FifoBuffer != nullptr) {
            std::free(FifoBuffer);
        }
    }

    /// Initializes the native host and enters the steady-state frame loop.
    int WiiApplication::Run() {
        if (!InitializeVideo()) {
            return 1;
        }

        if (!InitializeGraphics()) {
            return 1;
        }

#if HELENGINE_WII_HAS_GENERATED_CORE
        if (!InitializeEngineCore()) {
            while (true) {
                PresentFrame();
            }
        }
#endif

        while (true) {
#if HELENGINE_WII_HAS_GENERATED_CORE
            if (!UpdateEngineCore()) {
                PresentFrame();
                return 1;
            }

            if (!DrawEngineCore()) {
                PresentFrame();
                return 1;
            }
#endif
            PresentFrame();
            if (HasSatisfiedVerificationExitCondition()) {
                return 0;
            }
        }
    }

    /// Initializes the VI display state and allocates the external framebuffers.
    bool WiiApplication::InitializeVideo() {
        VIDEO_Init();

        RenderMode = VIDEO_GetPreferredMode(nullptr);
        if (RenderMode == nullptr) {
            return false;
        }

        FrameBuffers[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(RenderMode));
        FrameBuffers[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(RenderMode));
        if (FrameBuffers[0] == nullptr || FrameBuffers[1] == nullptr) {
            return false;
        }

        VIDEO_Configure(RenderMode);
        VIDEO_SetNextFramebuffer(FrameBuffers[FrameBufferIndex]);
        VIDEO_SetBlack(FALSE);
        VIDEO_Flush();
        VIDEO_WaitVSync();

        if (RenderMode->viTVMode & VI_NON_INTERLACE) {
            VIDEO_WaitVSync();
        }

        return true;
    }

    /// Initializes GX for the host clear-and-present loop.
    bool WiiApplication::InitializeGraphics() {
        FifoBuffer = memalign(32, DefaultFifoSize);
        if (FifoBuffer == nullptr) {
            return false;
        }

        std::memset(FifoBuffer, 0, DefaultFifoSize);
        GX_Init(FifoBuffer, DefaultFifoSize);

        const f32 yScale = GX_GetYScaleFactor(RenderMode->efbHeight, RenderMode->xfbHeight);
        const u16 xfbHeight = GX_SetDispCopyYScale(yScale);

        GX_SetScissor(0, 0, RenderMode->fbWidth, RenderMode->efbHeight);
        GX_SetDispCopySrc(0, 0, RenderMode->fbWidth, RenderMode->efbHeight);
        GX_SetDispCopyDst(RenderMode->fbWidth, xfbHeight);
        GX_SetCopyFilter(RenderMode->aa, RenderMode->sample_pattern, GX_TRUE, RenderMode->vfilter);
        GX_SetFieldMode(RenderMode->field_rendering, ((RenderMode->viHeight == (RenderMode->xfbHeight * 2)) ? GX_ENABLE : GX_DISABLE));
        GX_SetCullMode(GX_CULL_NONE);
        GX_SetDispCopyGamma(GX_GM_1_0);
        GX_SetNumChans(1);
        GX_SetNumTexGens(0);
        GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        GX_SetColorUpdate(GX_TRUE);
        GX_SetAlphaUpdate(GX_FALSE);
        GX_SetViewport(0.0F, 0.0F, static_cast<f32>(RenderMode->fbWidth), static_cast<f32>(RenderMode->efbHeight), 0.0F, 1.0F);
        GX_InvVtxCache();
        GX_InvalidateTexAll();
        return true;
    }
```

- [ ] **Step 5: Add temporary generated-core method stubs**

Append these methods to `WiiApplication.cpp`. Task 6 replaces the generated-core branches with real implementation.

```cpp
    /// Initializes the generated engine core when generated sources are present in the build.
    bool WiiApplication::InitializeEngineCore() {
        return true;
    }

    /// Advances one engine frame when the generated core was initialized successfully.
    bool WiiApplication::UpdateEngineCore() {
        return true;
    }

    /// Draws one engine frame when the generated core was initialized successfully.
    bool WiiApplication::DrawEngineCore() {
        return true;
    }

    /// Presents one fallback or generated frame to the active framebuffer.
    void WiiApplication::PresentFrame() {
        GX_SetCopyClear(ResolvePresentedClearColor(), 0x00FFFFFF);
        FrameBufferIndex ^= 1U;
        GX_CopyDisp(FrameBuffers[FrameBufferIndex], GX_TRUE);
        GX_DrawDone();
        GX_Flush();
        VIDEO_SetNextFramebuffer(FrameBuffers[FrameBufferIndex]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        PresentedFrameCount++;
    }

    /// Resolves the currently visible diagnostic color for the next presented frame.
    GXColor WiiApplication::ResolvePresentedClearColor() {
        return ClearColor;
    }

    /// Sets the current boot phase and visible clear color.
    void WiiApplication::SetBootPhase(WiiBootPhase phase, GXColor color) {
        BootPhase = phase;
        ClearColor = color;
    }

    /// Marks the current boot phase as failed and updates the visible clear color.
    void WiiApplication::FailBootPhase(WiiBootPhase phase, GXColor color) {
        BootPhase = phase;
        ClearColor = color;
    }

    /// Returns whether runtime verification has presented the requested number of generated frames.
    bool WiiApplication::HasSatisfiedVerificationExitCondition() const {
        return HELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT > 0
            && VerifiedFrameCount >= static_cast<uint32_t>(HELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT);
    }
}
```

- [ ] **Step 6: Build the non-generated host**

Run:

```bash
rtk docker run --rm -v /mnt/c/dev/helworks/helengine-wii:/workspace -w /workspace helengine-wii make clean all
```

Expected: `build/helengine_wii.dol` is emitted with `HELENGINE_WII_HAS_GENERATED_CORE=0`.

- [ ] **Step 7: Commit the Wii application host**

Run:

```bash
rtk git add Makefile src/main.cpp src/platform/wii/WiiBootPhase.hpp src/platform/wii/WiiApplication.hpp src/platform/wii/WiiApplication.cpp
rtk git commit -m "Add Wii application runtime host"
```

Expected: commit succeeds with only the listed Wii files.

### Task 5: Add Minimal Wii Generated-Core Bridge Services

**Files:**
- Create: `src/platform/wii/WiiInputManager.hpp`
- Create: `src/platform/wii/WiiInputManager.cpp`
- Create: `src/platform/wii/WiiRenderManager2D.hpp`
- Create: `src/platform/wii/WiiRenderManager2D.cpp`
- Create: `src/platform/wii/WiiRenderManager3D.hpp`
- Create: `src/platform/wii/WiiRenderManager3D.cpp`

- [ ] **Step 1: Add the Wii input manager header**

Create `src/platform/wii/WiiInputManager.hpp`:

```cpp
#pragma once

#include "IInputBackend.hpp"
#include "InputFrameState.hpp"

namespace helengine::wii {
    /// Implements the generated input backend contract for the bootstrap Wii host.
    class WiiInputManager : public IInputBackend {
    public:
        /// Creates the Wii input backend and initializes libogc controller polling.
        WiiInputManager();

        /// Releases the Wii input backend.
        ~WiiInputManager();

        /// Captures one bootstrap input frame with one optional controller state.
        InputFrameState CaptureFrame() override;
    };
}
```

- [ ] **Step 2: Add the Wii input manager implementation**

Create `src/platform/wii/WiiInputManager.cpp`:

```cpp
#include "platform/wii/WiiInputManager.hpp"

#include <gccore.h>

#include "InputGamepadButton.hpp"
#include "InputGamepadState.hpp"
#include "runtime/array.hpp"

namespace helengine::wii {
    /// Creates the Wii input backend and initializes libogc controller polling.
    WiiInputManager::WiiInputManager() {
        PAD_Init();
    }

    /// Releases the Wii input backend.
    WiiInputManager::~WiiInputManager() {
    }

    /// Captures one bootstrap input frame with one optional controller state.
    InputFrameState WiiInputManager::CaptureFrame() {
        PAD_ScanPads();

        InputFrameState frame;
        frame.set_GamepadCount(1);

        Array<InputGamepadState>* gamepads = new Array<InputGamepadState>(1);
        InputGamepadState gamepadState;

        const s32 probeResult = PAD_Probe(0);
        const bool isConnected = probeResult == PAD_ERR_NONE;
        gamepadState.set_Connected(isConnected);

        if (isConnected) {
            const u16 heldButtons = PAD_ButtonsHeld(0);
            gamepadState.SetButtonDown(InputGamepadButton::DPadUp, (heldButtons & PAD_BUTTON_UP) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::DPadDown, (heldButtons & PAD_BUTTON_DOWN) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::DPadLeft, (heldButtons & PAD_BUTTON_LEFT) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::DPadRight, (heldButtons & PAD_BUTTON_RIGHT) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::South, (heldButtons & PAD_BUTTON_A) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::East, (heldButtons & PAD_BUTTON_B) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::West, (heldButtons & PAD_BUTTON_X) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::North, (heldButtons & PAD_BUTTON_Y) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::LeftShoulder, (heldButtons & PAD_TRIGGER_L) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::RightShoulder, (heldButtons & PAD_TRIGGER_R) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::Start, (heldButtons & PAD_BUTTON_START) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::Select, (heldButtons & PAD_TRIGGER_Z) != 0);
            gamepadState.set_LeftStickX(static_cast<int16_t>(PAD_StickX(0) * 256));
            gamepadState.set_LeftStickY(static_cast<int16_t>(PAD_StickY(0) * 256));
            gamepadState.set_RightStickX(static_cast<int16_t>(PAD_SubStickX(0) * 256));
            gamepadState.set_RightStickY(static_cast<int16_t>(PAD_SubStickY(0) * 256));
            gamepadState.set_LeftTrigger(static_cast<int16_t>(PAD_TriggerL(0) * 256));
            gamepadState.set_RightTrigger(static_cast<int16_t>(PAD_TriggerR(0) * 256));
        }

        (*gamepads)[0] = gamepadState;
        frame.set_Gamepads(gamepads);
        return frame;
    }
}
```

- [ ] **Step 3: Add the Wii 2D render manager header**

Create `src/platform/wii/WiiRenderManager2D.hpp`:

```cpp
#pragma once

#include <vector>

#include "IRenderVisitor2D.hpp"
#include "RenderManager2D.hpp"

class RuntimeTexture;
class IDrawable2D;

namespace helengine::wii {
    /// Stores one captured sprite draw request for the current Wii 2D frame.
    struct WiiSpriteDrawCommand {
        /// Pointer to the shared-engine sprite drawable submitted during the current frame.
        ISpriteDrawable2D* Drawable;
    };

    /// Stores one captured text draw request for the current Wii 2D frame.
    struct WiiTextDrawCommand {
        /// Pointer to the shared-engine text drawable submitted during the current frame.
        ITextDrawable2D* Drawable;
    };

    /// Stores one captured rounded-rectangle draw request for the current Wii 2D frame.
    struct WiiRoundedRectDrawCommand {
        /// Pointer to the shared-engine rounded-rectangle drawable submitted during the current frame.
        IRoundedRectDrawable2D* Drawable;
    };

    /// Implements the Wii 2D render bridge by capturing shared-engine draw requests for a later GX overlay pass.
    class WiiRenderManager2D : public RenderManager2D, public IRenderVisitor2D {
    public:
        /// Creates the Wii 2D render bridge.
        WiiRenderManager2D();

        /// Fails because raw texture creation is outside the generated-core boot slice.
        RuntimeTexture* BuildTextureFromRaw(TextureAsset* data) override;

        /// Fails because cooked texture loading is outside the generated-core boot slice.
        RuntimeTexture* BuildTextureFromCooked(std::string cookedAssetPath) override;

        /// Releases one Wii runtime texture.
        void ReleaseTexture(RuntimeTexture* texture) override;

        /// Releases one font asset.
        void ReleaseFont(FontAsset* font) override;

        /// Accepts a sprite draw request without issuing native rendering yet.
        void DrawSprite(ISpriteDrawable2D* sprite) override;

        /// Accepts a text draw request without issuing native rendering yet.
        void DrawText(ITextDrawable2D* text) override;

        /// Accepts a rounded-rectangle draw request without issuing native rendering yet.
        void DrawRoundedRect(IRoundedRectDrawable2D* shape) override;

        /// Walks the active camera 2D queue and lets each drawable submit itself into this frame capture.
        void Draw() override;

        /// Visits one ordered 2D drawable from the active camera queue.
        void Visit(IDrawable2D* drawable) override;

        /// Clears deferred release state after an engine frame.
        void FlushReleasedTextures() override;

        /// Clears previously captured 2D draw requests before the next engine frame begins.
        void BeginFrame();

        /// Returns whether the current frame captured any 2D draw requests.
        bool HasCapturedDrawables() const;

    private:
        /// Captured sprite draw requests in shared-engine render order.
        std::vector<WiiSpriteDrawCommand> SpriteQueue;

        /// Captured text draw requests in shared-engine render order.
        std::vector<WiiTextDrawCommand> TextQueue;

        /// Captured rounded-rectangle draw requests in shared-engine render order.
        std::vector<WiiRoundedRectDrawCommand> RoundedRectQueue;
    };
}
```

- [ ] **Step 4: Add the Wii 2D render manager implementation**

Create `src/platform/wii/WiiRenderManager2D.cpp`:

```cpp
#include "platform/wii/WiiRenderManager2D.hpp"

#include "CameraComponent.hpp"
#include "Core.hpp"
#include "FontAsset.hpp"
#include "ICamera.hpp"
#include "IDrawable2D.hpp"
#include "ObjectManager.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/native_exceptions.hpp"

namespace helengine::wii {
    /// Creates the Wii 2D render bridge.
    WiiRenderManager2D::WiiRenderManager2D()
        : RenderManager2D() {
    }

    /// Fails because raw texture creation is outside the generated-core boot slice.
    RuntimeTexture* WiiRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        }

        throw new InvalidOperationException("Wii generated-core boot does not support raw texture creation yet.");
    }

    /// Fails because cooked texture loading is outside the generated-core boot slice.
    RuntimeTexture* WiiRenderManager2D::BuildTextureFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Wii cooked texture path is required.", "cookedAssetPath");
        }

        throw new InvalidOperationException("Wii generated-core boot does not support cooked texture loading yet.");
    }

    /// Releases one Wii runtime texture.
    void WiiRenderManager2D::ReleaseTexture(RuntimeTexture* texture) {
        if (texture == nullptr) {
            throw new ArgumentNullException("texture");
        }
    }

    /// Releases one font asset.
    void WiiRenderManager2D::ReleaseFont(FontAsset* font) {
        if (font == nullptr) {
            throw new ArgumentNullException("font");
        }

        font->Dispose();
        delete font;
    }

    /// Walks the active camera 2D queue and lets each drawable submit itself into this frame capture.
    void WiiRenderManager2D::Draw() {
        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr) {
            return;
        }

        List<ICamera*>* cameras = core->get_ObjectManager()->get_Cameras();
        for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++) {
            CameraComponent* camera = he_cpp_try_cast<CameraComponent>((*cameras)[cameraIndex]);
            if (camera == nullptr || camera->get_Parent() == nullptr || !camera->get_Parent()->get_IsHierarchyEnabled()) {
                continue;
            }

            camera->get_RenderQueue2D()->VisitOrdered(this);
            return;
        }
    }

    /// Visits one ordered 2D drawable from the active camera queue.
    void WiiRenderManager2D::Visit(IDrawable2D* drawable) {
        if (drawable == nullptr || drawable->get_Parent() == nullptr || !drawable->get_Parent()->get_IsHierarchyEnabled()) {
            return;
        }

        drawable->Draw();
    }

    /// Clears deferred release state after an engine frame.
    void WiiRenderManager2D::FlushReleasedTextures() {
    }

    /// Clears previously captured 2D draw requests before the next engine frame begins.
    void WiiRenderManager2D::BeginFrame() {
        SpriteQueue.clear();
        TextQueue.clear();
        RoundedRectQueue.clear();
    }

    /// Returns whether the current frame captured any 2D draw requests.
    bool WiiRenderManager2D::HasCapturedDrawables() const {
        return !SpriteQueue.empty() || !TextQueue.empty() || !RoundedRectQueue.empty();
    }

    /// Accepts a sprite draw request without issuing native rendering yet.
    void WiiRenderManager2D::DrawSprite(ISpriteDrawable2D* sprite) {
        if (sprite == nullptr) {
            throw new ArgumentNullException("sprite");
        }

        SpriteQueue.push_back(WiiSpriteDrawCommand { sprite });
    }

    /// Accepts a text draw request without issuing native rendering yet.
    void WiiRenderManager2D::DrawText(ITextDrawable2D* text) {
        if (text == nullptr) {
            throw new ArgumentNullException("text");
        }

        TextQueue.push_back(WiiTextDrawCommand { text });
    }

    /// Accepts a rounded-rectangle draw request without issuing native rendering yet.
    void WiiRenderManager2D::DrawRoundedRect(IRoundedRectDrawable2D* shape) {
        if (shape == nullptr) {
            throw new ArgumentNullException("shape");
        }

        RoundedRectQueue.push_back(WiiRoundedRectDrawCommand { shape });
    }
}
```

- [ ] **Step 5: Add the Wii 3D render manager header**

Create `src/platform/wii/WiiRenderManager3D.hpp`:

```cpp
#pragma once

#include "RenderManager3D.hpp"

class PlatformMaterialAsset;
class RuntimeMaterial;
class RuntimeModel;
class RendererBackendCapabilityProfile;
class MaterialLayout;
class ModelAsset;

namespace helengine::wii {
    class WiiRenderManager2D;

    /// Provides the minimal Wii 3D render bridge required for generated core update/draw boot validation.
    class WiiRenderManager3D : public RenderManager3D {
    public:
        /// Creates the Wii 3D render bridge.
        WiiRenderManager3D();

        /// Releases the Wii 3D render bridge.
        ~WiiRenderManager3D() override;

        /// Fails because material creation is outside the generated-core boot slice.
        RuntimeMaterial* BuildMaterialFromCooked(PlatformMaterialAsset* materialAsset) override;

        /// Fails because cooked material loading is outside the generated-core boot slice.
        RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath) override;

        /// Fails because raw model creation is outside the generated-core boot slice.
        RuntimeModel* BuildModelFromRaw(ModelAsset* data) override;

        /// Fails because cooked model loading is outside the generated-core boot slice.
        RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath) override;

        /// Releases one runtime material.
        void ReleaseMaterial(RuntimeMaterial* material) override;

        /// Releases one runtime model.
        void ReleaseModel(RuntimeModel* model) override;

        /// Clears deferred release state after an engine frame.
        void FlushReleasedAssets() override;

        /// Runs the generated draw path without native scene rasterization.
        void Draw() override;

        /// Registers the 2D overlay render manager used by the generated draw path.
        void SetOverlayRenderManager2D(WiiRenderManager2D* renderManager2D);

        /// Returns the strict backend capability surface exposed by the first Wii tier.
        RendererBackendCapabilityProfile* GetCapabilityProfile() override;

        /// Reports whether this backend has emitted a native scene frame.
        bool HasRenderedScene() const;

    private:
        /// Shared backend capability object reused across frame calls.
        RendererBackendCapabilityProfile* CapabilityProfile;

        /// Stores the 2D render manager whose captured overlay drawables are part of the draw boundary.
        WiiRenderManager2D* OverlayRenderManager2D;
    };
}
```

- [ ] **Step 6: Add the Wii 3D render manager implementation**

Create `src/platform/wii/WiiRenderManager3D.cpp`:

```cpp
#include "platform/wii/WiiRenderManager3D.hpp"

#include "RendererBackendCapabilityProfile.hpp"
#include "RuntimeMaterial.hpp"
#include "RuntimeModel.hpp"
#include "platform/wii/WiiRenderManager2D.hpp"
#include "runtime/native_exceptions.hpp"

namespace helengine::wii {
    /// Creates the Wii 3D render bridge.
    WiiRenderManager3D::WiiRenderManager3D()
        : RenderManager3D()
        , CapabilityProfile(new RendererBackendCapabilityProfile())
        , OverlayRenderManager2D(nullptr) {
    }

    /// Releases the Wii 3D render bridge.
    WiiRenderManager3D::~WiiRenderManager3D() {
        delete CapabilityProfile;
    }

    /// Fails because material creation is outside the generated-core boot slice.
    RuntimeMaterial* WiiRenderManager3D::BuildMaterialFromCooked(PlatformMaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw new ArgumentNullException("materialAsset");
        }

        throw new InvalidOperationException("Wii generated-core boot does not support material creation yet.");
    }

    /// Fails because cooked material loading is outside the generated-core boot slice.
    RuntimeMaterial* WiiRenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Wii cooked material path is required.", "cookedAssetPath");
        }

        throw new InvalidOperationException("Wii generated-core boot does not support cooked material loading yet.");
    }

    /// Fails because raw model creation is outside the generated-core boot slice.
    RuntimeModel* WiiRenderManager3D::BuildModelFromRaw(ModelAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        }

        throw new InvalidOperationException("Wii generated-core boot does not support raw model creation yet.");
    }

    /// Fails because cooked model loading is outside the generated-core boot slice.
    RuntimeModel* WiiRenderManager3D::BuildModelFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Wii cooked model path is required.", "cookedAssetPath");
        }

        throw new InvalidOperationException("Wii generated-core boot does not support cooked model loading yet.");
    }

    /// Releases one runtime material.
    void WiiRenderManager3D::ReleaseMaterial(RuntimeMaterial* material) {
        if (material == nullptr) {
            throw new ArgumentNullException("material");
        }
    }

    /// Releases one runtime model.
    void WiiRenderManager3D::ReleaseModel(RuntimeModel* model) {
        if (model == nullptr) {
            throw new ArgumentNullException("model");
        }
    }

    /// Clears deferred release state after an engine frame.
    void WiiRenderManager3D::FlushReleasedAssets() {
    }

    /// Runs the generated draw path without native scene rasterization.
    void WiiRenderManager3D::Draw() {
        if (OverlayRenderManager2D == nullptr) {
            throw new InvalidOperationException("WiiRenderManager3D requires an overlay WiiRenderManager2D before Draw().");
        }

        OverlayRenderManager2D->Draw();
    }

    /// Registers the 2D overlay render manager used by the generated draw path.
    void WiiRenderManager3D::SetOverlayRenderManager2D(WiiRenderManager2D* renderManager2D) {
        if (renderManager2D == nullptr) {
            throw new ArgumentNullException("renderManager2D");
        }

        OverlayRenderManager2D = renderManager2D;
    }

    /// Returns the strict backend capability surface exposed by the first Wii tier.
    RendererBackendCapabilityProfile* WiiRenderManager3D::GetCapabilityProfile() {
        return CapabilityProfile;
    }

    /// Reports whether this backend has emitted a native scene frame.
    bool WiiRenderManager3D::HasRenderedScene() const {
        return false;
    }
}
```

- [ ] **Step 7: Build with generated bridge files still disabled**

Run:

```bash
rtk docker run --rm -v /mnt/c/dev/helworks/helengine-wii:/workspace -w /workspace helengine-wii make clean all
```

Expected: non-generated build still succeeds because bridge files are only compiled when `HELENGINE_CORE_CPP_ROOT` is set.

- [ ] **Step 8: Commit the Wii bridge services**

Run:

```bash
rtk git add src/platform/wii/WiiInputManager.hpp src/platform/wii/WiiInputManager.cpp src/platform/wii/WiiRenderManager2D.hpp src/platform/wii/WiiRenderManager2D.cpp src/platform/wii/WiiRenderManager3D.hpp src/platform/wii/WiiRenderManager3D.cpp
rtk git commit -m "Add Wii generated-core bridge services"
```

Expected: commit succeeds with only the bridge service files.

### Task 6: Wire Generated Core Initialization And Frame Execution

**Files:**
- Modify: `src/platform/wii/WiiApplication.cpp`

- [ ] **Step 1: Replace `InitializeEngineCore` with real generated-core initialization**

Replace the stub with:

```cpp
    /// Initializes the generated engine core when generated sources are present in the build.
    bool WiiApplication::InitializeEngineCore() {
#if HELENGINE_WII_HAS_GENERATED_CORE
        const char* initializationStage = "BeforeCoreConstruction";
        try {
            initializationStage = "ConstructCore";
            SetBootPhase(WiiBootPhase::CoreConstruction, GXColor { 0xFF, 0xFF, 0x00, 0xFF });
            EngineCore = new Core();

            initializationStage = "ReadInitializationOptions";
            SetBootPhase(WiiBootPhase::CoreOptions, GXColor { 0xFF, 0x80, 0x00, 0xFF });
            CoreInitializationOptions* options = EngineCore->get_InitializationOptions();
            if (options == nullptr) {
                FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
                return false;
            }

            options->ContentRootPath = ".";
            options->SceneCatalog = nullptr;
            options->UpdateOrderLayers = 4;
            options->RenderOrderLayers3D = 4;
            options->UpdateListInitialCapacity = 64;
            options->RenderList2DInitialCapacity = 64;
            options->RenderList3DInitialCapacity = 64;

            initializationStage = "ConstructBridgeServices";
            SetBootPhase(WiiBootPhase::BridgeConstruction, GXColor { 0x00, 0xFF, 0xFF, 0xFF });
            EngineRenderManager3D = new WiiRenderManager3D();
            EngineRenderManager2D = new WiiRenderManager2D();
            EngineRenderManager3D->SetOverlayRenderManager2D(EngineRenderManager2D);
            EngineInputManager = new WiiInputManager();
            EnginePlatformInfo = new PlatformInfo("wii", "wii-headless");

            initializationStage = "AddPrimaryWindow";
            SetBootPhase(WiiBootPhase::CoreInitialization, GXColor { 0x00, 0x00, 0xFF, 0xFF });
            EngineRenderManager3D->AddWindow(0, RenderMode->fbWidth, RenderMode->efbHeight);

            initializationStage = "InitializeCore";
            EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputManager, EnginePlatformInfo, options);
            SYS_Report("[Wii] Engine core initialized.\n");
            EngineInitialized = true;
            PresentedFrameCount = 0;
            VerifiedFrameCount = 0;
            UpdateCompletedSincePresent = false;
            DrawCompletedSincePresent = false;
            SetBootPhase(WiiBootPhase::Running, GXColor { 0x00, 0xFF, 0x00, 0xFF });
            return true;
        }
        catch (const std::exception& exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine core initialization threw std::exception stage=%s message=%s\n", initializationStage, exception.what());
            return false;
        }
        catch (Exception* exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine core initialization threw Exception stage=%s message=%s\n", initializationStage, exception != nullptr ? exception->what() : "<null>");
            delete exception;
            return false;
        }
        catch (...) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine core initialization threw stage=%s.\n", initializationStage);
            return false;
        }
#else
        return true;
#endif
    }
```

- [ ] **Step 2: Replace `UpdateEngineCore` with real generated-core update**

Replace the stub with:

```cpp
    /// Advances one engine frame when the generated core was initialized successfully.
    bool WiiApplication::UpdateEngineCore() {
#if HELENGINE_WII_HAS_GENERATED_CORE
        if (!EngineInitialized || EngineCore == nullptr || EngineRenderManager2D == nullptr) {
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            return false;
        }

        try {
            SetBootPhase(WiiBootPhase::CoreUpdate, GXColor { 0x00, 0xA0, 0x00, 0xFF });
            EngineRenderManager2D->BeginFrame();
            EngineCore->Update();
            EngineRenderManager2D->FlushReleasedTextures();
            if (EngineRenderManager3D != nullptr) {
                EngineRenderManager3D->FlushReleasedAssets();
            }
            UpdateCompletedSincePresent = true;
            return true;
        }
        catch (Exception* exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine update threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            delete exception;
            return false;
        }
        catch (const std::exception& exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine update threw std::exception: %s\n", exception.what());
            return false;
        }
        catch (...) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine update threw.\n");
            return false;
        }
#else
        return true;
#endif
    }
```

- [ ] **Step 3: Replace `DrawEngineCore` with real generated-core draw**

Replace the stub with:

```cpp
    /// Draws one engine frame when the generated core was initialized successfully.
    bool WiiApplication::DrawEngineCore() {
#if HELENGINE_WII_HAS_GENERATED_CORE
        if (!EngineInitialized || EngineCore == nullptr || EngineRenderManager3D == nullptr || EngineRenderManager2D == nullptr) {
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            return false;
        }

        try {
            SetBootPhase(WiiBootPhase::CoreDraw, GXColor { 0x00, 0x60, 0x00, 0xFF });
            EngineCore->Draw();
            DrawCompletedSincePresent = true;
            VerifiedFrameCount++;
            return true;
        }
        catch (Exception* exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine draw threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            delete exception;
            return false;
        }
        catch (const std::exception& exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine draw threw std::exception: %s\n", exception.what());
            return false;
        }
        catch (...) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine draw threw.\n");
            return false;
        }
#else
        return true;
#endif
    }
```

- [ ] **Step 4: Replace `ResolvePresentedClearColor` with generated-frame diagnostics**

Replace the method with:

```cpp
    /// Resolves the currently visible diagnostic color for the next presented frame.
    GXColor WiiApplication::ResolvePresentedClearColor() {
#if HELENGINE_WII_HAS_GENERATED_CORE
        if (EngineInitialized) {
            if (UpdateCompletedSincePresent && DrawCompletedSincePresent) {
                UpdateCompletedSincePresent = false;
                DrawCompletedSincePresent = false;
                return GXColor { 0x00, 0x80, 0x80, 0xFF };
            }

            if (UpdateCompletedSincePresent) {
                UpdateCompletedSincePresent = false;
                return GXColor { 0xC0, 0xC0, 0x00, 0xFF };
            }

            if (DrawCompletedSincePresent) {
                DrawCompletedSincePresent = false;
                return GXColor { 0x00, 0x60, 0xA0, 0xFF };
            }
        }
#endif

        return ClearColor;
    }
```

- [ ] **Step 5: Generate Wii-targeted core output**

Run:

```bash
rtk dotnet run --project C:\dev\helworks\csharpcodegen\codegen\codegen.csproj -- --cpp --project C:\dev\helworks\helengine\engine\helengine.core\helengine.core.csproj --output C:\dev\helworks\helengine-wii\tmp\generated-core-wii --platform wii --compiler gcc --endianness big --preset wii-core-boot
```

Expected: output includes `tmp\generated-core-wii\helcpp_config.hpp` and either `helengine_core_amalgamated.cpp` or `helengine_core_unity.cpp`. The config contains `#define HE_CPP_PLATFORM_WII 1`.

- [ ] **Step 6: Build the Wii player with generated core enabled**

Run:

```bash
rtk docker run --rm -v /mnt/c/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii helengine-wii make clean all
```

Expected: `build/helengine_wii.dol` is emitted with `HELENGINE_WII_HAS_GENERATED_CORE=1`.

- [ ] **Step 7: Build a deterministic verification DOL**

Run:

```bash
rtk docker run --rm -v /mnt/c/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii -e HELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT=3 helengine-wii make clean all
```

Expected: `build/helengine_wii.dol` is emitted. In Dolphin, this DOL should return after three generated update/draw frames instead of looping forever.

- [ ] **Step 8: Commit generated-core host wiring**

Run:

```bash
rtk git add Makefile src/platform/wii/WiiApplication.cpp
rtk git commit -m "Wire Wii generated-core boot loop"
```

Expected: commit succeeds with only the Wii Makefile and application implementation.

### Task 7: Document And Verify The Generated-Core Flow

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Add the generated-core build section**

Append this section to `README.md`:

````md
## Generated Core Build

Generate Wii-targeted core output:

```bash
rtk dotnet run --project C:\dev\helworks\csharpcodegen\codegen\codegen.csproj -- --cpp --project C:\dev\helworks\helengine\engine\helengine.core\helengine.core.csproj --output C:\dev\helworks\helengine-wii\tmp\generated-core-wii --platform wii --compiler gcc --endianness big --preset wii-core-boot
```

Build the Wii player with generated core enabled:

```bash
rtk docker run --rm -v /mnt/c/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii helengine-wii make clean all
```

The build emits `build/helengine_wii.dol`.

## Generated Core Verification

For a deterministic emulator probe, build with a frame limit:

```bash
rtk docker run --rm -v /mnt/c/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii -e HELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT=3 helengine-wii make clean all
```

Load `build/helengine_wii.dol` in Dolphin.

Expected result:

- `Core::Initialize(...)` completes
- `Update()` and `Draw()` both run for at least three frames
- the visible frame reaches the generated-frame diagnostic teal color before the verification build exits
````

- [ ] **Step 2: Run codegen verification**

Run:

```bash
rtk dotnet test C:\dev\helworks\csharpcodegen\cs2.cpp.tests\cs2.cpp.tests.csproj --filter "FullyQualifiedName~WiiCoreBoot|FullyQualifiedName~Write_WithWiiProfile|FullyQualifiedName~GameCubeCoreBoot|FullyQualifiedName~Write_WithGameCubeProfile" --no-restore
```

Expected: all targeted codegen tests pass.

- [ ] **Step 3: Run Wii no-generated-core build verification**

Run:

```bash
rtk docker run --rm -v /mnt/c/dev/helworks/helengine-wii:/workspace -w /workspace helengine-wii make clean all
```

Expected: `build/helengine_wii.dol` is emitted and the Makefile does not require `HELENGINE_CORE_CPP_ROOT`.

- [ ] **Step 4: Run Wii generated-core build verification**

Run:

```bash
rtk dotnet run --project C:\dev\helworks\csharpcodegen\codegen\codegen.csproj -- --cpp --project C:\dev\helworks\helengine\engine\helengine.core\helengine.core.csproj --output C:\dev\helworks\helengine-wii\tmp\generated-core-wii --platform wii --compiler gcc --endianness big --preset wii-core-boot
rtk docker run --rm -v /mnt/c/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii helengine-wii make clean all
```

Expected: generated output is refreshed and `build/helengine_wii.dol` is emitted with generated core enabled.

- [ ] **Step 5: Run manual Dolphin verification**

Run:

```text
Open C:\dev\helworks\helengine-wii\build\helengine_wii.dol in Dolphin.
```

Expected: the runtime no longer stays on the initial pink bootstrap color. The visible diagnostic color reaches teal after update/draw frames, and the emulator does not reset or crash.

- [ ] **Step 6: Commit documentation**

Run:

```bash
rtk git add README.md
rtk git commit -m "Document Wii generated-core boot flow"
```

Expected: commit succeeds with only `README.md`.

## Self-Review

- Spec coverage check:
  - Use GC as reference rather than submodule: covered by architecture and Wii-owned file names.
  - Generated-core boot first: covered by Tasks 1 through 7.
  - No builder/packaging/full renderer scope creep: explicitly excluded in the architecture and bridge implementations.
  - Codegen prerequisite: covered by Tasks 1 and 2.
  - Wii native build consumption: covered by Task 3.
  - Runtime initialize/update/draw loop: covered by Tasks 4 through 6.
  - Documentation and verification: covered by Task 7.
- Placeholder scan:
  - No placeholder markers or copy-forward shortcuts remain.
- Type consistency:
  - Platform ids are consistently `wii` and `wii-headless`.
  - Generated config macro is consistently `HE_CPP_PLATFORM_WII`.
  - Host macro is consistently `HELENGINE_WII_HAS_GENERATED_CORE`.
  - Batch verification macro is consistently `HELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT`.
