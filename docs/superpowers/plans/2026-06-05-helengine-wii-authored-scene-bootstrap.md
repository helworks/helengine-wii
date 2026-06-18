# Helengine Wii Authored Scene Bootstrap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire real authored-scene bootstrap inputs into the Wii generated-core initialization path so the host uses a validated content root and runtime scene catalog instead of placeholder values.

**Architecture:** Mirror the GameCube scene-bootstrap shape inside a Wii-owned `WiiSceneBootstrap` boundary, then feed its validated content-root and scene-catalog outputs into `WiiApplication::InitializeEngineCore()`. Keep the renderer and input bridges unchanged, fail fast when staged content is missing, and treat visible authored rendering as a later milestone.

**Tech Stack:** C++20, devkitPPC, libogc Wii rules, GNU Make, Docker, Dolphin

---

## File Structure

- Modify: `Makefile`
  Purpose: compile `WiiSceneBootstrap.cpp` only when generated core is enabled.
- Modify: `src/platform/wii/WiiBootPhase.hpp`
  Purpose: add an explicit scene-bootstrap diagnostic phase between core options and bridge construction.
- Modify: `src/platform/wii/WiiApplication.cpp`
  Purpose: replace placeholder scene bootstrap values with `WiiSceneBootstrap` calls and emit bootstrap diagnostics.
- Create: `src/platform/wii/WiiSceneBootstrap.hpp`
  Purpose: declare the Wii staged-content root, startup-scene metadata, validation helpers, and runtime scene-catalog factory.
- Create: `src/platform/wii/WiiSceneBootstrap.cpp`
  Purpose: implement staged-content root resolution, required-file validation, and single-scene runtime catalog creation.
- Modify: `README.md`
  Purpose: document staged authored-scene content expectations, missing-content verification, and the successful Dolphin verification flow.

### Task 1: Write The Failing Wii Scene-Bootstrap Wiring

**Files:**
- Modify: `src/platform/wii/WiiBootPhase.hpp`
- Modify: `src/platform/wii/WiiApplication.cpp`

- [ ] **Step 1: Add a dedicated scene-bootstrap boot phase**

Insert this enum member in `WiiBootPhase` between `CoreOptions` and `BridgeConstruction`:

```cpp
        /// Authored-scene bootstrap data is being resolved and validated.
        SceneBootstrap,
```

- [ ] **Step 2: Replace placeholder scene bootstrap values in `WiiApplication.cpp`**

Add this include with the other generated-core includes:

```cpp
#include "platform/wii/WiiSceneBootstrap.hpp"
```

Then replace the current placeholder bootstrap block inside `WiiApplication::InitializeEngineCore()`:

```cpp
            options->ContentRootPath = ".";
            options->SceneCatalog = nullptr;
            options->UpdateOrderLayers = 4;
```

with this exact block:

```cpp
            initializationStage = "ResolveSceneBootstrap";
            SetBootPhase(WiiBootPhase::SceneBootstrap, GXColor { 0x40, 0x80, 0xFF, 0xFF });
            const std::string contentRootPath = WiiSceneBootstrap::GetValidatedContentRootPath();
            const std::string startupSceneId = WiiSceneBootstrap::GetStartupSceneId();
            SYS_Report("[Wii] Staged content root: %s\n", contentRootPath.c_str());
            SYS_Report("[Wii] Startup scene id: %s\n", startupSceneId.c_str());
            options->ContentRootPath = contentRootPath;
            options->SceneCatalog = WiiSceneBootstrap::CreateSceneCatalog();
            options->UpdateOrderLayers = 4;
```

Leave the startup scene id as a logged bootstrap artifact only in this slice. Do not add `LoadScene(...)` yet.

- [ ] **Step 3: Run the generated-core Docker build to verify the wiring fails**

Run:

```bash
rtk proxy docker run --rm -v C:/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii helengine-wii make clean all
```

Expected: the build fails because `platform/wii/WiiSceneBootstrap.hpp` does not exist yet or `WiiBootPhase::SceneBootstrap` is not fully wired.

### Task 2: Implement `WiiSceneBootstrap` And Restore A Green Build

**Files:**
- Modify: `Makefile`
- Create: `src/platform/wii/WiiSceneBootstrap.hpp`
- Create: `src/platform/wii/WiiSceneBootstrap.cpp`

- [ ] **Step 1: Add `WiiSceneBootstrap.cpp` to the generated-core source list**

Extend `GENERATED_BRIDGE_SOURCES` in `Makefile` so it reads:

```makefile
GENERATED_BRIDGE_SOURCES := \
	$(SOURCE_DIR)/platform/wii/WiiInputManager.cpp \
	$(SOURCE_DIR)/platform/wii/WiiRenderManager2D.cpp \
	$(SOURCE_DIR)/platform/wii/WiiRenderManager3D.cpp \
	$(SOURCE_DIR)/platform/wii/WiiSceneBootstrap.cpp
```

- [ ] **Step 2: Create `src/platform/wii/WiiSceneBootstrap.hpp`**

Create this file:

```cpp
#pragma once

#include "runtime/native_string.hpp"

class RuntimeSceneCatalog;

namespace helengine::wii {
    /// Declares the authored startup scene and content-root helpers used by the direct-DOL Wii bootstrap flow.
    class WiiSceneBootstrap {
    public:
        /// Relative repo path that must contain the staged cooked scene bundle before Dolphin verification.
        static std::string BundledContentRootPath;

        /// Absolute Windows host path used when Dolphin does not launch with the repo root as its working directory.
        static std::string BundledContentRootWindowsHostPath;

        /// Absolute WSL path used for local validation in the shared workspace.
        static std::string BundledContentRootWslPath;

        /// Stable scene id expected by the generated runtime scene catalog.
        static std::string StartupSceneId;

        /// Returns the staged content root and fails if the bundle has not been prepared.
        static std::string GetValidatedContentRootPath();

        /// Creates the single-scene runtime catalog used by the current Wii authored-scene bootstrap milestone.
        static RuntimeSceneCatalog* CreateSceneCatalog();

        /// Returns the authored startup scene id used by the staged runtime scene catalog.
        static std::string GetStartupSceneId();

    private:
        /// Returns whether all required staged files exist under the candidate content root.
        static bool HasRequiredFiles(std::string rootPath);

        /// Verifies one required staged content file exists under the bundle root.
        static void ValidateRequiredFile(std::string rootPath, std::string relativePath);
    };
}
```

- [ ] **Step 3: Create `src/platform/wii/WiiSceneBootstrap.cpp`**

Create this file:

```cpp
#include "platform/wii/WiiSceneBootstrap.hpp"

#include "RuntimeSceneCatalog.hpp"
#include "RuntimeSceneCatalogEntry.hpp"
#include "runtime/array.hpp"
#include "runtime/native_exceptions.hpp"
#include "system/io/file.hpp"
#include "system/io/path.hpp"

namespace helengine::wii {
    std::string WiiSceneBootstrap::BundledContentRootPath = "tmp/city-demo-disc-main-menu-content";

    std::string WiiSceneBootstrap::BundledContentRootWindowsHostPath = "C:/dev/helworks/helengine-wii/tmp/city-demo-disc-main-menu-content";

    std::string WiiSceneBootstrap::BundledContentRootWslPath = "/mnt/c/dev/helworks/helengine-wii/tmp/city-demo-disc-main-menu-content";

    std::string WiiSceneBootstrap::StartupSceneId = "Scenes/DemoDiscMainMenu.helen";

    /// Returns the staged content root and fails if the bundle has not been prepared.
    std::string WiiSceneBootstrap::GetValidatedContentRootPath() {
        const std::string relativeRootPath = Path::GetFullPath(BundledContentRootPath);
        if (HasRequiredFiles(relativeRootPath)) {
            return relativeRootPath;
        }

        const std::string windowsHostRootPath = Path::GetFullPath(BundledContentRootWindowsHostPath);
        if (HasRequiredFiles(windowsHostRootPath)) {
            return windowsHostRootPath;
        }

        const std::string wslRootPath = Path::GetFullPath(BundledContentRootWslPath);
        if (HasRequiredFiles(wslRootPath)) {
            return wslRootPath;
        }

        ValidateRequiredFile(relativeRootPath, "cooked/scenes/DemoDiscMainMenu.hasset");
        return relativeRootPath;
    }

    /// Creates the single-scene runtime catalog used by the current Wii authored-scene bootstrap milestone.
    RuntimeSceneCatalog* WiiSceneBootstrap::CreateSceneCatalog() {
        Array<RuntimeSceneCatalogEntry*>* entries = new Array<RuntimeSceneCatalogEntry*>(1);
        (*entries)[0] = new RuntimeSceneCatalogEntry(StartupSceneId, "cooked/scenes/DemoDiscMainMenu.hasset");
        return new RuntimeSceneCatalog(entries);
    }

    /// Returns the authored startup scene id used by the staged runtime scene catalog.
    std::string WiiSceneBootstrap::GetStartupSceneId() {
        return StartupSceneId;
    }

    /// Returns whether all required staged files exist under the candidate content root.
    bool WiiSceneBootstrap::HasRequiredFiles(std::string rootPath) {
        if (String::IsNullOrWhiteSpace(rootPath)) {
            return false;
        }

        return File::Exists(Path::GetFullPath(Path::Combine(rootPath, "cooked/scenes/DemoDiscMainMenu.hasset")));
    }

    /// Verifies one required staged content file exists under the bundle root.
    void WiiSceneBootstrap::ValidateRequiredFile(std::string rootPath, std::string relativePath) {
        const std::string fullPath = Path::GetFullPath(Path::Combine(rootPath, relativePath));
        if (!File::Exists(fullPath)) {
            throw new InvalidOperationException(std::string("Required staged Wii content file is missing: ") + fullPath);
        }
    }
}
```

- [ ] **Step 4: Run the generated-core Docker build to verify it passes**

Run:

```bash
rtk proxy docker run --rm -v C:/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii helengine-wii make clean all
```

Expected: `build/helengine_wii.dol` is emitted successfully.

- [ ] **Step 5: Commit the bootstrap implementation**

```bash
rtk git add Makefile src/platform/wii/WiiBootPhase.hpp src/platform/wii/WiiApplication.cpp src/platform/wii/WiiSceneBootstrap.hpp src/platform/wii/WiiSceneBootstrap.cpp
rtk git commit -m "Add Wii authored scene bootstrap plumbing"
```

### Task 3: Verify The Missing-Content Failure Path In Dolphin

**Files:**
- No code changes. Runtime verification only.

- [ ] **Step 1: Ensure the staged content bundle is absent for the failure check**

If `tmp/city-demo-disc-main-menu-content` already exists, temporarily move it aside:

```bash
rtk proxy powershell.exe -NoProfile -Command "if (Test-Path 'C:\dev\helworks\helengine-wii\tmp\city-demo-disc-main-menu-content') { Move-Item 'C:\dev\helworks\helengine-wii\tmp\city-demo-disc-main-menu-content' 'C:\dev\helworks\helengine-wii\tmp\city-demo-disc-main-menu-content.off' -Force }"
```

Expected: the Wii repo no longer has the staged authored-scene bundle at the mirrored path.

- [ ] **Step 2: Launch visible Dolphin with the generated-core Wii `.dol`**

Run:

```bash
rtk proxy powershell.exe -NoProfile -Command "Start-Process -FilePath 'C:\dev\helworks\emus\dolphin-2603a-x64\Dolphin-x64\Dolphin.exe' -ArgumentList '-e','C:\dev\helworks\helengine-wii\build\helengine_wii.dol'"
```

Expected Dolphin log lines:

```text
[Wii] Engine core initialization threw Exception stage=ResolveSceneBootstrap message=Required staged Wii content file is missing:
```

Expected screen result: the runtime stays on the failure diagnostic color instead of silently proceeding with placeholder bootstrap data.

- [ ] **Step 3: Restore the content bundle location if you moved it**

Run:

```bash
rtk proxy powershell.exe -NoProfile -Command "if (Test-Path 'C:\dev\helworks\helengine-wii\tmp\city-demo-disc-main-menu-content.off') { Move-Item 'C:\dev\helworks\helengine-wii\tmp\city-demo-disc-main-menu-content.off' 'C:\dev\helworks\helengine-wii\tmp\city-demo-disc-main-menu-content' -Force }"
```

Expected: the original staged-content path is restored for the success-path verification.

### Task 4: Stage A Mirrored Bundle And Verify The Success Path

**Files:**
- No code changes. Runtime verification only.

- [ ] **Step 1: Mirror the staged content bundle into the Wii repo**

If the sibling GameCube repo already has the staged content root, copy it into the Wii repo:

```bash
rtk proxy powershell.exe -NoProfile -Command "Copy-Item 'C:\dev\helworks\helengine-gc\tmp\city-demo-disc-main-menu-content' 'C:\dev\helworks\helengine-wii\tmp\city-demo-disc-main-menu-content' -Recurse -Force"
```

Expected: `C:\dev\helworks\helengine-wii\tmp\city-demo-disc-main-menu-content\cooked\scenes\DemoDiscMainMenu.hasset` exists.

- [ ] **Step 2: Relaunch visible Dolphin with the Wii `.dol`**

Run:

```bash
rtk proxy powershell.exe -NoProfile -Command "Start-Process -FilePath 'C:\dev\helworks\emus\dolphin-2603a-x64\Dolphin-x64\Dolphin.exe' -ArgumentList '-e','C:\dev\helworks\helengine-wii\build\helengine_wii.dol'"
```

Expected Dolphin log lines:

```text
[Wii] Staged content root: C:/dev/helworks/helengine-wii/tmp/city-demo-disc-main-menu-content
[Wii] Startup scene id: Scenes/DemoDiscMainMenu.helen
[Wii] Engine core initialized.
```

Expected screen result: the runtime remains alive on the current diagnostic running color. Visible authored-scene rendering is not required in this milestone.

- [ ] **Step 3: Capture the resulting artifact timestamp**

Run:

```bash
rtk proxy powershell.exe -NoProfile -Command "Get-Item 'C:\dev\helworks\helengine-wii\build\helengine_wii.dol' | Format-List FullName,Length,LastWriteTimeUtc"
```

Expected: a fresh `LastWriteTimeUtc` for the verified bootstrap artifact.

### Task 5: Document The Authored Scene Bootstrap Workflow

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update the milestone bullets**

Replace the current milestone list:

```markdown
- Docker-only build using devkitPro Wii tooling
- Native `.dol` output for direct loading in Dolphin
- Host-only pink-frame bootstrap when `HELENGINE_CORE_CPP_ROOT` is unset
- Generated-core build that initializes the Wii runtime host and emits a `.dol`
```

with:

```markdown
- Docker-only build using devkitPro Wii tooling
- Native `.dol` output for direct loading in Dolphin
- Host-only pink-frame bootstrap when `HELENGINE_CORE_CPP_ROOT` is unset
- Generated-core build that initializes the Wii runtime host and emits a `.dol`
- Authored-scene bootstrap plumbing that validates a staged content root and runtime scene catalog before core initialization
```

- [ ] **Step 2: Add a staged content section after `## Generated core build`**

Insert this section:

````markdown
## Authored scene bootstrap content

The current Wii authored-scene bootstrap milestone expects a staged content root at:

- `tmp/city-demo-disc-main-menu-content`

The minimum required authored-scene file for this milestone is:

- `tmp/city-demo-disc-main-menu-content/cooked/scenes/DemoDiscMainMenu.hasset`

If the sibling GameCube repo already has the staged bundle, mirror it into the Wii repo:

```bash
rtk proxy powershell.exe -NoProfile -Command "Copy-Item 'C:\dev\helworks\helengine-gc\tmp\city-demo-disc-main-menu-content' 'C:\dev\helworks\helengine-wii\tmp\city-demo-disc-main-menu-content' -Recurse -Force"
```
````

- [ ] **Step 3: Replace the generated-core verification section**

Replace the current `## Generated core verification` section with:

````markdown
## Generated core verification

Build the Wii player with generated core enabled:

```bash
rtk docker run --rm -v C:/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii helengine-wii make clean all
```

Failure-path verification:

- launch `build/helengine_wii.dol` in Dolphin without the staged content bundle present
- expect a `ResolveSceneBootstrap` failure log and a persistent failure diagnostic frame

Success-path verification:

- mirror the staged content bundle into `tmp/city-demo-disc-main-menu-content`
- launch `build/helengine_wii.dol` in Dolphin
- expect staged content root logging, startup scene id logging, `Engine core initialized.`, and a stable running frame
````

- [ ] **Step 4: Commit the documentation update**

```bash
rtk git add README.md
rtk git commit -m "Document Wii authored scene bootstrap verification"
```
