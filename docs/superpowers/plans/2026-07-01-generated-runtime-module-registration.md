# Generated Runtime Module Registration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace physics-specific platform bootstrap hooks with a generated-core-owned runtime module registration seam that includes optional modules only when their runtime types are used by the cooked build.

**Architecture:** Add an assembly-level `GeneratedRuntimeModuleManifestAttribute` in `helengine.core`, teach `EditorGeneratedCoreRegenerationService` and `EditorPlatformBuildGraphRunner` to emit `GeneratedRuntimeModuleRegistration.hpp/.cpp` from scene-referenced runtime types, then migrate `helengine.physics3d`, Wii, and GameCube to the generic generated bootstrap call. The generated files always exist as a no-op stub when no optional modules are active, so platforms only depend on one stable generated header.

**Tech Stack:** C#/.NET 9, xUnit, generated C++, Wii/GameCube native hosts, editor build pipeline.

---

### Task 1: Add the shared runtime module manifest contract and discovery tests

**Files:**
- Create: `C:\dev\helworks\helengine\engine\helengine.core\runtime\GeneratedRuntimeModuleManifestAttribute.cs`
- Create: `C:\dev\helworks\helengine\engine\helengine.editor.tests\AssemblyInfo.cs`
- Create: `C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\GeneratedRuntimeModuleRegistrationTestComponent.cs`
- Create: `C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\GeneratedRuntimeModuleRegistrationTestRegistration.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\managers\project\EditorGeneratedCoreRegenerationService.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\EditorGeneratedCoreRegenerationServiceTests.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\EditorGeneratedCoreRegenerationServiceTests.cs`

- [ ] **Step 1: Write the failing discovery tests and test manifest scaffold**

Add the assembly-level test manifest and the failing tests first.

`C:\dev\helworks\helengine\engine\helengine.editor.tests\AssemblyInfo.cs`
```csharp
using helengine;

[assembly: GeneratedRuntimeModuleManifest(
    "editor-tests-runtime-module",
    typeof(helengine.editor.tests.GeneratedRuntimeModuleRegistrationTestRegistration),
    nameof(helengine.editor.tests.GeneratedRuntimeModuleRegistrationTestRegistration.Register),
    typeof(helengine.editor.tests.GeneratedRuntimeModuleRegistrationTestComponent))]
```

`C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\GeneratedRuntimeModuleRegistrationTestComponent.cs`
```csharp
namespace helengine.editor.tests;

/// <summary>
/// Provides one deterministic activation type for generated runtime module discovery tests.
/// </summary>
public sealed class GeneratedRuntimeModuleRegistrationTestComponent : Component {
}
```

`C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\GeneratedRuntimeModuleRegistrationTestRegistration.cs`
```csharp
namespace helengine.editor.tests;

/// <summary>
/// Provides one deterministic registration entrypoint for generated runtime module discovery tests.
/// </summary>
public static class GeneratedRuntimeModuleRegistrationTestRegistration {
    /// <summary>
    /// Records the generated runtime module registration call shape required by the editor tests.
    /// </summary>
    /// <param name="core">Initialized core instance supplied by the generated bootstrap.</param>
    public static void Register(Core core) {
        if (core == null) {
            throw new ArgumentNullException(nameof(core));
        }
    }
}
```

Append these tests to `EditorGeneratedCoreRegenerationServiceTests.cs`:
```csharp
[Fact]
public void DiscoverGeneratedRuntimeModuleManifests_WhenAssemblyDeclaresManifest_ReturnsManifest() {
    IReadOnlyList<GeneratedRuntimeModuleManifestAttribute> manifests =
        EditorGeneratedCoreRegenerationService.DiscoverGeneratedRuntimeModuleManifests([
            typeof(GeneratedRuntimeModuleRegistrationTestComponent).Assembly
        ]);

    GeneratedRuntimeModuleManifestAttribute manifest = Assert.Single(manifests);
    Assert.Equal("editor-tests-runtime-module", manifest.ModuleId);
    Assert.Equal(typeof(GeneratedRuntimeModuleRegistrationTestRegistration), manifest.RegistrationType);
    Assert.Equal(nameof(GeneratedRuntimeModuleRegistrationTestRegistration.Register), manifest.RegistrationMethodName);
    Assert.Contains(typeof(GeneratedRuntimeModuleRegistrationTestComponent), manifest.ActivationTypes);
}

[Fact]
public void ResolveActiveGeneratedRuntimeModuleManifests_WhenActivationTypeIsUsed_ReturnsManifest() {
    IReadOnlyList<GeneratedRuntimeModuleManifestAttribute> manifests =
        EditorGeneratedCoreRegenerationService.DiscoverGeneratedRuntimeModuleManifests([
            typeof(GeneratedRuntimeModuleRegistrationTestComponent).Assembly
        ]);

    IReadOnlyList<GeneratedRuntimeModuleManifestAttribute> activeManifests =
        EditorGeneratedCoreRegenerationService.ResolveActiveGeneratedRuntimeModuleManifests(
            manifests,
            [typeof(GeneratedRuntimeModuleRegistrationTestComponent)]);

    GeneratedRuntimeModuleManifestAttribute manifest = Assert.Single(activeManifests);
    Assert.Equal("editor-tests-runtime-module", manifest.ModuleId);
}
```

- [ ] **Step 2: Run the editor tests to verify they fail**

Run:
```bash
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter "FullyQualifiedName~GeneratedRuntimeModule"
```

Expected: FAIL because `GeneratedRuntimeModuleManifestAttribute`, `DiscoverGeneratedRuntimeModuleManifests`, and `ResolveActiveGeneratedRuntimeModuleManifests` do not exist yet.

- [ ] **Step 3: Implement the shared manifest contract and discovery helpers**

Create `GeneratedRuntimeModuleManifestAttribute.cs`:
```csharp
namespace helengine;

/// <summary>
/// Declares one optional generated runtime module bootstrap contract that can be emitted into native generated core when its activation types are used by cooked scenes.
/// </summary>
[AttributeUsage(AttributeTargets.Assembly, AllowMultiple = true)]
public sealed class GeneratedRuntimeModuleManifestAttribute : Attribute {
    /// <summary>
    /// Initializes one generated runtime module manifest declaration.
    /// </summary>
    /// <param name="moduleId">Stable module id used for deterministic emission order.</param>
    /// <param name="registrationType">Static registration type that exposes the native bootstrap entrypoint.</param>
    /// <param name="registrationMethodName">Static registration method that should be emitted into generated bootstrap code.</param>
    /// <param name="activationTypes">Runtime-owned types that activate the module when used by cooked content.</param>
    public GeneratedRuntimeModuleManifestAttribute(
        string moduleId,
        Type registrationType,
        string registrationMethodName,
        params Type[] activationTypes) {
        if (string.IsNullOrWhiteSpace(moduleId)) {
            throw new ArgumentException("Runtime module id is required.", nameof(moduleId));
        } else if (registrationType == null) {
            throw new ArgumentNullException(nameof(registrationType));
        } else if (string.IsNullOrWhiteSpace(registrationMethodName)) {
            throw new ArgumentException("Runtime module registration method is required.", nameof(registrationMethodName));
        } else if (activationTypes == null) {
            throw new ArgumentNullException(nameof(activationTypes));
        } else if (activationTypes.Length == 0) {
            throw new ArgumentException("At least one activation type is required.", nameof(activationTypes));
        } else if (Array.Exists(activationTypes, activationType => activationType == null)) {
            throw new ArgumentException("Activation types cannot contain null entries.", nameof(activationTypes));
        }

        ModuleId = moduleId;
        RegistrationType = registrationType;
        RegistrationMethodName = registrationMethodName;
        ActivationTypes = [.. activationTypes];
    }

    /// <summary>
    /// Gets the stable runtime module id used for deterministic emission order.
    /// </summary>
    public string ModuleId { get; }

    /// <summary>
    /// Gets the static registration type that should be emitted into the generated bootstrap source.
    /// </summary>
    public Type RegistrationType { get; }

    /// <summary>
    /// Gets the static registration method that should be invoked by generated core.
    /// </summary>
    public string RegistrationMethodName { get; }

    /// <summary>
    /// Gets the runtime-owned activation types that enable this module when they are used by cooked content.
    /// </summary>
    public Type[] ActivationTypes { get; }
}
```

Add these helpers to `EditorGeneratedCoreRegenerationService.cs`:
```csharp
internal static IReadOnlyList<GeneratedRuntimeModuleManifestAttribute> DiscoverGeneratedRuntimeModuleManifests(
    IReadOnlyList<Assembly> assemblies) {
    if (assemblies == null) {
        throw new ArgumentNullException(nameof(assemblies));
    }

    Dictionary<string, GeneratedRuntimeModuleManifestAttribute> manifestsById = new(StringComparer.Ordinal);
    for (int index = 0; index < assemblies.Count; index++) {
        Assembly assembly = assemblies[index];
        if (assembly == null) {
            continue;
        }

        GeneratedRuntimeModuleManifestAttribute[] manifests = assembly
            .GetCustomAttributes<GeneratedRuntimeModuleManifestAttribute>()
            .ToArray();
        for (int manifestIndex = 0; manifestIndex < manifests.Length; manifestIndex++) {
            GeneratedRuntimeModuleManifestAttribute manifest = manifests[manifestIndex];
            if (manifestsById.ContainsKey(manifest.ModuleId)) {
                throw new InvalidOperationException($"Duplicate generated runtime module id '{manifest.ModuleId}' was declared.");
            }

            manifestsById.Add(manifest.ModuleId, manifest);
        }
    }

    return manifestsById.Values
        .OrderBy(manifest => manifest.ModuleId, StringComparer.Ordinal)
        .ToArray();
}

internal static IReadOnlyList<GeneratedRuntimeModuleManifestAttribute> ResolveActiveGeneratedRuntimeModuleManifests(
    IReadOnlyList<GeneratedRuntimeModuleManifestAttribute> manifests,
    IReadOnlyList<Type> usedTypes) {
    if (manifests == null) {
        throw new ArgumentNullException(nameof(manifests));
    }
    if (usedTypes == null) {
        throw new ArgumentNullException(nameof(usedTypes));
    }

    HashSet<Type> usedTypeSet = new HashSet<Type>(usedTypes);
    List<GeneratedRuntimeModuleManifestAttribute> activeManifests = new List<GeneratedRuntimeModuleManifestAttribute>();
    for (int index = 0; index < manifests.Count; index++) {
        GeneratedRuntimeModuleManifestAttribute manifest = manifests[index];
        if (manifest.ActivationTypes.Any(usedTypeSet.Contains)) {
            activeManifests.Add(manifest);
        }
    }

    return activeManifests
        .OrderBy(manifest => manifest.ModuleId, StringComparer.Ordinal)
        .ToArray();
}
```

- [ ] **Step 4: Run the editor tests to verify they pass**

Run:
```bash
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter "FullyQualifiedName~GeneratedRuntimeModule"
```

Expected: PASS with both discovery tests green.

- [ ] **Step 5: Commit**

```bash
rtk git add C:\dev\helworks\helengine\engine\helengine.core\runtime\GeneratedRuntimeModuleManifestAttribute.cs C:\dev\helworks\helengine\engine\helengine.editor\managers\project\EditorGeneratedCoreRegenerationService.cs C:\dev\helworks\helengine\engine\helengine.editor.tests\AssemblyInfo.cs C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\GeneratedRuntimeModuleRegistrationTestComponent.cs C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\GeneratedRuntimeModuleRegistrationTestRegistration.cs C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\EditorGeneratedCoreRegenerationServiceTests.cs
rtk git commit -m "feat: add generated runtime module manifest contract"
```

### Task 2: Emit generated runtime module bootstrap files from cooked-scene type usage

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\managers\project\EditorGeneratedCoreRegenerationService.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor\managers\project\EditorPlatformBuildGraphRunner.cs`
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\EditorGeneratedCoreRegenerationServiceTests.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\EditorGeneratedCoreRegenerationServiceTests.cs`

- [ ] **Step 1: Write the failing bootstrap emission tests**

Append these tests to `EditorGeneratedCoreRegenerationServiceTests.cs`:
```csharp
[Fact]
public void EnsureGeneratedRuntimeModuleRegistrationSupport_WhenNoModulesAreActive_EmitsNoOpBootstrap() {
    string generatedCoreRootPath = Path.Combine(RootPath, "generated-runtime-modules-empty");
    try {
        EditorGeneratedCoreRegenerationService.EnsureGeneratedRuntimeModuleRegistrationSupport(generatedCoreRootPath);

        string headerPath = Path.Combine(generatedCoreRootPath, "GeneratedRuntimeModuleRegistration.hpp");
        string sourcePath = Path.Combine(generatedCoreRootPath, "GeneratedRuntimeModuleRegistration.cpp");

        Assert.True(File.Exists(headerPath));
        Assert.True(File.Exists(sourcePath));
        Assert.Contains("void RegisterGeneratedRuntimeModules(Core* core);", File.ReadAllText(headerPath), StringComparison.Ordinal);
        Assert.Contains("void RegisterGeneratedRuntimeModules(Core* core)", File.ReadAllText(sourcePath), StringComparison.Ordinal);
        Assert.DoesNotContain("::Register(core);", File.ReadAllText(sourcePath), StringComparison.Ordinal);
    } finally {
        DeleteDirectoryIfPresent(generatedCoreRootPath);
    }
}

[Fact]
public void EmitGeneratedRuntimeModuleRegistration_WhenActivationTypeIsUsed_EmitsRegistrationCall() {
    string generatedCoreRootPath = Path.Combine(RootPath, "generated-runtime-modules-active");
    try {
        EditorGeneratedCoreRegenerationService.EnsureGeneratedRuntimeModuleRegistrationSupport(generatedCoreRootPath);
        EditorGeneratedCoreRegenerationService.EmitGeneratedRuntimeModuleRegistration(
            generatedCoreRootPath,
            [typeof(GeneratedRuntimeModuleRegistrationTestComponent)]);

        string sourcePath = Path.Combine(generatedCoreRootPath, "GeneratedRuntimeModuleRegistration.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("#include \"GeneratedRuntimeModuleRegistrationTestRegistration.hpp\"", source, StringComparison.Ordinal);
        Assert.Contains("GeneratedRuntimeModuleRegistrationTestRegistration::Register(core);", source, StringComparison.Ordinal);
    } finally {
        DeleteDirectoryIfPresent(generatedCoreRootPath);
    }
}
```

- [ ] **Step 2: Run the editor tests to verify they fail**

Run:
```bash
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter "FullyQualifiedName~GeneratedRuntimeModule"
```

Expected: FAIL because `EnsureGeneratedRuntimeModuleRegistrationSupport` and `EmitGeneratedRuntimeModuleRegistration` do not exist yet.

- [ ] **Step 3: Implement generated runtime module bootstrap emission and wire it into the cooked-scene pass**

Add these methods to `EditorGeneratedCoreRegenerationService.cs`:
```csharp
internal static void EnsureGeneratedRuntimeModuleRegistrationSupport(string generatedCoreRootPath) {
    if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
        throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
    }

    Directory.CreateDirectory(generatedCoreRootPath);
    File.WriteAllText(
        Path.Combine(generatedCoreRootPath, "GeneratedRuntimeModuleRegistration.hpp"),
        "#pragma once\nclass Core;\nvoid RegisterGeneratedRuntimeModules(Core* core);\n");
    File.WriteAllText(
        Path.Combine(generatedCoreRootPath, "GeneratedRuntimeModuleRegistration.cpp"),
        "#include \"GeneratedRuntimeModuleRegistration.hpp\"\n#include \"Core.hpp\"\nvoid RegisterGeneratedRuntimeModules(Core* core) {\n    if (core == nullptr) {\n        throw new ArgumentNullException(\"core\");\n    }\n}\n");
}

internal static void EmitGeneratedRuntimeModuleRegistration(
    string generatedCoreRootPath,
    IReadOnlyList<Type> usedTypes) {
    if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
        throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
    }
    if (usedTypes == null) {
        throw new ArgumentNullException(nameof(usedTypes));
    }

    IReadOnlyList<Assembly> assemblies = usedTypes
        .Select(type => type.Assembly)
        .Distinct()
        .ToArray();
    IReadOnlyList<GeneratedRuntimeModuleManifestAttribute> manifests =
        DiscoverGeneratedRuntimeModuleManifests(assemblies);
    IReadOnlyList<GeneratedRuntimeModuleManifestAttribute> activeManifests =
        ResolveActiveGeneratedRuntimeModuleManifests(manifests, usedTypes);

    File.WriteAllText(
        Path.Combine(generatedCoreRootPath, "GeneratedRuntimeModuleRegistration.hpp"),
        BuildGeneratedRuntimeModuleRegistrationHeader());
    File.WriteAllText(
        Path.Combine(generatedCoreRootPath, "GeneratedRuntimeModuleRegistration.cpp"),
        BuildGeneratedRuntimeModuleRegistrationSource(activeManifests));
}
```

Use these source builders in the same file:
```csharp
static string BuildGeneratedRuntimeModuleRegistrationHeader() {
    return "#pragma once\nclass Core;\nvoid RegisterGeneratedRuntimeModules(Core* core);\n";
}

static string BuildGeneratedRuntimeModuleRegistrationSource(
    IReadOnlyList<GeneratedRuntimeModuleManifestAttribute> manifests) {
    StringBuilder builder = new StringBuilder();
    builder.AppendLine("#include \"GeneratedRuntimeModuleRegistration.hpp\"");
    builder.AppendLine("#include \"Core.hpp\"");
    builder.AppendLine("#include \"runtime/native_exceptions.hpp\"");
    foreach (GeneratedRuntimeModuleManifestAttribute manifest in manifests) {
        builder.AppendLine($"#include \"{manifest.RegistrationType.Name}.hpp\"");
    }
    builder.AppendLine();
    builder.AppendLine("void RegisterGeneratedRuntimeModules(Core* core) {");
    builder.AppendLine("    if (core == nullptr) {");
    builder.AppendLine("        throw new ArgumentNullException(\"core\");");
    builder.AppendLine("    }");
    foreach (GeneratedRuntimeModuleManifestAttribute manifest in manifests) {
        builder.AppendLine($"    {manifest.RegistrationType.Name}::{manifest.RegistrationMethodName}(core);");
    }
    builder.AppendLine("}");
    return builder.ToString();
}
```

Wire the stub into initial regeneration:
```csharp
AppendRegenerationLog(regenerationLogPath, "runtime-module-support-start");
EnsureGeneratedRuntimeModuleRegistrationSupport(generatedCoreOutputRoot);
AppendRegenerationLog(regenerationLogPath, "runtime-module-support-complete");
```

Wire the scene-driven refresh into `EditorPlatformBuildGraphRunner.EmitGeneratedRuntimeComponentDeserializersForCookedScenes(...)` immediately after `sceneReferencedComponentTypes` are available:
```csharp
IReadOnlyList<Type> sceneReferencedComponentTypes =
    EditorGeneratedCoreRegenerationService.DiscoverAutomaticRuntimeComponentTypesFromCookedScenes(
        cookedSceneAssetPaths,
        ScriptTypeResolver);

EditorGeneratedCoreRegenerationService.EmitGeneratedRuntimeModuleRegistration(
    generatedCoreRootPath,
    sceneReferencedComponentTypes);
EditorGeneratedCoreRegenerationService.EmitCookedSceneAutomaticRuntimeComponentDeserializers(
    generatedCoreRootPath,
    cookedSceneAssetPaths,
    ScriptTypeResolver,
    platformDefinition);
EditorGeneratedCoreRegenerationService.WriteGeneratedCoreTranslationUnit(generatedCoreRootPath);
```

- [ ] **Step 4: Run the editor tests to verify they pass**

Run:
```bash
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter "FullyQualifiedName~GeneratedRuntimeModule"
```

Expected: PASS with the discovery and emission tests green.

- [ ] **Step 5: Commit**

```bash
rtk git add C:\dev\helworks\helengine\engine\helengine.editor\managers\project\EditorGeneratedCoreRegenerationService.cs C:\dev\helworks\helengine\engine\helengine.editor\managers\project\EditorPlatformBuildGraphRunner.cs C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\EditorGeneratedCoreRegenerationServiceTests.cs
rtk git commit -m "feat: emit generated runtime module bootstrap"
```

### Task 3: Migrate physics3d to the manifest-driven generated bootstrap

**Files:**
- Create: `C:\dev\helworks\helengine\engine\helengine.physics3d\AssemblyInfo.cs`
- Create: `C:\dev\helworks\helengine\engine\helengine.physics3d.tests\GeneratedRuntimeModuleManifestTests.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.physics3d.tests\GeneratedRuntimeModuleManifestTests.cs`

- [ ] **Step 1: Write the failing physics manifest test**

Create `GeneratedRuntimeModuleManifestTests.cs`:
```csharp
namespace helengine.physics3d.tests;

/// <summary>
/// Verifies the physics runtime declares one generated runtime module manifest for the generic native bootstrap seam.
/// </summary>
public sealed class GeneratedRuntimeModuleManifestTests {
    /// <summary>
    /// Ensures the physics runtime contributes the expected module id, registration entrypoint, and activation types.
    /// </summary>
    [Fact]
    public void Assembly_declares_generated_runtime_module_manifest() {
        GeneratedRuntimeModuleManifestAttribute manifest = Assert.Single(
            typeof(PhysicsWorld3D).Assembly.GetCustomAttributes(typeof(GeneratedRuntimeModuleManifestAttribute), false)
                .Cast<GeneratedRuntimeModuleManifestAttribute>());

        Assert.Equal("physics3d", manifest.ModuleId);
        Assert.Equal(typeof(Physics3DRuntimeComponentRegistration), manifest.RegistrationType);
        Assert.Equal(nameof(Physics3DRuntimeComponentRegistration.Register), manifest.RegistrationMethodName);
        Assert.Contains(typeof(RigidBody3D), manifest.ActivationTypes);
        Assert.Contains(typeof(BoxCollider3D), manifest.ActivationTypes);
        Assert.Contains(typeof(SphereCollider3D), manifest.ActivationTypes);
    }
}
```

- [ ] **Step 2: Run the physics tests to verify they fail**

Run:
```bash
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.physics3d.tests\helengine.physics3d.tests.csproj --filter "FullyQualifiedName~GeneratedRuntimeModuleManifestTests"
```

Expected: FAIL because the assembly-level manifest does not exist yet.

- [ ] **Step 3: Add the physics3d assembly manifest**

Create `C:\dev\helworks\helengine\engine\helengine.physics3d\AssemblyInfo.cs`:
```csharp
using helengine;

[assembly: GeneratedRuntimeModuleManifest(
    "physics3d",
    typeof(Physics3DRuntimeComponentRegistration),
    nameof(Physics3DRuntimeComponentRegistration.Register),
    typeof(RigidBody3D),
    typeof(BoxCollider3D),
    typeof(SphereCollider3D),
    typeof(CapsuleCollider3D),
    typeof(StaticMeshCollider3D),
    typeof(CharacterController3D))]
```

- [ ] **Step 4: Run the physics tests to verify they pass**

Run:
```bash
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.physics3d.tests\helengine.physics3d.tests.csproj --filter "FullyQualifiedName~GeneratedRuntimeModuleManifestTests"
```

Expected: PASS with the manifest test green.

- [ ] **Step 5: Commit**

```bash
rtk git add C:\dev\helworks\helengine\engine\helengine.physics3d\AssemblyInfo.cs C:\dev\helworks\helengine\engine\helengine.physics3d.tests\GeneratedRuntimeModuleManifestTests.cs
rtk git commit -m "feat: declare physics runtime module manifest"
```

### Task 4: Migrate Wii and GameCube hosts to the generic generated bootstrap seam

**Files:**
- Modify: `C:\dev\helworks\helengine-wii\src\platform\wii\WiiApplication.cpp`
- Modify: `C:\dev\helworks\helengine-wii\Makefile`
- Modify: `C:\dev\helworks\helengine-wii\builder.tests\WiiRuntimeSourceTests.cs`
- Modify: `C:\dev\helworks\helengine-gc\src\platform\gamecube\GameCubeApplication.cpp`
- Modify: `C:\dev\helworks\helengine-gc\Makefile`
- Modify: `C:\dev\helworks\helengine-gc\builder.tests\GameCubePackagedRuntimeSourceTests.cs`
- Test: `C:\dev\helworks\helengine-wii\builder.tests\WiiRuntimeSourceTests.cs`
- Test: `C:\dev\helworks\helengine-gc\builder.tests\GameCubePackagedRuntimeSourceTests.cs`

- [ ] **Step 1: Update the Wii and GameCube source-contract tests first**

Replace the physics-specific expectations with generic generated bootstrap expectations.

In `WiiRuntimeSourceTests.cs`, update the test to assert:
```csharp
Assert.DoesNotContain("HELENGINE_WII_HAS_PHYSICS3D_RUNTIME_REGISTRATION", makefileSource, StringComparison.Ordinal);
Assert.Contains("#include \"GeneratedRuntimeModuleRegistration.hpp\"", applicationSource, StringComparison.Ordinal);
Assert.Contains("RegisterGeneratedRuntimeModules(EngineCore);", applicationSource, StringComparison.Ordinal);
Assert.DoesNotContain("#include \"Physics3DRuntimeComponentRegistration.hpp\"", applicationSource, StringComparison.Ordinal);
Assert.DoesNotContain("Physics3DRuntimeComponentRegistration::Register(EngineCore);", applicationSource, StringComparison.Ordinal);
```

In `GameCubePackagedRuntimeSourceTests.cs`, update the test to assert:
```csharp
Assert.DoesNotContain("HELENGINE_GAMECUBE_HAS_PHYSICS3D_RUNTIME_REGISTRATION", makefileSource, StringComparison.Ordinal);
Assert.Contains("#include \"GeneratedRuntimeModuleRegistration.hpp\"", applicationSource, StringComparison.Ordinal);
Assert.Contains("RegisterGeneratedRuntimeModules(EngineCore);", applicationSource, StringComparison.Ordinal);
Assert.DoesNotContain("#include \"Physics3DRuntimeComponentRegistration.hpp\"", applicationSource, StringComparison.Ordinal);
Assert.DoesNotContain("Physics3DRuntimeComponentRegistration::Register(EngineCore);", applicationSource, StringComparison.Ordinal);
```

- [ ] **Step 2: Run the Wii and GameCube tests to verify they fail**

Run:
```bash
rtk dotnet test C:\dev\helworks\helengine-wii\builder.tests\helengine.wii.builder.tests.csproj --filter "WiiRuntimeSourceTests.PackagedBootstrap_RegistersPhysicsRuntimeConditionallyBeforeStartupSceneLoad" --no-restore
rtk dotnet test C:\dev\helworks\helengine-gc\builder.tests\helengine.gamecube.builder.tests.csproj --filter "GameCubePackagedRuntimeSourceTests.PackagedDiscBootSource_RegistersPhysicsRuntimeConditionallyBeforeStartupSceneLoad" --no-restore
```

Expected: FAIL because both hosts still reference the physics-specific seam.

- [ ] **Step 3: Replace the platform-specific physics seam with the generated bootstrap call**

In `WiiApplication.cpp`, replace the include and registration block with:
```cpp
#include "PlatformInfo.hpp"
#include "GeneratedRuntimeModuleRegistration.hpp"
#include "RuntimeSceneLoadService.hpp"
```

and:
```cpp
initializationStage = "InitializeCore";
EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputManager, EnginePlatformInfo, options);
SYS_Report("[Wii] Engine core initialized.\n");
AppendRuntimeTrace("[WiiFile] Engine core initialized.\n");
initializationStage = "RegisterGeneratedRuntimeModules";
RegisterGeneratedRuntimeModules(EngineCore);
SYS_Report("[Wii] Generated runtime modules registered.\n");
AppendRuntimeTrace("[WiiFile] Generated runtime modules registered.\n");
```

Remove the `HELENGINE_WII_HAS_PHYSICS3D_RUNTIME_REGISTRATION` branches from `Makefile`.

In `GameCubeApplication.cpp`, replace the include and registration block with:
```cpp
#include "PlatformInfo.hpp"
#include "GeneratedRuntimeModuleRegistration.hpp"
#include "RuntimeSceneLoadService.hpp"
```

and:
```cpp
initializationStage = "InitializeCore";
EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputManager, EnginePlatformInfo, options);
SYS_Report("[GC] Engine core initialized.\n");
initializationStage = "RegisterGeneratedRuntimeModules";
RegisterGeneratedRuntimeModules(EngineCore);
SYS_Report("[GC] Generated runtime modules registered.\n");
```

Remove the `HELENGINE_GAMECUBE_HAS_PHYSICS3D_RUNTIME_REGISTRATION` branches from `C:\dev\helworks\helengine-gc\Makefile`.

- [ ] **Step 4: Run the targeted Wii and GameCube tests to verify they pass**

Run:
```bash
rtk dotnet test C:\dev\helworks\helengine-wii\builder.tests\helengine.wii.builder.tests.csproj --filter "WiiRuntimeSourceTests.PackagedBootstrap_RegistersPhysicsRuntimeConditionallyBeforeStartupSceneLoad" --no-restore
rtk dotnet test C:\dev\helworks\helengine-gc\builder.tests\helengine.gamecube.builder.tests.csproj --filter "GameCubePackagedRuntimeSourceTests.PackagedDiscBootSource_RegistersPhysicsRuntimeConditionallyBeforeStartupSceneLoad" --no-restore
```

Expected: PASS with both platform source-contract tests green.

- [ ] **Step 5: Commit**

```bash
rtk git -C C:\dev\helworks\helengine-wii add C:\dev\helworks\helengine-wii\src\platform\wii\WiiApplication.cpp C:\dev\helworks\helengine-wii\Makefile C:\dev\helworks\helengine-wii\builder.tests\WiiRuntimeSourceTests.cs
rtk git -C C:\dev\helworks\helengine-gc add C:\dev\helworks\helengine-gc\src\platform\gamecube\GameCubeApplication.cpp C:\dev\helworks\helengine-gc\Makefile C:\dev\helworks\helengine-gc\builder.tests\GameCubePackagedRuntimeSourceTests.cs
rtk git -C C:\dev\helworks\helengine-wii commit -m "feat: use generated runtime module bootstrap on wii"
rtk git -C C:\dev\helworks\helengine-gc commit -m "feat: use generated runtime module bootstrap on gamecube"
```

### Task 5: Run the narrow cross-project verification sweep

**Files:**
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\managers\project\EditorGeneratedCoreRegenerationServiceTests.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.physics3d.tests\GeneratedRuntimeModuleManifestTests.cs`
- Test: `C:\dev\helworks\helengine-wii\builder.tests\WiiRuntimeSourceTests.cs`
- Test: `C:\dev\helworks\helengine-gc\builder.tests\GameCubePackagedRuntimeSourceTests.cs`

- [ ] **Step 1: Run the editor runtime-module tests**

Run:
```bash
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter "FullyQualifiedName~GeneratedRuntimeModule"
```

Expected: PASS.

- [ ] **Step 2: Run the physics manifest test**

Run:
```bash
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.physics3d.tests\helengine.physics3d.tests.csproj --filter "FullyQualifiedName~GeneratedRuntimeModuleManifestTests"
```

Expected: PASS.

- [ ] **Step 3: Run the targeted Wii and GameCube source-contract tests**

Run:
```bash
rtk dotnet test C:\dev\helworks\helengine-wii\builder.tests\helengine.wii.builder.tests.csproj --filter "WiiRuntimeSourceTests.PackagedBootstrap_RegistersPhysicsRuntimeConditionallyBeforeStartupSceneLoad" --no-restore
rtk dotnet test C:\dev\helworks\helengine-gc\builder.tests\helengine.gamecube.builder.tests.csproj --filter "GameCubePackagedRuntimeSourceTests.PackagedDiscBootSource_RegistersPhysicsRuntimeConditionallyBeforeStartupSceneLoad" --no-restore
```

Expected: PASS.

- [ ] **Step 4: Commit the plan-complete integration checkpoint**

```bash
rtk git -C C:\dev\helworks\helengine status --short
rtk git -C C:\dev\helworks\helengine-wii status --short
rtk git -C C:\dev\helworks\helengine-gc status --short
```

Expected: only the planned engine/Wii/GameCube files are modified or committed.
