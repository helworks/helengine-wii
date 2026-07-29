# Wii Loader-Safe Disc Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make packaged Wii builds reuse the loader-open encrypted partition through libdi and keep runtime failure codes visible until shutdown.

**Architecture:** `WiiSceneBootstrap` will reopen only the process-local `/dev/di` descriptor and hand the already-open partition to `WiiDiscFileSystem`; it will no longer reset, mount, inspect, or reopen the physical disc. `WiiDiscFileSystem` will track explicit opened-partition readiness while preserving apploader FST and partition-relative `DI_Read` behavior. `WiiApplication` will route every post-video runtime failure through one shutdown-aware failure presentation loop.

**Tech Stack:** C++20, devkitPPC/libogc `libdi`, C#/.NET 9 xUnit source-contract tests, Docker Wii native build, Wiimms ISO Tools.

---

## File Structure

- Modify `builder.tests/WiiRuntimeSourceTests.cs`: replace the obsolete DVD-remount source contract and add the persistent runtime-failure contract.
- Modify `src/platform/wii/WiiSceneBootstrap.cpp`: reduce packaged storage startup to `DI_Init` plus opened-partition initialization.
- Modify `src/platform/wii/WiiDiscFileSystem.hpp`: expose an accurately named opened-partition initializer.
- Modify `src/platform/wii/WiiDiscFileSystem.cpp`: replace physical-offset state with explicit partition readiness while preserving relative reads.
- Modify `src/platform/wii/WiiApplication.hpp`: declare the shared failure presentation loop.
- Modify `src/platform/wii/WiiApplication.cpp`: use the shared loop for initialization, update, and draw failures.

### Task 1: Establish the loader-safe storage source contract

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Test: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Replace the obsolete packaged-storage assertions with the loader-safe contract**

Update `PackagedBootstrap_DeclaresPackagedDiscHelpers` so its storage assertions are:

```csharp
Assert.Contains("DI_Init()", bootstrapSource, StringComparison.Ordinal);
Assert.Contains("WiiDiscFileSystem::InitializeOpenedPartition();", bootstrapSource, StringComparison.Ordinal);
Assert.DoesNotContain("DVD_Init", bootstrapSource, StringComparison.Ordinal);
Assert.DoesNotContain("DVD_MountAsync", bootstrapSource, StringComparison.Ordinal);
Assert.DoesNotContain("DVD_ReadAbsAsyncPrio", bootstrapSource, StringComparison.Ordinal);
Assert.DoesNotContain("DI_OpenPartition", bootstrapSource, StringComparison.Ordinal);
Assert.DoesNotContain("0x40000U", bootstrapSource, StringComparison.Ordinal);
Assert.DoesNotContain("0x2B8U", bootstrapSource, StringComparison.Ordinal);
Assert.Contains("std::string WiiSceneBootstrap::GetPackagedContentRootPath()", bootstrapSource, StringComparison.Ordinal);
Assert.Contains("return \"dvd:/\";", bootstrapSource, StringComparison.Ordinal);
Assert.DoesNotContain("return \".\";", bootstrapSource, StringComparison.Ordinal);

string discFileSystemHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiDiscFileSystem.hpp"));
string discFileSystemSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiDiscFileSystem.cpp"));
Assert.Contains("static void InitializeOpenedPartition();", discFileSystemHeaderSource, StringComparison.Ordinal);
Assert.Contains("OpenedPartitionInitialized = true;", discFileSystemSource, StringComparison.Ordinal);
Assert.DoesNotContain("PartitionDataOffset", discFileSystemHeaderSource, StringComparison.Ordinal);
Assert.DoesNotContain("PartitionDataOffset", discFileSystemSource, StringComparison.Ordinal);
Assert.Contains("const std::size_t wordOffset = currentOffset >> 2U;", discFileSystemSource, StringComparison.Ordinal);
Assert.Contains("DI_Read(alignedBuffer, static_cast<u32>(alignedLength), static_cast<u32>(wordOffset))", discFileSystemSource, StringComparison.Ordinal);
```

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj -c Release --no-restore --filter "FullyQualifiedName~PackagedBootstrap_DeclaresPackagedDiscHelpers" -v minimal
```

Expected: FAIL because the runtime still contains `DVD_Init`, `DVD_MountAsync`, `DVD_ReadAbsAsyncPrio`, `DI_OpenPartition`, and `ConfigurePartitionDataOffset` instead of `InitializeOpenedPartition`.

### Task 2: Implement DI-only packaged storage initialization

**Files:**
- Modify: `src/platform/wii/WiiSceneBootstrap.cpp`
- Modify: `src/platform/wii/WiiDiscFileSystem.hpp`
- Modify: `src/platform/wii/WiiDiscFileSystem.cpp`
- Test: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Replace the physical-drive bootstrap with the loader handoff**

Remove the `ogc/dvd.h` include, DVD callback state, raw read helpers, mount helper, partition-table constants, and `TryResolvePartitionOffsets` from `WiiSceneBootstrap.cpp`. Implement packaged startup as:

```cpp
/// Initializes access to the encrypted Wii partition already opened by the disc apploader or USB loader.
bool WiiSceneBootstrap::InitializePackagedStorage() {
    SYS_Report("[Wii] InitializePackagedStorage begin.\n");
    const int diInitResult = DI_Init();
    SYS_Report("[Wii] DI_Init result: %d\n", diInitResult);
    if (diInitResult < 0) {
        return false;
    }

    WiiDiscFileSystem::InitializeOpenedPartition();
    return true;
}
```

- [ ] **Step 2: Replace physical-offset configuration with explicit readiness**

Change the public API in `WiiDiscFileSystem.hpp` to:

```cpp
/// Initializes packaged reads for the encrypted partition already opened by the disc apploader or USB loader.
static void InitializeOpenedPartition();
```

Replace the file-scope state in `WiiDiscFileSystem.cpp` with:

```cpp
bool OpenedPartitionInitialized = false;
```

Change the read precondition and diagnostic to:

```cpp
if (destination == nullptr) {
    return false;
} else if (!OpenedPartitionInitialized) {
    SYS_Report("[Wii] Packaged reads were requested before the opened partition was initialized.\n");
    return false;
}
```

Keep `wordOffset` relative to the opened partition and remove physical `absoluteOffset` arithmetic from the read logs. Implement initialization as:

```cpp
/// Initializes packaged reads for the encrypted partition already opened by the disc apploader or USB loader.
void WiiDiscFileSystem::InitializeOpenedPartition() {
    OpenedPartitionInitialized = true;
    IndexLoaded = false;
    DiscReadLogCount = 0U;
    FileEntries.clear();
    FstBytes.clear();
    SYS_Report("[Wii] WiiDiscFileSystem initialized for the opened encrypted partition.\n");
}
```

- [ ] **Step 3: Run the focused test and verify GREEN**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj -c Release --no-restore --filter "FullyQualifiedName~PackagedBootstrap_DeclaresPackagedDiscHelpers" -v minimal
```

Expected: PASS.

- [ ] **Step 4: Commit the loader-safe storage change**

```powershell
git add -- builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiSceneBootstrap.cpp src/platform/wii/WiiDiscFileSystem.hpp src/platform/wii/WiiDiscFileSystem.cpp
git commit -m "Fix Wii loader disc handoff"
```

### Task 3: Establish the persistent runtime-failure contract

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Test: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Add a source-contract test for shutdown-aware failure presentation**

Add this test beside the existing failure-screen tests:

```csharp
/// <summary>
/// Ensures update and draw failures keep their red checkpoint visible until the user requests shutdown.
/// </summary>
[Fact]
public void FailureScreen_LatchesRuntimeFailuresUntilShutdown() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.hpp"));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));

    Assert.Contains("void PresentFailureUntilShutdown();", applicationHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void WiiApplication::PresentFailureUntilShutdown()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("while (!ShutdownRequested) {", applicationSource, StringComparison.Ordinal);

    int updateFailureIndex = applicationSource.IndexOf("if (!UpdateEngineCore()) {", StringComparison.Ordinal);
    int updateLatchIndex = applicationSource.IndexOf("PresentFailureUntilShutdown();", updateFailureIndex, StringComparison.Ordinal);
    int updateSuccessBoundaryIndex = applicationSource.IndexOf("if (!DrawEngineCore()) {", updateFailureIndex, StringComparison.Ordinal);
    Assert.True(updateFailureIndex >= 0 && updateLatchIndex > updateFailureIndex && updateLatchIndex < updateSuccessBoundaryIndex);

    int drawFailureIndex = updateSuccessBoundaryIndex;
    int drawLatchIndex = applicationSource.IndexOf("PresentFailureUntilShutdown();", drawFailureIndex, StringComparison.Ordinal);
    int framePresentIndex = applicationSource.IndexOf("PresentFrame();", drawFailureIndex, StringComparison.Ordinal);
    Assert.True(drawFailureIndex >= 0 && drawLatchIndex > drawFailureIndex && drawLatchIndex < framePresentIndex);
}
```

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj -c Release --no-restore --filter "FullyQualifiedName~FailureScreen_LatchesRuntimeFailuresUntilShutdown" -v minimal
```

Expected: FAIL because `PresentFailureUntilShutdown` does not exist and update/draw failures still return `1` after one presentation.

### Task 4: Latch all visible runtime failures

**Files:**
- Modify: `src/platform/wii/WiiApplication.hpp`
- Modify: `src/platform/wii/WiiApplication.cpp`
- Test: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Declare the shared failure presentation method**

Add this private member after `PresentFrame` in `WiiApplication.hpp`:

```cpp
/// Presents the active failure framebuffer until a registered reset or power callback requests shutdown.
void PresentFailureUntilShutdown();
```

- [ ] **Step 2: Route initialization, update, and draw failures through the method**

Use this failure flow in `Run`:

```cpp
if (!InitializeEngineCore()) {
    PresentFailureUntilShutdown();
    return 0;
}
```

```cpp
if (!UpdateEngineCore()) {
    PresentFailureUntilShutdown();
    return 0;
}

if (!DrawEngineCore()) {
    PresentFailureUntilShutdown();
    return 0;
}
```

Implement the shared loop after `PresentFrame`:

```cpp
/// Presents the active failure framebuffer until a registered reset or power callback requests shutdown.
void WiiApplication::PresentFailureUntilShutdown() {
    while (!ShutdownRequested) {
        PresentFrame();
    }
}
```

- [ ] **Step 3: Run both focused tests and verify GREEN**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj -c Release --no-restore --filter "FullyQualifiedName~PackagedBootstrap_DeclaresPackagedDiscHelpers|FullyQualifiedName~FailureScreen_LatchesRuntimeFailuresUntilShutdown" -v minimal
```

Expected: 2 passed, 0 failed.

- [ ] **Step 4: Run the complete Wii runtime source-test class**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj -c Release --no-restore --filter "FullyQualifiedName~WiiRuntimeSourceTests" -v minimal
```

Expected: the new storage and failure tests pass. Record any unrelated pre-existing fixture failures separately rather than changing their behavior.

- [ ] **Step 5: Commit the failure-latching change**

```powershell
git add -- builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiApplication.hpp src/platform/wii/WiiApplication.cpp
git commit -m "Keep Wii failure codes visible"
```

### Task 5: Build and verify one matched hardware artifact

**Files:**
- Build from: `C:/dev/helprojs/demodisc/project.heproj`
- Produce: `C:/dev/helprojs/demodisc/output/wii-loader-safe-di-20260729/game.iso`
- Produce: `C:/dev/helprojs/demodisc/output/wii-loader-safe-di-20260729/RCIE01.wbfs`

- [ ] **Step 1: Run the production Wii build through the build waiter**

```powershell
dotnet run --project C:/dev/helworks/helengine/tools/build-waiter/helengine.buildwaiter.csproj -- --output C:/dev/helprojs/demodisc/output/wii-loader-safe-di-20260729 --require game.iso -- powershell -NoProfile -ExecutionPolicy Bypass -File C:/dev/helworks/helengine/scripts/build-platform.ps1 -Project C:/dev/helprojs/demodisc/project.heproj -Platform wii -Output C:/dev/helprojs/demodisc/output/wii-loader-safe-di-20260729
```

Expected: build waiter reports all required artifacts fresh and non-empty, and `wii-build-phase.txt` records completion through packaged output verification.

- [ ] **Step 2: Verify the ISO and WBFS structures**

```powershell
$wit = 'C:/dev/helworks/helengine-wii/tmp/tools/wit-v3.05a-r8638-cygwin64/bin/wit.exe'
& $wit verify --verbose C:/dev/helprojs/demodisc/output/wii-loader-safe-di-20260729/game.iso
& $wit verify --verbose C:/dev/helprojs/demodisc/output/wii-loader-safe-di-20260729/RCIE01.wbfs
& $wit dump --long C:/dev/helprojs/demodisc/output/wii-loader-safe-di-20260729/game.iso | Select-String -Pattern 'System version|Partition IDs|main.dol|FST'
```

Expected: both images report `+OK`, one encrypted DATA partition, ID `RCIE01`, and IOS56.

- [ ] **Step 3: Confirm the packaged DOL exactly matches the native build**

```powershell
Get-FileHash -Algorithm SHA256 C:/dev/helprojs/demodisc/output/wii-loader-safe-di-20260729/native/helengine_wii.dol,C:/dev/helworks/helengine-wii/build/helengine_wii.dol
```

Expected: identical SHA-256 hashes.

- [ ] **Step 4: Report the hardware test artifact**

Use `RCIE01.wbfs` directly with USB Loader GX. Do not alter per-game settings. Expected hardware behavior is authored rendering, or a persistent red four-digit checkpoint that can be exited with reset or power rather than a black hard lock.
