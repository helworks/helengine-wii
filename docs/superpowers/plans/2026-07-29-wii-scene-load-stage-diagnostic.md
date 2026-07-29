# Wii Scene-Load Stage Diagnostic Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Wii hardware diagnostic WBFS that replaces ambiguous orange `C002` results with exact scene-manager, scene-materialization, component, registration, event, and cleanup codes.

**Architecture:** `WiiApplication` reads the already-generated `Core.LastSceneTransitionStage` only after a caught draw exception. The current marker prefix selects either `SceneManager` or `RuntimeSceneLoadService`, preventing stale trace state from winning; documented application methods translate exact stages and startup component identifiers into `WiiFailureCode` values, while `WiiFailureScreen` owns the distinct presentation palette.

**Tech Stack:** C#/.NET 9, xUnit source-contract tests, C++20, generated Helengine C++, devkitPPC/libogc, Docker Wii toolchain, Wiimms ISO Tools, Dolphin.

---

## File Structure

- Modify `builder.tests/WiiRuntimeSourceTests.cs`: add focused source-contract tests for enum values, prefix-selected stage mapping, component-specific mapping, precedence, and exact colors.
- Modify `src/platform/wii/WiiFailureCode.hpp`: define documented `C110`-`C145` scene-load diagnostic values.
- Modify `src/platform/wii/WiiApplication.hpp`: declare documented scene-manager and runtime scene-load refinement methods.
- Modify `src/platform/wii/WiiApplication.cpp`: preserve native precedence, select trace ownership from the current core marker, and translate exact trace values without changing scene behavior.
- Modify `src/platform/wii/WiiFailureScreen.cpp`: assign each new code its approved distinct background color.
- Create build outputs under `C:/dev/helprojs/demodisc/output/wii-scene-load-diagnostic-20260729`: fresh ISO, matched WBFS, and native DOL.

### Task 1: Map scene-manager and owned-asset boundaries

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Modify: `src/platform/wii/WiiFailureCode.hpp`
- Modify: `src/platform/wii/WiiApplication.hpp`
- Modify: `src/platform/wii/WiiApplication.cpp`

- [ ] **Step 1: Write the failing scene-manager mapping test**

Add this documented test to `WiiRuntimeSourceTests`:

```csharp
/// <summary>
/// Ensures caught scene-manager failures retain the exact loading, registration, event, or cleanup boundary.
/// </summary>
[Fact]
public void DrawFailureDiagnostic_MapsSceneManagerTraceStages() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string failureCodeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiFailureCode.hpp"));
    string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.hpp"));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));

    Dictionary<string, string> expectedCodes = new Dictionary<string, string> {
        ["SceneOperationCommit"] = "0xC110U",
        ["ScenePathResolution"] = "0xC111U",
        ["SceneRecordLookup"] = "0xC112U",
        ["SceneContentLoad"] = "0xC113U",
        ["SceneMaterializationCall"] = "0xC114U",
        ["SceneMaterializationReturn"] = "0xC115U",
        ["SceneRecordTrack"] = "0xC116U",
        ["SceneRecordListInsertion"] = "0xC117U",
        ["SceneRecordDictionaryInsertion"] = "0xC118U",
        ["SceneLoadedEvent"] = "0xC119U",
        ["SceneLoadedEventReturned"] = "0xC11AU",
        ["SceneLoadedEventArgsRelease"] = "0xC11BU",
        ["SceneLoadedEventArgsReleased"] = "0xC11CU",
        ["SceneLoadCleanup"] = "0xC11DU",
        ["OwnedTextureRegistration"] = "0xC140U",
        ["OwnedFontRegistration"] = "0xC141U",
        ["OwnedAudioRegistration"] = "0xC142U",
        ["OwnedModelRegistration"] = "0xC143U",
        ["OwnedMaterialRegistration"] = "0xC144U",
        ["OwnedAssetRegistrationCompleted"] = "0xC145U"
    };
    foreach (KeyValuePair<string, string> expectedCode in expectedCodes) {
        Assert.Contains($"{expectedCode.Key} = {expectedCode.Value}", failureCodeSource, StringComparison.Ordinal);
    }

    Assert.Contains("bool RefineSceneManagerFailureCheckpoint(const std::string& sceneManagerStage);", applicationHeaderSource, StringComparison.Ordinal);
    Assert.Contains("sceneTransitionStage.starts_with(\"SceneManager:\")", applicationSource, StringComparison.Ordinal);
    Assert.Contains("get_LastTraceStage()", applicationSource, StringComparison.Ordinal);

    string[] expectedStages = {
        "CommitPendingOperationsAtFrameBoundaryOperation",
        "LoadSceneImmediateBeforeResolveSceneContentPath",
        "LoadSceneImmediateBeforeLoadedSceneRecordLookup",
        "LoadSceneImmediateBeforeContentLoad",
        "LoadSceneImmediateBeforeSceneLoadServiceLoad",
        "LoadSceneImmediateAfterSceneLoadServiceLoad",
        "LoadSceneImmediateBeforeLoadedSceneRecordTrack",
        "LoadSceneImmediateAfterLoadedSceneRecordListAdd",
        "LoadSceneImmediateAfterLoadedSceneRecordDictionaryAdd",
        "LoadSceneImmediateBeforeSceneLoadedEvent",
        "LoadSceneImmediateAfterSceneLoadedEvent",
        "LoadSceneImmediateBeforeRegisterOwnedTextures",
        "LoadSceneImmediateBeforeRegisterOwnedFonts",
        "LoadSceneImmediateBeforeRegisterOwnedAudio",
        "LoadSceneImmediateBeforeRegisterOwnedModels",
        "LoadSceneImmediateBeforeRegisterOwnedMaterials",
        "LoadSceneImmediateAfterRegisterOwnedAssets"
    };
    foreach (string expectedStage in expectedStages) {
        Assert.Contains($"sceneManagerStage == \"{expectedStage}\"", applicationSource, StringComparison.Ordinal);
    }

    Assert.Contains("sceneTransitionStage == \"AfterSceneLoadedEventDispatch\"", applicationSource, StringComparison.Ordinal);
    Assert.Contains("sceneTransitionStage == \"BeforeSceneLoadedEventArgsRelease\"", applicationSource, StringComparison.Ordinal);
    Assert.Contains("sceneTransitionStage == \"AfterSceneLoadedEventArgsRelease\"", applicationSource, StringComparison.Ordinal);
    Assert.Contains("sceneTransitionStage == \"AfterTransitionSceneAssetRelease\"", applicationSource, StringComparison.Ordinal);
    Assert.Contains("sceneTransitionStage == \"AfterSceneTransitionCommit\"", applicationSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the focused test and verify the expected failure**

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter FullyQualifiedName~DrawFailureDiagnostic_MapsSceneManagerTraceStages --verbosity minimal
```

Expected: FAIL because `C110`-`C11D`, `C140`-`C145`, and the detailed refinement method are absent.

- [ ] **Step 3: Add the documented scene-manager enum values**

Append these members before the enum's closing brace in `WiiFailureCode.hpp`; add the shown substantive comments rather than modifying generated files:

```cpp
/// A pending scene operation was being dispatched at the generated frame boundary.
SceneOperationCommit = 0xC110U,
/// The immediate loader was resolving the packaged scene content path.
ScenePathResolution = 0xC111U,
/// The scene manager was finding or preparing the loaded-scene record.
SceneRecordLookup = 0xC112U,
/// The content manager was loading the packaged scene asset.
SceneContentLoad = 0xC113U,
/// The scene manager was entering runtime scene materialization.
SceneMaterializationCall = 0xC114U,
/// Runtime scene materialization returned and owned-asset registration was about to begin.
SceneMaterializationReturn = 0xC115U,
/// The scene manager was about to track the new loaded-scene record.
SceneRecordTrack = 0xC116U,
/// Loaded-scene list insertion returned and dictionary insertion was active.
SceneRecordListInsertion = 0xC117U,
/// Loaded-scene dictionary insertion returned and event dispatch was next.
SceneRecordDictionaryInsertion = 0xC118U,
/// The scene-loaded event was being dispatched.
SceneLoadedEvent = 0xC119U,
/// Scene-loaded event dispatch returned successfully.
SceneLoadedEventReturned = 0xC11AU,
/// Scene-loaded event arguments were being released.
SceneLoadedEventArgsRelease = 0xC11BU,
/// Scene-loaded event-argument release returned successfully.
SceneLoadedEventArgsReleased = 0xC11CU,
/// Immediate scene-load or transition cleanup was active.
SceneLoadCleanup = 0xC11DU,
/// Texture ownership registration was active.
OwnedTextureRegistration = 0xC140U,
/// Font ownership registration was active.
OwnedFontRegistration = 0xC141U,
/// Audio ownership registration was active.
OwnedAudioRegistration = 0xC142U,
/// Model ownership registration was active.
OwnedModelRegistration = 0xC143U,
/// Material ownership registration was active.
OwnedMaterialRegistration = 0xC144U,
/// Owned-asset registration returned successfully.
OwnedAssetRegistrationCompleted = 0xC145U
```

- [ ] **Step 4: Implement exact scene-manager refinement**

Include `<string>` in `WiiApplication.hpp` and declare:

```cpp
/// Maps one generated scene-manager trace stage to a persistent Wii failure code.
bool RefineSceneManagerFailureCheckpoint(const std::string& sceneManagerStage);
```

Implement the method with one formatted `if / else if` chain. Each recognized branch sets exactly one checkpoint and the method returns `true`; an unknown stage returns `false` without replacing `C002`:

```cpp
bool WiiApplication::RefineSceneManagerFailureCheckpoint(const std::string& sceneManagerStage) {
    if (sceneManagerStage == "CommitPendingOperationsAtFrameBoundaryBegin"
        || sceneManagerStage == "CommitPendingOperationsAtFrameBoundaryOperation"
        || sceneManagerStage == "CommitPendingOperationsAtFrameBoundaryEnd") {
        SetFailureCheckpoint(WiiFailureCode::SceneOperationCommit);
    } else if (sceneManagerStage == "LoadSceneImmediateBegin"
        || sceneManagerStage == "LoadSceneImmediateBeforeResolveSceneContentPath") {
        SetFailureCheckpoint(WiiFailureCode::ScenePathResolution);
    } else if (sceneManagerStage == "LoadSceneImmediateAfterResolveSceneContentPath"
        || sceneManagerStage == "LoadSceneImmediateBeforeLoadedSceneRecordLookup"
        || sceneManagerStage == "LoadSceneImmediateAfterLoadedSceneRecordLookup"
        || sceneManagerStage == "LoadSceneImmediateDisposeUntrackedRoots"
        || sceneManagerStage == "LoadSceneImmediateUnloadSingleModeScenes"
        || sceneManagerStage == "LoadSceneImmediateFlushReleasedTextures"
        || sceneManagerStage == "LoadSceneImmediateResetPhysicsTiming") {
        SetFailureCheckpoint(WiiFailureCode::SceneRecordLookup);
    } else if (sceneManagerStage == "LoadSceneImmediateBeforeContentLoad") {
        SetFailureCheckpoint(WiiFailureCode::SceneContentLoad);
    } else if (sceneManagerStage == "LoadSceneImmediateBeforeSceneLoadServiceLoad") {
        SetFailureCheckpoint(WiiFailureCode::SceneMaterializationCall);
    } else if (sceneManagerStage == "LoadSceneImmediateAfterSceneLoadServiceLoad") {
        SetFailureCheckpoint(WiiFailureCode::SceneMaterializationReturn);
    } else if (sceneManagerStage == "LoadSceneImmediateBeforeLoadedSceneRecordTrack") {
        SetFailureCheckpoint(WiiFailureCode::SceneRecordTrack);
    } else if (sceneManagerStage == "LoadSceneImmediateAfterLoadedSceneRecordListAdd") {
        SetFailureCheckpoint(WiiFailureCode::SceneRecordListInsertion);
    } else if (sceneManagerStage == "LoadSceneImmediateAfterLoadedSceneRecordDictionaryAdd") {
        SetFailureCheckpoint(WiiFailureCode::SceneRecordDictionaryInsertion);
    } else if (sceneManagerStage == "LoadSceneImmediateBeforeSceneLoadedEvent") {
        SetFailureCheckpoint(WiiFailureCode::SceneLoadedEvent);
    } else if (sceneManagerStage == "LoadSceneImmediateAfterSceneLoadedEvent"
        || sceneManagerStage == "LoadSceneImmediateEnd") {
        SetFailureCheckpoint(WiiFailureCode::SceneLoadCleanup);
    } else if (sceneManagerStage == "LoadSceneImmediateBeforeRegisterOwnedTextures") {
        SetFailureCheckpoint(WiiFailureCode::OwnedTextureRegistration);
    } else if (sceneManagerStage == "LoadSceneImmediateBeforeRegisterOwnedFonts") {
        SetFailureCheckpoint(WiiFailureCode::OwnedFontRegistration);
    } else if (sceneManagerStage == "LoadSceneImmediateBeforeRegisterOwnedAudio") {
        SetFailureCheckpoint(WiiFailureCode::OwnedAudioRegistration);
    } else if (sceneManagerStage == "LoadSceneImmediateBeforeRegisterOwnedModels") {
        SetFailureCheckpoint(WiiFailureCode::OwnedModelRegistration);
    } else if (sceneManagerStage == "LoadSceneImmediateBeforeRegisterOwnedMaterials") {
        SetFailureCheckpoint(WiiFailureCode::OwnedMaterialRegistration);
    } else if (sceneManagerStage == "LoadSceneImmediateAfterRegisterOwnedAssets") {
        SetFailureCheckpoint(WiiFailureCode::OwnedAssetRegistrationCompleted);
    } else {
        return false;
    }
    return true;
}
```

In `RefineDrawFailureCheckpoint()`, keep the existing native `WiiDrawStage` switch first. Then map direct event markers to `C11A`-`C11D`. Only call the method when `sceneTransitionStage.starts_with("SceneManager:")`, `EngineCore->get_SceneManager()` is non-null, and its `LastTraceStage` is available. Return when the method reports a match. Keep existing outer generated-stage mappings after this block.

- [ ] **Step 5: Run the focused mapping tests**

Run the Step 2 command and:

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~DrawFailureDiagnostic_MapsCaughtFailuresToPersistentVisibleCodes|FullyQualifiedName~DrawFailureDiagnostic_MapsRemainingGeneratedDrawStages" --verbosity minimal
```

Expected: all selected tests pass, and native renderer mapping still appears before generated stage refinement.

- [ ] **Step 6: Commit the scene-manager mapping**

```powershell
git add -- builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiFailureCode.hpp src/platform/wii/WiiApplication.hpp src/platform/wii/WiiApplication.cpp
git commit -m "Map Wii scene manager failure stages"
```

### Task 2: Map runtime materialization and startup components

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Modify: `src/platform/wii/WiiFailureCode.hpp`
- Modify: `src/platform/wii/WiiApplication.hpp`
- Modify: `src/platform/wii/WiiApplication.cpp`

- [ ] **Step 1: Write the failing runtime scene-load test**

Add `DrawFailureDiagnostic_MapsRuntimeSceneLoadTraceStages` with documented XML comments. Begin the method with these exact source reads, then assert the enum snippets below:

```csharp
string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
string failureCodeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiFailureCode.hpp"));
string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.hpp"));
string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));
```

```csharp
Dictionary<string, string> expectedCodes = new Dictionary<string, string> {
    ["SceneMaterializationBegin"] = "0xC120U",
    ["SceneEntityConstruction"] = "0xC121U",
    ["SceneComponentDeserialization"] = "0xC122U",
    ["SceneChildEntity"] = "0xC123U",
    ["SceneEntityCompletion"] = "0xC124U",
    ["SceneMaterializationCompleted"] = "0xC125U",
    ["CameraComponentDeserialization"] = "0xC130U",
    ["RoundedRectComponentDeserialization"] = "0xC131U",
    ["ViewportComponentDeserialization"] = "0xC132U",
    ["ReferenceCanvasFitComponentDeserialization"] = "0xC133U",
    ["SplashComponentDeserialization"] = "0xC134U",
    ["SpriteComponentDeserialization"] = "0xC135U"
};
foreach (KeyValuePair<string, string> expectedCode in expectedCodes) {
    Assert.Contains($"{expectedCode.Key} = {expectedCode.Value}", failureCodeSource, StringComparison.Ordinal);
}

Assert.Contains("bool RefineSceneLoadFailureCheckpoint(const std::string& sceneLoadStage, const std::string& componentTypeId);", applicationHeaderSource, StringComparison.Ordinal);
Assert.Contains("WiiFailureCode ResolveSceneComponentFailureCode(const std::string& componentTypeId) const;", applicationHeaderSource, StringComparison.Ordinal);
Assert.Contains("sceneTransitionStage.starts_with(\"SceneLoad:\")", applicationSource, StringComparison.Ordinal);
Assert.Contains("get_LastTraceComponentTypeId()", applicationSource, StringComparison.Ordinal);

string[] expectedStages = { "LoadBegin", "BeforeRootEntityLoad", "LoadEntityBegin", "BeforeComponentLoad", "LoadComponentBegin", "BeforeChildEntityLoad", "LoadEntityEnd", "LoadEnd" };
foreach (string expectedStage in expectedStages) {
    Assert.Contains($"sceneLoadStage == \"{expectedStage}\"", applicationSource, StringComparison.Ordinal);
}

string[] expectedComponentTypes = {
    "helengine.CameraComponent",
    "helengine.RoundedRectComponent",
    "helengine.ViewportComponent",
    "helengine.ReferenceCanvasFitComponent",
    "city.menu.HelenOfCodeSplashComponent, gameplay",
    "helengine.SpriteComponent"
};
foreach (string expectedComponentType in expectedComponentTypes) {
    Assert.Contains($"componentTypeId == \"{expectedComponentType}\"", applicationSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the focused test and verify the expected failure**

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter FullyQualifiedName~DrawFailureDiagnostic_MapsRuntimeSceneLoadTraceStages --verbosity minimal
```

Expected: FAIL because `C120`-`C135` and both runtime scene-load methods are absent.

- [ ] **Step 3: Add documented materialization and component enum values**

Add `SceneMaterializationBegin = 0xC120U`, `SceneEntityConstruction = 0xC121U`, `SceneComponentDeserialization = 0xC122U`, `SceneChildEntity = 0xC123U`, `SceneEntityCompletion = 0xC124U`, `SceneMaterializationCompleted = 0xC125U`, `CameraComponentDeserialization = 0xC130U`, `RoundedRectComponentDeserialization = 0xC131U`, `ViewportComponentDeserialization = 0xC132U`, `ReferenceCanvasFitComponentDeserialization = 0xC133U`, `SplashComponentDeserialization = 0xC134U`, and `SpriteComponentDeserialization = 0xC135U`. Give every member a substantive comment naming the operation or exact component.

- [ ] **Step 4: Implement runtime scene-load refinement**

Declare the two documented methods asserted in Step 1. Implement component resolution as one `if / else if` chain returning the six exact component codes and generic `SceneComponentDeserialization` for every other valid component identifier.

```cpp
WiiFailureCode WiiApplication::ResolveSceneComponentFailureCode(const std::string& componentTypeId) const {
    if (componentTypeId == "helengine.CameraComponent") {
        return WiiFailureCode::CameraComponentDeserialization;
    } else if (componentTypeId == "helengine.RoundedRectComponent") {
        return WiiFailureCode::RoundedRectComponentDeserialization;
    } else if (componentTypeId == "helengine.ViewportComponent") {
        return WiiFailureCode::ViewportComponentDeserialization;
    } else if (componentTypeId == "helengine.ReferenceCanvasFitComponent") {
        return WiiFailureCode::ReferenceCanvasFitComponentDeserialization;
    } else if (componentTypeId == "city.menu.HelenOfCodeSplashComponent, gameplay") {
        return WiiFailureCode::SplashComponentDeserialization;
    } else if (componentTypeId == "helengine.SpriteComponent") {
        return WiiFailureCode::SpriteComponentDeserialization;
    }
    return WiiFailureCode::SceneComponentDeserialization;
}
```

Implement stage refinement as:

```cpp
bool WiiApplication::RefineSceneLoadFailureCheckpoint(const std::string& sceneLoadStage, const std::string& componentTypeId) {
    if (sceneLoadStage == "LoadBegin" || sceneLoadStage == "BeforeRootEntityLoad") {
        SetFailureCheckpoint(WiiFailureCode::SceneMaterializationBegin);
    } else if (sceneLoadStage == "LoadEntityBegin") {
        SetFailureCheckpoint(WiiFailureCode::SceneEntityConstruction);
    } else if (sceneLoadStage == "BeforeComponentLoad" || sceneLoadStage == "LoadComponentBegin") {
        SetFailureCheckpoint(ResolveSceneComponentFailureCode(componentTypeId));
    } else if (sceneLoadStage == "BeforeChildEntityLoad") {
        SetFailureCheckpoint(WiiFailureCode::SceneChildEntity);
    } else if (sceneLoadStage == "LoadEntityEnd") {
        SetFailureCheckpoint(WiiFailureCode::SceneEntityCompletion);
    } else if (sceneLoadStage == "LoadEnd") {
        SetFailureCheckpoint(WiiFailureCode::SceneMaterializationCompleted);
    } else {
        return false;
    }
    return true;
}
```

Call it from `RefineDrawFailureCheckpoint()` only when the current core marker starts with `SceneLoad:` and the current `RuntimeSceneLoadService` is non-null. Pass both `get_LastTraceStage()` and `get_LastTraceComponentTypeId()`, return on a recognized stage, and keep the `SceneLoad:` block before the `SceneManager:` block.

- [ ] **Step 5: Run all refinement tests**

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~DrawFailureDiagnostic_Maps" --verbosity minimal
```

Expected: every selected mapping test passes.

- [ ] **Step 6: Commit the runtime materialization mapping**

```powershell
git add -- builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiFailureCode.hpp src/platform/wii/WiiApplication.hpp src/platform/wii/WiiApplication.cpp
git commit -m "Map Wii scene materialization failures"
```

### Task 3: Add distinct scene-load diagnostic colors

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Modify: `src/platform/wii/WiiFailureScreen.cpp`

- [ ] **Step 1: Write the failing scene-load palette test**

Add `DrawFailureDiagnostic_UsesDistinctSceneLoadBackgrounds` with XML comments. Read `WiiFailureScreen.cpp`, normalize line endings, and use a dictionary whose keys are all enum names from Tasks 1 and 2 and whose values are the exact color initializers from the approved design. For each entry assert this exact switch fragment:

```csharp
Dictionary<string, string> expectedColors = new Dictionary<string, string> {
    ["SceneOperationCommit"] = "{ 0x70, 0x10, 0x10, 0xFF }",
    ["ScenePathResolution"] = "{ 0x80, 0x28, 0x08, 0xFF }",
    ["SceneRecordLookup"] = "{ 0x78, 0x48, 0x00, 0xFF }",
    ["SceneContentLoad"] = "{ 0x68, 0x60, 0x00, 0xFF }",
    ["SceneMaterializationCall"] = "{ 0x38, 0x68, 0x00, 0xFF }",
    ["SceneMaterializationReturn"] = "{ 0x08, 0x68, 0x18, 0xFF }",
    ["SceneRecordTrack"] = "{ 0x00, 0x68, 0x48, 0xFF }",
    ["SceneRecordListInsertion"] = "{ 0x00, 0x58, 0x70, 0xFF }",
    ["SceneRecordDictionaryInsertion"] = "{ 0x00, 0x38, 0x80, 0xFF }",
    ["SceneLoadedEvent"] = "{ 0x28, 0x28, 0x80, 0xFF }",
    ["SceneLoadedEventReturned"] = "{ 0x50, 0x20, 0x80, 0xFF }",
    ["SceneLoadedEventArgsRelease"] = "{ 0x78, 0x10, 0x70, 0xFF }",
    ["SceneLoadedEventArgsReleased"] = "{ 0x80, 0x18, 0x48, 0xFF }",
    ["SceneLoadCleanup"] = "{ 0x58, 0x38, 0x48, 0xFF }",
    ["SceneMaterializationBegin"] = "{ 0x50, 0x18, 0x08, 0xFF }",
    ["SceneEntityConstruction"] = "{ 0x60, 0x38, 0x08, 0xFF }",
    ["SceneComponentDeserialization"] = "{ 0x58, 0x58, 0x10, 0xFF }",
    ["SceneChildEntity"] = "{ 0x20, 0x58, 0x20, 0xFF }",
    ["SceneEntityCompletion"] = "{ 0x10, 0x50, 0x58, 0xFF }",
    ["SceneMaterializationCompleted"] = "{ 0x20, 0x30, 0x68, 0xFF }",
    ["CameraComponentDeserialization"] = "{ 0x68, 0x18, 0x30, 0xFF }",
    ["RoundedRectComponentDeserialization"] = "{ 0x68, 0x30, 0x18, 0xFF }",
    ["ViewportComponentDeserialization"] = "{ 0x48, 0x58, 0x08, 0xFF }",
    ["ReferenceCanvasFitComponentDeserialization"] = "{ 0x18, 0x58, 0x38, 0xFF }",
    ["SplashComponentDeserialization"] = "{ 0x18, 0x40, 0x68, 0xFF }",
    ["SpriteComponentDeserialization"] = "{ 0x48, 0x18, 0x68, 0xFF }",
    ["OwnedTextureRegistration"] = "{ 0x48, 0x20, 0x10, 0xFF }",
    ["OwnedFontRegistration"] = "{ 0x58, 0x40, 0x10, 0xFF }",
    ["OwnedAudioRegistration"] = "{ 0x38, 0x58, 0x18, 0xFF }",
    ["OwnedModelRegistration"] = "{ 0x10, 0x58, 0x50, 0xFF }",
    ["OwnedMaterialRegistration"] = "{ 0x18, 0x38, 0x70, 0xFF }",
    ["OwnedAssetRegistrationCompleted"] = "{ 0x50, 0x18, 0x58, 0xFF }"
};
foreach (KeyValuePair<string, string> expectedColor in expectedColors) {
    Assert.Contains($"case WiiFailureCode::{expectedColor.Key}:\n                return GXColor {expectedColor.Value};", normalizedFailureScreenSource, StringComparison.Ordinal);
}
Assert.Equal(expectedColors.Count, expectedColors.Values.Distinct(StringComparer.Ordinal).Count());
```

- [ ] **Step 2: Run the palette test and verify the expected failure**

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter FullyQualifiedName~DrawFailureDiagnostic_UsesDistinctSceneLoadBackgrounds --verbosity minimal
```

Expected: FAIL because no new switch cases exist.

- [ ] **Step 3: Add all approved switch cases**

In `WiiFailureScreen::ResolveBackgroundColor`, add one `case` and `return GXColor` pair for every dictionary entry from Step 1. The production switch entries are generated mechanically from the asserted enum name and exact initializer, in the same order as that dictionary. Keep existing `C002` orange, existing `C101`-`C10A` colors, and the opaque red default unchanged; do not change the dictionary while making production code pass.

- [ ] **Step 4: Run focused and complete Wii tests**

```powershell
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --filter "FullyQualifiedName~DrawFailureDiagnostic_" --verbosity minimal
dotnet test .\builder.tests\helengine.wii.builder.tests.csproj --verbosity minimal
```

Expected: all focused tests and the complete Wii builder test project pass.

- [ ] **Step 5: Commit the palette**

```powershell
git add -- builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiFailureScreen.cpp
git commit -m "Color Wii scene load diagnostics"
```

### Task 4: Build and verify the hardware diagnostic

**Files:**
- Build output: `C:/dev/helprojs/demodisc/output/wii-scene-load-diagnostic-20260729/game.iso`
- Build output: `C:/dev/helprojs/demodisc/output/wii-scene-load-diagnostic-20260729/RCIE01.wbfs`
- Build output: `C:/dev/helprojs/demodisc/output/wii-scene-load-diagnostic-20260729/native/helengine_wii.dol`

- [ ] **Step 1: Confirm repository scope before building**

Run `git status --short`. Expected: only the user's pre-existing `tmp_wii_audio_contract_test.txt` and `tmp_wii_audio_test.txt` remain untracked after the implementation commits.

- [ ] **Step 2: Build through the normal packaged Wii pipeline**

```powershell
dotnet run --project C:\dev\helworks\helengine\tools\build-waiter\helengine.buildwaiter.csproj -- --output C:\dev\helprojs\demodisc\output\wii-scene-load-diagnostic-20260729 --require game.iso -- powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\scripts\build-platform.ps1 -Project C:\dev\helprojs\demodisc\project.heproj -Platform wii -Output C:\dev\helprojs\demodisc\output\wii-scene-load-diagnostic-20260729
```

Expected: exit code 0 and `wii-build-phase.txt` ends with `packaged outputs verified`.

- [ ] **Step 3: Confirm generated trace markers without editing generated output**

Use the demodisc project's stable isolated generated-core directory and run:

```powershell
rg -n "SceneManager:|SceneLoad:|AfterSceneLoadedEventDispatch|BeforeSceneLoadedEventArgsRelease|AfterSceneLoadedEventArgsRelease" C:\dev\helworks\b\wii\ae2accd840ab4b78928fa256ece2268c\generated-core
```

Expected: generated `SceneManager.cpp` and `RuntimeSceneLoadService.cpp` contain the existing markers consumed by the Wii mapper.

- [ ] **Step 4: Derive and verify the matched WBFS**

Run:

```powershell
$witPath = 'C:\dev\helworks\helengine-wii\tmp\tools\wit-v3.05a-r8638-cygwin64\bin\wit.exe'
$outputPath = 'C:\dev\helprojs\demodisc\output\wii-scene-load-diagnostic-20260729'
& $witPath verify --verbose "$outputPath\game.iso"
& $witPath copy "$outputPath\game.iso" "$outputPath\RCIE01.wbfs" --wbfs --overwrite
& $witPath verify --verbose "$outputPath\RCIE01.wbfs"
& $witPath dump --long "$outputPath\game.iso" | Select-Object -First 180
```

Expected for both images: `+OK`, disc ID `RCIE01`, IOS56, and one DATA partition.

- [ ] **Step 5: Launch the ISO in Dolphin without screenshots**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\demodisc\output\wii-scene-load-diagnostic-20260729\game.iso
```

Confirm Dolphin remains responsive and the runtime trace shows engine initialization, scene queueing, scene commits, disc reads, and repeated rendered frames. Do not capture screenshots.

- [ ] **Step 6: Record artifact hashes and final repository state**

Run `Get-FileHash -Algorithm SHA256` for the ISO, WBFS, and DOL. Report the three hashes, WIT results, Dolphin process state, output directory, and implementation commits. Confirm no unrelated tracked or untracked files were added or modified.
