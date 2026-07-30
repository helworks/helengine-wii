# Wii Direct Disc Interface Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace libdi's AHBPROT-gated false-success path with an engine-owned, cIOS-compatible `/dev/di` interface so USB Loader GX can read packaged scene content without user settings.

**Architecture:** A new `WiiDiscInterface` owns the title-local IOS descriptor and submits encrypted partition reads using the same ioctl `0x71` command contract as USB Loader GX. `WiiSceneBootstrap` initializes that interface, while `WiiDiscFileSystem` retains FST lookup, aligned chunking, and stream construction and delegates only the low-level read. Generated runtime code and packaged asset formats remain unchanged.

**Tech Stack:** C++20, devkitPPC/libogc IOS APIs, C#/.NET 9, xUnit source-contract tests, Docker Wii toolchain, Wiimms ISO Tools, Dolphin.

## Global Constraints

- Do not require AHBPROT or any per-game USB Loader GX setting.
- Do not reload IOS, select a cIOS slot, reset the drive, change WBFS mode, or reopen the encrypted partition.
- Do not patch, fork, or post-process libogc/libdi.
- Do not edit generated C++ output; change only the Wii platform source and its code-generation/build inputs.
- Preserve the apploader FST handoff, partition-relative offsets, 32-byte read alignment, existing exception behavior, and visible failure checkpoints.
- Write and observe failing source-contract tests before changing production code.
- Preserve the existing untracked `tmp_wii_audio_contract_test.txt` and `tmp_wii_audio_test.txt` files.

---

## File Structure

- Create `src/platform/wii/WiiDiscInterface.hpp`: declare the isolated IOS descriptor owner and encrypted partition read API.
- Create `src/platform/wii/WiiDiscInterface.cpp`: open `/dev/di`, validate aligned requests, build ioctl `0x71` commands, and return the raw IOS result.
- Modify `src/platform/wii/WiiSceneBootstrap.cpp`: initialize `WiiDiscInterface` instead of libdi `DI_Init()`.
- Modify `src/platform/wii/WiiDiscFileSystem.cpp`: delegate aligned partition reads to `WiiDiscInterface` instead of `DI_Read()`.
- Modify `Makefile`: compile the new implementation and remove the unused `-ldi` dependency.
- Modify `builder.tests/WiiRuntimeSourceTests.cs`: lock the direct IOS contract, bootstrap ownership, filesystem delegation, and build dependency.

### Task 1: Implement the direct IOS disc interface

**Files:**
- Create: `src/platform/wii/WiiDiscInterface.hpp`
- Create: `src/platform/wii/WiiDiscInterface.cpp`
- Modify: `src/platform/wii/WiiSceneBootstrap.cpp`
- Modify: `src/platform/wii/WiiDiscFileSystem.cpp`
- Modify: `Makefile`
- Test: `builder.tests/WiiRuntimeSourceTests.cs`

**Interfaces:**
- Consumes: the loader-open encrypted partition, apploader FST low-memory handoff, and libogc `IOS_Open`/`IOS_Ioctl` APIs.
- Produces: `WiiDiscInterface::Initialize()` and `WiiDiscInterface::ReadEncryptedPartition(void*, std::size_t, std::size_t)`.

- [ ] **Step 1: Add the failing direct-IOS source contract**

Add this test beside the existing packaged-disc filesystem tests in `WiiRuntimeSourceTests.cs`:

```csharp
/// <summary>
/// Ensures packaged Wii startup opens the cIOS disc device directly instead of accepting libdi's AHBPROT-gated false-success result.
/// </summary>
[Fact]
public void PackagedDiscInterface_UsesDirectIosDeviceWithoutAhbprotGate() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string interfaceHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiDiscInterface.hpp"));
    string interfaceSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiDiscInterface.cpp"));
    string bootstrapSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiSceneBootstrap.cpp"));
    string makefileSource = File.ReadAllText(Path.Combine(repositoryRootPath, "Makefile"));

    Assert.Contains("class WiiDiscInterface", interfaceHeaderSource, StringComparison.Ordinal);
    Assert.Contains("static bool Initialize();", interfaceHeaderSource, StringComparison.Ordinal);
    Assert.Contains("static int ReadEncryptedPartition(void* destination, std::size_t length, std::size_t partitionRelativeOffset);", interfaceHeaderSource, StringComparison.Ordinal);
    Assert.Contains("static constexpr uint32_t IoctlDiRead = 0x71U;", interfaceSource, StringComparison.Ordinal);
    Assert.Contains("IOS_Open(DevicePath, 0)", interfaceSource, StringComparison.Ordinal);
    Assert.Contains("CommandBuffer[0] = IoctlDiRead << 24U;", interfaceSource, StringComparison.Ordinal);
    Assert.Contains("CommandBuffer[1] = static_cast<uint32_t>(length);", interfaceSource, StringComparison.Ordinal);
    Assert.Contains("CommandBuffer[2] = static_cast<uint32_t>(partitionRelativeOffset >> 2U);", interfaceSource, StringComparison.Ordinal);
    Assert.Contains("if (FileDescriptor < 0) {", interfaceSource, StringComparison.Ordinal);
    Assert.Contains("return -ENXIO;", interfaceSource, StringComparison.Ordinal);
    Assert.Contains("(reinterpret_cast<uintptr_t>(destination) & 0x1FU) != 0U", interfaceSource, StringComparison.Ordinal);
    Assert.Contains("(length & 0x1FU) != 0U", interfaceSource, StringComparison.Ordinal);
    Assert.Contains("(partitionRelativeOffset & 0x3U) != 0U", interfaceSource, StringComparison.Ordinal);
    Assert.Contains("return -EINVAL;", interfaceSource, StringComparison.Ordinal);
    Assert.Contains("return IOS_Ioctl(", interfaceSource, StringComparison.Ordinal);
    Assert.Contains("WiiDiscInterface::Initialize()", bootstrapSource, StringComparison.Ordinal);
    Assert.DoesNotContain("DI_Init()", bootstrapSource, StringComparison.Ordinal);
    Assert.DoesNotContain("DI_OpenPartition", interfaceSource, StringComparison.Ordinal);
    Assert.Contains("$(SOURCE_DIR)/platform/wii/WiiDiscInterface.cpp", makefileSource, StringComparison.Ordinal);
    Assert.DoesNotContain("\t-ldi", makefileSource, StringComparison.Ordinal);
    Assert.DoesNotContain("AHBPROT", interfaceSource, StringComparison.OrdinalIgnoreCase);
}
```

Update `PackagedDiscFileSystem_UsesPartitionDataRelativeDecryptedDiReads` by replacing its `DI_Read` assertion with:

```csharp
Assert.Contains("const int readResult = WiiDiscInterface::ReadEncryptedPartition(alignedBuffer, alignedLength, currentOffset);", source, StringComparison.Ordinal);
Assert.Contains("if (readResult != 1) {", source, StringComparison.Ordinal);
Assert.DoesNotContain("DI_Read(", source, StringComparison.Ordinal);
```

- [ ] **Step 2: Run the focused tests and verify RED**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj -p:HelEngineRoot=C:\dev\helworks\helengine --filter "FullyQualifiedName~PackagedDiscInterface_UsesDirectIosDeviceWithoutAhbprotGate|FullyQualifiedName~PackagedDiscFileSystem_UsesPartitionDataRelativeDecryptedDiReads" -v minimal
```

Expected: `PackagedDiscInterface_UsesDirectIosDeviceWithoutAhbprotGate` fails because the new files do not exist, and the filesystem contract fails because it still calls `DI_Read`.

- [ ] **Step 3: Create the interface header**

Create `WiiDiscInterface.hpp` with this API and substantive member comments:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>

namespace helengine::wii {
    /// Owns the title-local IOS disc descriptor used for encrypted reads from the partition opened by the loader.
    class WiiDiscInterface {
    public:
        /// Opens the cIOS disc device without requiring AHBPROT and returns whether a usable descriptor was acquired.
        static bool Initialize();

        /// Submits one aligned encrypted read relative to the partition already opened by the loader and returns the raw IOS result.
        static int ReadEncryptedPartition(void* destination, std::size_t length, std::size_t partitionRelativeOffset);

    private:
        /// Title-local IOS descriptor for the loader-configured disc device.
        static int FileDescriptor;

        /// Aligned IOS device path retained for descriptor initialization.
        alignas(32) static char DevicePath[8];

        /// Aligned eight-word request buffer required by the `/dev/di` ioctl ABI.
        alignas(32) static uint32_t CommandBuffer[8];
    };
}
```

- [ ] **Step 4: Implement direct initialization and encrypted reads**

Create `WiiDiscInterface.cpp`:

```cpp
#include "platform/wii/WiiDiscInterface.hpp"

#include <cerrno>
#include <cstdint>
#include <cstring>

#include <ogc/ipc.h>

namespace helengine::wii {
    namespace {
        static constexpr uint32_t IoctlDiRead = 0x71U;
    }

    int WiiDiscInterface::FileDescriptor = -1;
    alignas(32) char WiiDiscInterface::DevicePath[8] = "/dev/di";
    alignas(32) uint32_t WiiDiscInterface::CommandBuffer[8] = {};

    /// Opens the cIOS disc device without requiring AHBPROT and returns whether a usable descriptor was acquired.
    bool WiiDiscInterface::Initialize() {
        if (FileDescriptor >= 0) {
            return true;
        }

        FileDescriptor = IOS_Open(DevicePath, 0);
        return FileDescriptor >= 0;
    }

    /// Submits one aligned encrypted read relative to the partition already opened by the loader and returns the raw IOS result.
    int WiiDiscInterface::ReadEncryptedPartition(void* destination, std::size_t length, std::size_t partitionRelativeOffset) {
        if (FileDescriptor < 0) {
            return -ENXIO;
        } else if (destination == nullptr
            || length == 0U
            || (reinterpret_cast<uintptr_t>(destination) & 0x1FU) != 0U
            || (length & 0x1FU) != 0U
            || (partitionRelativeOffset & 0x3U) != 0U) {
            return -EINVAL;
        }

        std::memset(CommandBuffer, 0, sizeof(CommandBuffer));
        CommandBuffer[0] = IoctlDiRead << 24U;
        CommandBuffer[1] = static_cast<uint32_t>(length);
        CommandBuffer[2] = static_cast<uint32_t>(partitionRelativeOffset >> 2U);
        return IOS_Ioctl(
            FileDescriptor,
            IoctlDiRead,
            CommandBuffer,
            sizeof(CommandBuffer),
            destination,
            static_cast<uint32_t>(length));
    }
}
```

- [ ] **Step 5: Route bootstrap and filesystem reads through the interface**

In `WiiSceneBootstrap.cpp`, remove `<di/di.h>`, include `WiiDiscInterface.hpp`, and replace `DI_Init()` handling with:

```cpp
/// Initializes access to the encrypted Wii partition already opened by the disc apploader or USB loader.
bool WiiSceneBootstrap::InitializePackagedStorage() {
    SYS_Report("[Wii] InitializePackagedStorage begin.\n");
    if (!WiiDiscInterface::Initialize()) {
        SYS_Report("[Wii] Could not open the cIOS disc device.\n");
        return false;
    }

    WiiDiscFileSystem::InitializeOpenedPartition();
    return true;
}
```

In `WiiDiscFileSystem.cpp`, remove `<di/di.h>`, include `WiiDiscInterface.hpp`, and replace the low-level read call with:

```cpp
const int readResult = WiiDiscInterface::ReadEncryptedPartition(alignedBuffer, alignedLength, currentOffset);
if (readResult != 1) {
```

Keep `wordOffset` only for diagnostics, preserve the existing aligned allocation and `memcpy`, and continue returning false on a failed read.

- [ ] **Step 6: Update the native build inputs**

Add the new source beside `WiiDiscFileSystem.cpp` in `GENERATED_BRIDGE_SOURCES`:

```make
	$(SOURCE_DIR)/platform/wii/WiiDiscInterface.cpp \
```

Remove this now-unused entry from `LDLIBS`:

```make
	-ldi \
```

- [ ] **Step 7: Run the focused tests and verify GREEN**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj -p:HelEngineRoot=C:\dev\helworks\helengine --filter "FullyQualifiedName~PackagedDiscInterface_UsesDirectIosDeviceWithoutAhbprotGate|FullyQualifiedName~PackagedDiscFileSystem_UsesPartitionDataRelativeDecryptedDiReads|FullyQualifiedName~PackagedDiscFileSystem_UsesApploaderLoadedFstFromLowMemory|FullyQualifiedName~PackagedDiscFileSystem_ConvertsFileEntryOffsetsFromQuarterWordsToBytes" -v minimal
```

Expected: 4 passed, 0 failed.

- [ ] **Step 8: Run the complete Wii runtime source-test class**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj -p:HelEngineRoot=C:\dev\helworks\helengine --filter "FullyQualifiedName~WiiRuntimeSourceTests" -v minimal
```

Expected: all `WiiRuntimeSourceTests` pass. Do not change unrelated behavior to hide a pre-existing failure.

- [ ] **Step 9: Verify the native build compiles without libdi**

Run the packaged native build against the last generated core:

```powershell
docker run --rm -v C:\dev\helworks\helengine-wii:/workspace -v C:\dev\helworks\b\wii\d4f691b4c25b47c88b387681b5101a05\generated-core:/helengine-generated-core -w /workspace -e HELENGINE_CORE_CPP_ROOT=/helengine-generated-core -e HELENGINE_WII_BOOT_MODE=packaged-disc helengine-wii sh -lc "set -e; make clean && make"
```

Expected: `build/helengine_wii.dol` and `build/helengine_wii_apploader_template.bin` are produced with no undefined IOS symbols and the link map contains no `DI_Init` or `DI_Read` reference.

- [ ] **Step 10: Commit the root fix**

```powershell
git add -- builder.tests/WiiRuntimeSourceTests.cs Makefile src/platform/wii/WiiDiscInterface.hpp src/platform/wii/WiiDiscInterface.cpp src/platform/wii/WiiSceneBootstrap.cpp src/platform/wii/WiiDiscFileSystem.cpp
git commit -m "Use direct cIOS reads for Wii content"
```

### Task 2: Build and verify fresh Wii artifacts

**Files:**
- Build from: `C:/dev/helprojs/demodisc/project.heproj`
- Produce: `C:/dev/helprojs/demodisc/output/wii-direct-di-20260729/game.iso`
- Produce: `C:/dev/helprojs/demodisc/output/wii-direct-di-20260729/RCIE01.wbfs`
- Produce: `C:/dev/helprojs/demodisc/output/wii-direct-di-20260729/native/helengine_wii.dol`

**Interfaces:**
- Consumes: the committed direct `/dev/di` implementation and Demo Disc project build inputs.
- Produces: one matched ISO/WBFS/DOL artifact set suitable for Dolphin and hardware testing.

- [ ] **Step 1: Run the production Wii build through the build waiter**

```powershell
dotnet run --project C:\dev\helworks\helengine\tools\build-waiter\helengine.buildwaiter.csproj -- --output C:\dev\helprojs\demodisc\output\wii-direct-di-20260729 --require game.iso --require RCIE01.wbfs --require native\helengine_wii.dol -- powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\scripts\build-platform.ps1 -Project C:\dev\helprojs\demodisc\project.heproj -Platform wii -Output C:\dev\helprojs\demodisc\output\wii-direct-di-20260729
```

Expected: the waiter reports fresh, non-empty artifacts and `wii-build-phase.txt` ends with packaged outputs verified.

- [ ] **Step 2: Verify ISO and WBFS structures**

```powershell
$witPath = 'C:\dev\helworks\helengine-wii\tmp\tools\wit-v3.05a-r8638-cygwin64\bin\wit.exe'
& $witPath verify --verbose C:\dev\helprojs\demodisc\output\wii-direct-di-20260729\game.iso
& $witPath verify --verbose C:\dev\helprojs\demodisc\output\wii-direct-di-20260729\RCIE01.wbfs
& $witPath dump --long C:\dev\helprojs\demodisc\output\wii-direct-di-20260729\game.iso | Select-String -Pattern 'System version|Partition|main.dol|FST'
```

Expected: both images report `+OK`, ID `RCIE01`, IOS56, and one encrypted DATA partition.

- [ ] **Step 3: Record artifact hashes and embedded implementation evidence**

```powershell
Get-FileHash -Algorithm SHA256 C:\dev\helprojs\demodisc\output\wii-direct-di-20260729\game.iso,C:\dev\helprojs\demodisc\output\wii-direct-di-20260729\RCIE01.wbfs,C:\dev\helprojs\demodisc\output\wii-direct-di-20260729\native\helengine_wii.dol
rg -a -l "/dev/di" C:\dev\helprojs\demodisc\output\wii-direct-di-20260729\native\helengine_wii.dol
```

Expected: all three hashes are recorded and the DOL contains `/dev/di`.

- [ ] **Step 4: Launch the fresh ISO in Dolphin**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-wii\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\demodisc\output\wii-direct-di-20260729\game.iso
```

Expected: Dolphin starts the exact fresh ISO and remains responsive. Do not take screenshots.

- [ ] **Step 5: Inspect the fresh runtime trace**

Read:

```text
C:\dev\helworks\helengine-wii\tmp\dolphin-launcher-user\Wii\title\00010000\52434945\data\runtime_registry_trace.txt
```

Expected: content root `dvd:/`, startup scene queued and committed, cooked assets loaded, and repeated render frames with submitted glyphs.

- [ ] **Step 6: Confirm repository cleanliness**

```powershell
git status --short
```

Expected: only the two pre-existing untracked audio test files remain.

- [ ] **Step 7: Hand off the hardware artifact**

Use `C:\dev\helprojs\demodisc\output\wii-direct-di-20260729\RCIE01.wbfs` in USB Loader GX without changing per-game settings. The decisive result is progression beyond `C113`; successful authored rendering completes the fix, while a later visible code identifies an independent next fault.
