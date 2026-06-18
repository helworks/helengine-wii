# Wii Packaged Scene List Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make packaged Wii scene selection from the main menu work by ensuring the packaged Wii build receives, stages, and emits the same authored scene list as the Windows demo-disc build.

**Architecture:** Fix the build-system path that narrows `PlatformBuildManifest.Scenes` for packaged Wii relative to Windows. Keep staging, runtime manifest generation, and ISO packaging unchanged so they continue to derive from the corrected manifest.

**Tech Stack:** C#, xUnit, platform build manifests, Wii packaged workspace builder, runtime scene manifest generation, Docker Wii native build, Dolphin ISO verification

---

## File Structure

- Modify: `builder.tests/WiiPackagedBuildWorkspaceTests.cs`
  - Add focused coverage proving packaged Wii keeps the expected multi-scene set instead of only the startup scene.
- Modify: `builder/WiiBuildWorkspace.cs`
  - Fix the packaged Wii workspace/build path so it preserves the authored scene set from the incoming manifest.
- Modify: `builder.tests/WiiRuntimeSceneManifestWriterTests.cs`
  - Extend runtime manifest coverage if needed so multiple scenes are emitted, not just the startup scene.

## Task 1: Add the Failing Multi-Scene Wii Build Test

**Files:**
- Modify: `builder.tests/WiiPackagedBuildWorkspaceTests.cs`
- Test: `builder.tests/helengine.wii.builder.tests.csproj`

- [ ] **Step 1: Write the failing packaged-workspace test**

Add a test that feeds multiple authored scenes into the Wii packaged build request and expects them all to survive into the staged disc:

```csharp
[Fact]
public async Task BuildPackagedAsync_PreservesAllManifestScenesIntoDiscStage() {
    string tempRootPath = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));
    string sourceRootPath = Path.Combine(tempRootPath, "source");
    string outputRootPath = Path.Combine(tempRootPath, "output");
    string workingRootPath = Path.Combine(tempRootPath, "working");
    Directory.CreateDirectory(Path.Combine(sourceRootPath, "cooked", "scenes"));

    WriteSceneArtifact(sourceRootPath, "DemoDiscMainMenu.hasset");
    WriteSceneArtifact(sourceRootPath, "PhysicsSandbox.hasset");
    WriteSceneArtifact(sourceRootPath, "LightingShowcase.hasset");

    PlatformBuildRequest request = CreatePackagedRequest(
        sourceRootPath,
        outputRootPath,
        workingRootPath,
        [
            CreateScene("Scenes/DemoDiscMainMenu.helen", "cooked/scenes/DemoDiscMainMenu.hasset"),
            CreateScene("Scenes/PhysicsSandbox.helen", "cooked/scenes/PhysicsSandbox.hasset"),
            CreateScene("Scenes/LightingShowcase.helen", "cooked/scenes/LightingShowcase.hasset")
        ],
        [
            new PlatformBuildArtifact("cooked/scenes/DemoDiscMainMenu.hasset", "scene-main", "scene", "wii-default"),
            new PlatformBuildArtifact("cooked/scenes/PhysicsSandbox.hasset", "scene-physics", "scene", "wii-default"),
            new PlatformBuildArtifact("cooked/scenes/LightingShowcase.hasset", "scene-lighting", "scene", "wii-default")
        ]);

    PlatformBuildReport report = await WiiBuildWorkspace.BuildPackagedAsync(
        request,
        new FakePlatformBuildProgressReporter(),
        new FakePlatformBuildDiagnosticReporter(),
        CancellationToken.None,
        new FakeWiiNativeBuildExecutor(),
        new FakeWiiImagePackager(),
        new WiiDiscSystemAreaOptions("RCIE01", "city"));

    Assert.True(report.Succeeded);
    Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "files", "cooked", "scenes", "DemoDiscMainMenu.hasset")));
    Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "files", "cooked", "scenes", "PhysicsSandbox.hasset")));
    Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "files", "cooked", "scenes", "LightingShowcase.hasset")));
}
```

- [ ] **Step 2: Add or reuse local helpers in the test file**

If the file does not already have reusable helpers, add simple test helpers consistent with the existing style:

```csharp
static void WriteSceneArtifact(string sourceRootPath, string fileName) {
    string fullPath = Path.Combine(sourceRootPath, "cooked", "scenes", fileName);
    Directory.CreateDirectory(Path.GetDirectoryName(fullPath)!);
    File.WriteAllText(fullPath, "scene-bytes");
}

static PlatformBuildScene CreateScene(string sceneId, string cookedRelativePath) {
    return new PlatformBuildScene(
        sceneId,
        cookedRelativePath,
        cookedRelativePath,
        [new KeyValuePair<string, string>("cooked-relative-path", cookedRelativePath)]);
}
```

Keep helper style aligned with the current test file if equivalent helpers already exist.

- [ ] **Step 3: Run the focused test to verify it fails**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter BuildPackagedAsync_PreservesAllManifestScenesIntoDiscStage
```

Expected: `FAIL` because the current packaged Wii path drops scenes that Windows keeps.

- [ ] **Step 4: Commit the red test**

```bash
git add builder.tests/WiiPackagedBuildWorkspaceTests.cs
git commit -m "test: add failing Wii packaged scene parity coverage"
```

## Task 2: Fix the Packaged Wii Manifest/Stage Path

**Files:**
- Modify: `builder/WiiBuildWorkspace.cs`
- Test: `builder.tests/WiiPackagedBuildWorkspaceTests.cs`

- [ ] **Step 1: Identify the current narrowing point and remove it**

In `builder/WiiBuildWorkspace.cs`, inspect any code path that rebuilds scene outcomes, runtime-scene-manifest input, or staged scene artifacts from less than `request.Manifest.Scenes`.

The implementation must preserve the full incoming scene set:

```csharp
List<PlatformBuildItemOutcome> sceneOutcomes = BuildSuccessfulSceneOutcomes(request.Manifest.Scenes);
WriteRuntimeSceneManifest(paths, request.Manifest);
StageCookedArtifacts(request, paths.StagingRootPath, progressReporter, diagnosticReporter, diagnostics, cancellationToken);
```

If there is a helper that synthesizes a startup-only manifest for Wii, remove that narrowing and pass the full authored manifest through instead.

- [ ] **Step 2: Keep runtime manifest generation manifest-driven**

Do not add special-case scene injection to `WiiRuntimeSceneManifestWriter`. The desired state is that the existing writer naturally emits all scenes because the manifest already contains them.

If `WiiBuildWorkspace` currently creates a reduced intermediate manifest, replace that with one that preserves:

```csharp
request.Manifest.Scenes
```

and keeps:

```csharp
request.Manifest.StartupSceneId
```

unchanged.

- [ ] **Step 3: Run the focused packaged-workspace test to verify it passes**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter BuildPackagedAsync_PreservesAllManifestScenesIntoDiscStage
```

Expected: `PASS`

- [ ] **Step 4: Commit the green fix**

```bash
git add builder/WiiBuildWorkspace.cs builder.tests/WiiPackagedBuildWorkspaceTests.cs
git commit -m "fix: preserve full packaged Wii scene list"
```

## Task 3: Lock Runtime Manifest Multi-Scene Emission if Needed

**Files:**
- Modify: `builder.tests/WiiRuntimeSceneManifestWriterTests.cs`
- Test: `builder.tests/helengine.wii.builder.tests.csproj`

- [ ] **Step 1: Add or extend manifest-writer coverage for multiple scenes**

If current coverage only proves the startup scene is emitted, extend it to prove multiple scenes are written:

```csharp
Assert.Contains("\"Scenes/DemoDiscMainMenu.helen\"", source, StringComparison.Ordinal);
Assert.Contains("\"Scenes/PhysicsSandbox.helen\"", source, StringComparison.Ordinal);
Assert.Contains("\"Scenes/LightingShowcase.helen\"", source, StringComparison.Ordinal);
Assert.Contains("\"cooked/scenes/PhysicsSandbox.hasset\"", source, StringComparison.Ordinal);
Assert.Contains("\"cooked/scenes/LightingShowcase.hasset\"", source, StringComparison.Ordinal);
```

- [ ] **Step 2: Run the focused runtime manifest writer test**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter WiiRuntimeSceneManifestWriterTests
```

Expected: `PASS`

- [ ] **Step 3: Commit only if this test file changed**

```bash
git add builder.tests/WiiRuntimeSceneManifestWriterTests.cs
git commit -m "test: cover multi-scene Wii runtime manifest output"
```

Skip this commit if no changes were required.

## Task 4: Run the Packaged Builder Slice

**Files:**
- Test: `builder.tests/helengine.wii.builder.tests.csproj`

- [ ] **Step 1: Run the packaged Wii builder slice**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter Packaged
```

Expected: `PASS`

- [ ] **Step 2: Commit only if follow-up test adjustments were required**

If the packaged slice exposed assertion drift and you changed tests:

```bash
git add builder.tests/WiiPackagedBuildWorkspaceTests.cs builder.tests/WiiRuntimeSceneManifestWriterTests.cs
git commit -m "test: align Wii packaged scene parity coverage"
```

Otherwise skip this commit.

## Task 5: Rebuild, Repackage, and Prove Scene Loading in Dolphin

**Files:**
- Runtime artifact: `build/helengine_wii.dol`
- Disc root: `tmp/self-apploader-package-v3/disc-refresh-v56`
- ISO: `tmp/self-apploader-package-v3/city-self-apploader-v56.iso`

- [ ] **Step 1: Rebuild the packaged Wii native runtime**

Run:

```powershell
rtk proxy docker run --rm -v C:/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii -e HELENGINE_WII_BOOT_MODE=packaged-disc helengine-wii sh -lc "make 2>&1 | tail -c 12000"
```

Expected: `build/helengine_wii.dol` rebuilds successfully.

- [ ] **Step 2: Regenerate the packaged disc layout**

Run:

```powershell
rtk dotnet C:\dev\helworks\helengine-wii\builder\bin\Debug\net9.0-windows\helengine.wii.builder.dll --write-disc-layout C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\staging C:\dev\helworks\helengine-wii\build\helengine_wii.dol C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\disc-refresh-v56 RCIE01 city
```

Expected: `disc-refresh-v56` is regenerated.

- [ ] **Step 3: Verify the staged DOL matches the rebuilt DOL**

Run:

```powershell
Get-FileHash C:\dev\helworks\helengine-wii\build\helengine_wii.dol, C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\disc-refresh-v56\sys\main.dol -Algorithm SHA256
```

Expected: both hashes are identical.

- [ ] **Step 4: Package the fresh ISO**

Run:

```powershell
$env:HELENGINE_WII_WIT_PATH='C:\dev\helworks\helengine-wii\tmp\tools\wit-v3.05a-r8638-cygwin64\bin\wit.exe'
rtk dotnet C:\dev\helworks\helengine-wii\builder\bin\Debug\net9.0-windows\helengine.wii.builder.dll --package-image C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\disc-refresh-v56 C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\city-self-apploader-v56.iso
```

Expected: `city-self-apploader-v56.iso` is produced.

- [ ] **Step 5: Launch Dolphin on the fresh ISO**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-wii\tmp\launch_wii_iso_in_dolphin.ps1 -IsoPath C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\city-self-apploader-v56.iso
```

Expected: Dolphin opens on the new ISO.

- [ ] **Step 6: Manually verify a menu scene now loads**

In the visible Dolphin window:

- navigate to one of the non-startup demo scenes
- confirm selection
- verify that the selected scene actually loads instead of failing due to a missing packaged scene

Expected: the scene transition succeeds because the scene is now present in the packaged Wii manifest and ISO.

- [ ] **Step 7: Commit the verified scene-parity result**

```bash
git add builder/WiiBuildWorkspace.cs builder.tests/WiiPackagedBuildWorkspaceTests.cs builder.tests/WiiRuntimeSceneManifestWriterTests.cs
git commit -m "fix: align packaged Wii scenes with Windows build"
```
