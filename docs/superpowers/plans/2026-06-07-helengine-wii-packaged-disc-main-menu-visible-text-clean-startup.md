# Helengine Wii Packaged-Disc Main Menu Visible Text Clean-Startup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the packaged-disc Wii build reach the authored main menu in Dolphin with visible authored text, no Dolphin startup warning dialog, and GPU-only cooked-font rendering.

**Architecture:** Treat this as one packaged-disc-only lane with a strict clean-start gate. First stabilize one repeatable Dolphin launch and host-log capture path, then add temporary host-visible runtime diagnostics for the text path, then apply the smallest renderer/bootstrap fix needed to make authored menu text visible, and finally remove or gate the temporary debug output.

**Tech Stack:** PowerShell helper scripts, Dolphin, Wii native C++ runtime, xUnit source-audit tests, packaged-disc ISO patching and launch workflow.

---

## File Map

- Create: `tmp/launch_wii_packaged_debug_session.ps1`
  - One repeatable packaged-disc launcher that force-closes Dolphin, seeds a known logging profile, launches the target ISO, and records host-visible outputs for investigation runs.
- Modify: `tmp/inspect_dolphin_startup_ui.ps1`
  - Reuse existing startup inspection infrastructure only if needed to support the packaged debug session without duplicating window-capture logic.
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
  - Add or tighten source-audit coverage for the packaged-disc runtime diagnostics and final renderer contract.
- Modify: `src/platform/wii/WiiApplication.cpp`
  - Emit temporary packaged-disc runtime diagnostics through host-visible logging for startup scene id, font resolution, and text-path state.
- Modify: `src/platform/wii/WiiRenderManager2D.hpp`
  - Keep only the small temporary state needed to expose camera visitation, drawable visitation, queued text count, and glyph submission state while debugging.
- Modify: `src/platform/wii/WiiRenderManager2D.cpp`
  - Add temporary packaged-disc diagnostics, preserve GPU-only glyph rendering, and then remove or gate temporary markers in the final success state.
- Modify: `src/platform/wii/WiiSceneBootstrap.cpp`
  - Fix any packaged-disc startup metadata or filesystem behavior needed to eliminate the Dolphin startup warning if the root cause lands here.
- Modify: `src/platform/wii/WiiDiscFileSystem.cpp`
  - Fix packaged-disc file bridge behavior only if the startup warning or text-path failure is traced to this layer.
- Modify: `README.md`
  - Update the Wii Dolphin/debug workflow only if the chosen packaged debug launcher becomes part of the repo’s intended day-to-day investigation path.

### Task 1: Stabilize One Packaged-Disc Launch and Host-Log Capture Path

**Files:**
- Create: `tmp/launch_wii_packaged_debug_session.ps1`
- Modify: `README.md`
- Test: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Write the failing source-audit test for the packaged debug launcher**

Add this test to `builder.tests/WiiRuntimeSourceTests.cs`:

```csharp
/// <summary>
/// Ensures Wii packaged-disc debugging uses one explicit launcher that targets an existing ISO and a seeded Dolphin logging profile.
/// </summary>
[Fact]
public void PackagedDebugLauncher_UsesExplicitIsoAndSeededLoggingProfile() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string launcherPath = Path.Combine(repositoryRootPath, "tmp", "launch_wii_packaged_debug_session.ps1");

    Assert.True(File.Exists(launcherPath), "Expected tmp/launch_wii_packaged_debug_session.ps1 to exist.");

    string launcherSource = File.ReadAllText(launcherPath);

    Assert.Contains("[Parameter(Mandatory = $true)]", launcherSource, StringComparison.Ordinal);
    Assert.Contains("[string]$IsoPath", launcherSource, StringComparison.Ordinal);
    Assert.Contains("Get-Process -Name 'Dolphin'", launcherSource, StringComparison.Ordinal);
    Assert.Contains("Logger.ini", launcherSource, StringComparison.Ordinal);
    Assert.Contains("WriteToConsole = True", launcherSource, StringComparison.Ordinal);
    Assert.Contains("OSREPORT = True", launcherSource, StringComparison.Ordinal);
    Assert.Contains("OSREPORT_HLE = True", launcherSource, StringComparison.Ordinal);
    Assert.Contains("Start-Process", launcherSource, StringComparison.Ordinal);
    Assert.Contains("'-u', $userDir, '-e', $resolvedIsoPath", launcherSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the new test to verify it fails**

Run:

```bash
dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter PackagedDebugLauncher_UsesExplicitIsoAndSeededLoggingProfile -v minimal
```

Expected: FAIL because `tmp/launch_wii_packaged_debug_session.ps1` does not exist yet.

- [ ] **Step 3: Write the minimal packaged debug launcher**

Create `tmp/launch_wii_packaged_debug_session.ps1` with this content:

```powershell
param(
    [Parameter(Mandatory = $true)]
    [string]$IsoPath
)

$ErrorActionPreference = 'Stop'

$resolvedIsoPath = [System.IO.Path]::GetFullPath($IsoPath)
if (-not (Test-Path -LiteralPath $resolvedIsoPath)) {
    throw "ISO was not found: $resolvedIsoPath"
}

$dolphinPath = 'C:\dev\helworks\emus\dolphin-2603a-x64\Dolphin-x64\Dolphin.exe'
$userDir = 'C:\dev\helworks\helengine-wii\tmp\dolphin-packaged-debug-user'
$seedConfigDirectory = 'C:\dev\helworks\helengine-wii\tmp\dolphin-packaged-debug-seed'
$stdoutPath = 'C:\dev\helworks\helengine-wii\tmp\packaged-disc-proof-life\packaged-debug-stdout.log'
$stderrPath = 'C:\dev\helworks\helengine-wii\tmp\packaged-disc-proof-life\packaged-debug-stderr.log'

if (-not (Test-Path -LiteralPath $dolphinPath)) {
    throw "Dolphin executable was not found: $dolphinPath"
}

if (Test-Path -LiteralPath $userDir) {
    Remove-Item -LiteralPath $userDir -Recurse -Force
}

if (Test-Path -LiteralPath $seedConfigDirectory) {
    Remove-Item -LiteralPath $seedConfigDirectory -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $seedConfigDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $userDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $userDir 'Config') | Out-Null

Set-Content -LiteralPath (Join-Path $seedConfigDirectory 'Dolphin.ini') -Value @(
    '[Analytics]'
    'Enabled = False'
    'PermissionAsked = True'
    '[General]'
    'UseDiscordPresence = False'
) -Encoding ASCII

Set-Content -LiteralPath (Join-Path $seedConfigDirectory 'Logger.ini') -Value @(
    '[Logs]'
    'CORE = True'
    'BOOT = True'
    'DVD = True'
    'IOS = True'
    'IOS_DI = True'
    'IOS_FS = True'
    'OSREPORT = True'
    'OSREPORT_HLE = True'
    'FileMon = True'
    'WII_IPC = True'
    '[Options]'
    'WriteToConsole = True'
    'WriteToFile = False'
    'WriteToWindow = False'
    'Verbosity = 1'
) -Encoding ASCII

$globalQtPath = 'C:\Users\Helena\AppData\Roaming\Dolphin Emulator\Config\Qt.ini'
if (-not (Test-Path -LiteralPath $globalQtPath)) {
    throw "Dolphin Qt.ini was not found: $globalQtPath"
}

Copy-Item -LiteralPath $globalQtPath -Destination (Join-Path $seedConfigDirectory 'Qt.ini') -Force
Copy-Item -LiteralPath (Join-Path $seedConfigDirectory 'Qt.ini') -Destination (Join-Path $userDir 'Config\Qt.ini') -Force
Copy-Item -LiteralPath (Join-Path $seedConfigDirectory 'Dolphin.ini') -Destination (Join-Path $userDir 'Config\Dolphin.ini') -Force
Copy-Item -LiteralPath (Join-Path $seedConfigDirectory 'Logger.ini') -Destination (Join-Path $userDir 'Config\Logger.ini') -Force

foreach ($logPath in @($stdoutPath, $stderrPath)) {
    if (Test-Path -LiteralPath $logPath) {
        Remove-Item -LiteralPath $logPath -Force
    }
}

$existingDolphinProcesses = @(Get-Process -Name 'Dolphin' -ErrorAction SilentlyContinue)
foreach ($process in $existingDolphinProcesses) {
    Stop-Process -Id $process.Id -Force
}

$isoItem = Get-Item -LiteralPath $resolvedIsoPath
Write-Output ("ISO=" + $resolvedIsoPath)
Write-Output ("ISO_LAST_WRITE_TIME=" + $isoItem.LastWriteTime.ToString('O'))
Write-Output ("DOLPHIN=" + $dolphinPath)
Write-Output ("USER_DIR=" + $userDir)
Write-Output ("STDOUT=" + $stdoutPath)
Write-Output ("STDERR=" + $stderrPath)

$process = Start-Process -FilePath $dolphinPath -ArgumentList '-b', '-u', $userDir, '-e', $resolvedIsoPath -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -PassThru -WindowStyle Hidden
Write-Output ("PROCESS_ID=" + $process.Id)
```

- [ ] **Step 4: Run the focused test to verify it passes**

Run:

```bash
dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter PackagedDebugLauncher_UsesExplicitIsoAndSeededLoggingProfile -v minimal
```

Expected: PASS with `1 passed`.

- [ ] **Step 5: Commit**

```bash
git add tmp/launch_wii_packaged_debug_session.ps1 builder.tests/WiiRuntimeSourceTests.cs
git commit -m "feat: add Wii packaged debug launcher"
```

### Task 2: Add Temporary Host-Visible Runtime Diagnostics

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Modify: `src/platform/wii/WiiApplication.cpp`
- Modify: `src/platform/wii/WiiRenderManager2D.hpp`
- Modify: `src/platform/wii/WiiRenderManager2D.cpp`

- [ ] **Step 1: Write the failing source-audit test for packaged runtime text-path diagnostics**

Add this test to `builder.tests/WiiRuntimeSourceTests.cs`:

```csharp
/// <summary>
/// Ensures the packaged-disc Wii runtime exposes temporary host-visible diagnostics for the menu text path while debugging.
/// </summary>
[Fact]
public void PackagedGpuText_ExposesHostVisibleDiagnosticsForMenuTextPath() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));
    string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.hpp"));
    string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.cpp"));

    Assert.Contains("get_LastTextLoadStage()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("get_LastTextFontRelativePath()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("get_LastTextureRelativePath()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("get_VisitedCameraCount()", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("get_VisitedDrawableCount()", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("get_QueuedTextCount()", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("get_DidSubmitGlyph()", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("DidSubmitGlyph = true;", renderManagerSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```bash
dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter PackagedGpuText_ExposesHostVisibleDiagnosticsForMenuTextPath -v minimal
```

Expected: FAIL because the new accessors and diagnostic state do not exist yet.

- [ ] **Step 3: Add the minimal temporary diagnostic surface**

Add these members to `src/platform/wii/WiiRenderManager2D.hpp` inside the public section:

```cpp
        /// Returns the number of enabled cameras visited during the current frame capture.
        int32_t get_VisitedCameraCount() const;

        /// Returns the number of 2D drawables visited during the current frame capture.
        int32_t get_VisitedDrawableCount() const;

        /// Returns the number of queued text drawables captured during the current frame.
        int32_t get_QueuedTextCount() const;

        /// Returns whether the current frame submitted at least one glyph quad.
        bool get_DidSubmitGlyph() const;
```

Add this field to the private section:

```cpp
        /// Tracks whether the current frame submitted at least one glyph quad.
        bool DidSubmitGlyph;
```

Initialize and maintain that state in `src/platform/wii/WiiRenderManager2D.cpp`:

```cpp
    WiiRenderManager2D::WiiRenderManager2D()
        : RenderManager2D()
        , VisitedCameraCount(0)
        , VisitedDrawableCount(0)
        , DidSubmitGlyph(false) {
    }
```

```cpp
    void WiiRenderManager2D::BeginFrame() {
        SpriteQueue.clear();
        TextQueue.clear();
        RoundedRectQueue.clear();
        VisitedCameraCount = 0;
        VisitedDrawableCount = 0;
        DidSubmitGlyph = false;
    }
```

```cpp
                DrawTexturedQuad2D(
                    static_cast<float>(lineOriginX + offsetX),
                    static_cast<float>(baseY + snappedLineOffsetY + (glyph.OffsetY * fontScale)),
                    static_cast<float>(glyphWidth),
                    static_cast<float>(glyphHeight),
                    glyph.SourceRect,
                    drawable->get_Color(),
                    texture);
                DidSubmitGlyph = true;
```

Add these getters:

```cpp
    int32_t WiiRenderManager2D::get_VisitedCameraCount() const {
        return VisitedCameraCount;
    }

    int32_t WiiRenderManager2D::get_VisitedDrawableCount() const {
        return VisitedDrawableCount;
    }

    int32_t WiiRenderManager2D::get_QueuedTextCount() const {
        return static_cast<int32_t>(TextQueue.size());
    }

    bool WiiRenderManager2D::get_DidSubmitGlyph() const {
        return DidSubmitGlyph;
    }
```

Then add one temporary packaged-only log block in `src/platform/wii/WiiApplication.cpp` immediately after `EngineRenderManager2D->RenderCapturedText(...)`:

```cpp
            if (DrawFrameLogCount < 8U) {
                SYS_Report(
                    "[Wii] Text capture cameras=%ld drawables=%ld queuedText=%ld glyph=%s textStage=%s textFont=%s texture=%s\n",
                    static_cast<long>(EngineRenderManager2D->get_VisitedCameraCount()),
                    static_cast<long>(EngineRenderManager2D->get_VisitedDrawableCount()),
                    static_cast<long>(EngineRenderManager2D->get_QueuedTextCount()),
                    EngineRenderManager2D->get_DidSubmitGlyph() ? "true" : "false",
                    EngineCore->get_SceneLoadService() != nullptr ? EngineCore->get_SceneLoadService()->get_LastTextLoadStage().c_str() : "<null>",
                    EngineCore->get_SceneLoadService() != nullptr ? EngineCore->get_SceneLoadService()->get_LastTextFontRelativePath().c_str() : "<null>",
                    EngineCore->get_SceneLoadService() != nullptr ? EngineCore->get_SceneLoadService()->get_LastTextureRelativePath().c_str() : "<null>");
            }
```

- [ ] **Step 4: Run the focused test to verify it passes**

Run:

```bash
dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter PackagedGpuText_ExposesHostVisibleDiagnosticsForMenuTextPath -v minimal
```

Expected: PASS with `1 passed`.

- [ ] **Step 5: Commit**

```bash
git add builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiApplication.cpp src/platform/wii/WiiRenderManager2D.hpp src/platform/wii/WiiRenderManager2D.cpp
git commit -m "feat: add Wii packaged text-path diagnostics"
```

### Task 3: Eliminate the Startup Warning on the Chosen Packaged Path

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Modify: `src/platform/wii/WiiSceneBootstrap.cpp`
- Modify: `src/platform/wii/WiiDiscFileSystem.cpp`
- Modify: `tmp/launch_wii_packaged_debug_session.ps1`

- [ ] **Step 1: Write the failing source-audit test for the chosen clean packaged path**

Add this test to `builder.tests/WiiRuntimeSourceTests.cs`:

```csharp
/// <summary>
/// Ensures the packaged-disc debug workflow keeps using the clean startup lane and avoids the direct-DOL warning path.
/// </summary>
[Fact]
public void PackagedDebugWorkflow_UsesPackagedIsoLaneOnly() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string launcherSource = File.ReadAllText(Path.Combine(repositoryRootPath, "tmp", "launch_wii_packaged_debug_session.ps1"));

    Assert.DoesNotContain("helengine_wii.dol", launcherSource, StringComparison.Ordinal);
    Assert.DoesNotContain("direct-dol", launcherSource, StringComparison.OrdinalIgnoreCase);
    Assert.Contains("'-b', '-u', $userDir, '-e', $resolvedIsoPath", launcherSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the test to verify it passes before deeper runtime debugging**

Run:

```bash
dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter PackagedDebugWorkflow_UsesPackagedIsoLaneOnly -v minimal
```

Expected: PASS. If it fails, fix the launcher first before continuing.

- [ ] **Step 3: Run the packaged debug launcher and inspect host-visible output**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tmp\launch_wii_packaged_debug_session.ps1 `
  -IsoPath .\tmp\packaged-disc-proof-life\city.iso
```

Expected:

- the script prints `ISO=`, `ISO_LAST_WRITE_TIME=`, `DOLPHIN=`, `USER_DIR=`, `STDOUT=`, `STDERR=`, and `PROCESS_ID=`
- the launcher uses the packaged ISO path only

Then inspect the captured host logs:

```powershell
Get-Content .\tmp\packaged-disc-proof-life\packaged-debug-stdout.log -TotalCount 120
Get-Content .\tmp\packaged-disc-proof-life\packaged-debug-stderr.log -TotalCount 120
```

Expected: enough output to determine whether the warning is still coming from this lane.

- [ ] **Step 4: Apply the minimal startup-warning fix in the traced layer**

If the warning still reproduces on the packaged lane, fix only the smallest traced cause in either:

- `src/platform/wii/WiiSceneBootstrap.cpp`
- `src/platform/wii/WiiDiscFileSystem.cpp`

The implementation must preserve packaged-disc boot semantics and avoid introducing direct-`DOL` fallback logic.

- [ ] **Step 5: Re-run the packaged debug launcher to verify clean startup**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tmp\launch_wii_packaged_debug_session.ps1 `
  -IsoPath .\tmp\packaged-disc-proof-life\city.iso
```

Expected:

- no Dolphin startup warning dialog
- host-captured logs still work

- [ ] **Step 6: Commit**

```bash
git add builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiSceneBootstrap.cpp src/platform/wii/WiiDiscFileSystem.cpp tmp/launch_wii_packaged_debug_session.ps1
git commit -m "fix: clean packaged Wii startup path"
```

### Task 4: Make Authored Menu Text Visible and Remove Debug-Only Output

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Modify: `src/platform/wii/WiiApplication.cpp`
- Modify: `src/platform/wii/WiiRenderManager2D.hpp`
- Modify: `src/platform/wii/WiiRenderManager2D.cpp`
- Modify: `README.md`

- [ ] **Step 1: Write the failing source-audit test for the final proof-state renderer contract**

Add this test to `builder.tests/WiiRuntimeSourceTests.cs`:

```csharp
/// <summary>
/// Ensures the final packaged Wii text proof keeps GPU-only glyph rendering and does not retain temporary on-screen debug markers.
/// </summary>
[Fact]
public void PackagedGpuText_FinalProofRemovesTemporaryOnScreenMarkers() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.cpp"));

    Assert.Contains("GX_LoadTexObj(", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("DrawTexturedQuad2D(", renderManagerSource, StringComparison.Ordinal);
    Assert.DoesNotContain("DrawSolidQuad2D(8.0f, 8.0f, 24.0f, 24.0f", renderManagerSource, StringComparison.Ordinal);
    Assert.DoesNotContain("DrawSolidQuad2D(40.0f, 8.0f, 24.0f, 24.0f", renderManagerSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```bash
dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter PackagedGpuText_FinalProofRemovesTemporaryOnScreenMarkers -v minimal
```

Expected: FAIL while the temporary on-screen markers still exist.

- [ ] **Step 3: Use the packaged debug logs to find the first broken text-path link and apply the minimal fix**

Run the packaged lane again:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tmp\launch_wii_packaged_debug_session.ps1 `
  -IsoPath .\tmp\packaged-disc-proof-life\city.iso
```

Then inspect the host logs:

```powershell
Get-Content .\tmp\packaged-disc-proof-life\packaged-debug-stdout.log -TotalCount 160
Get-Content .\tmp\packaged-disc-proof-life\packaged-debug-stderr.log -TotalCount 160
```

Use the diagnostic chain to identify the first failing link:

- `textStage` never reaches a resolved font or atlas path
- camera count is zero
- drawable count is zero
- queued text count is zero
- glyph submission stays false

Apply only the smallest runtime fix needed in `src/platform/wii/WiiApplication.cpp` or `src/platform/wii/WiiRenderManager2D.cpp` to make authored menu text appear through the existing GPU glyph path.

- [ ] **Step 4: Remove or gate temporary on-screen markers while preserving useful host logging**

Delete the corner `DrawSolidQuad2D(...)` marker calls from `RenderCapturedText()` once the root cause is fixed. Keep only the host-visible logging needed for the final proof run.

- [ ] **Step 5: Run focused tests and final packaged proof**

Run:

```bash
dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter WiiRuntimeSourceTests -v minimal
```

Expected: PASS.

Then run the final packaged proof:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tmp\launch_wii_packaged_debug_session.ps1 `
  -IsoPath .\tmp\packaged-disc-proof-life\city.iso
```

Expected:

- no Dolphin startup warning dialog
- host-captured logs show the packaged text path reaching the expected stage
- authored main-menu text is visible
- no CPU fallback was added

- [ ] **Step 6: Update README only if the packaged debug launcher is now part of the intended Wii debug workflow**

If the new launcher is part of the normal Wii debug workflow, add one short note to `README.md` under the Dolphin section describing when to use it.

- [ ] **Step 7: Commit**

```bash
git add builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiApplication.cpp src/platform/wii/WiiRenderManager2D.hpp src/platform/wii/WiiRenderManager2D.cpp README.md
git commit -m "feat: show Wii packaged main menu text"
```

## Self-Review

- Spec coverage: Task 1 covers the repeatable packaged debug lane, Task 2 covers host-visible runtime diagnostics, Task 3 covers the clean-start gate, and Task 4 covers visible authored text plus cleanup of temporary on-screen markers.
- Placeholder scan: no `TODO`, `TBD`, or “fix the issue somehow” steps remain; every step names exact files, commands, and expected outcomes.
- Type consistency: the plan consistently uses `launch_wii_packaged_debug_session.ps1`, `-IsoPath`, `textStage`, `VisitedCameraCount`, `VisitedDrawableCount`, `QueuedTextCount`, and `DidSubmitGlyph` across later tasks.
