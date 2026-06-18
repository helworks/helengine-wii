# Helengine Wii Main Menu Scene-Load Proof Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove that the authored Wii startup scene `Scenes/DemoDiscMainMenu.helen` loads through the direct-`DOL` developer boot path in Dolphin, using narrow Wii-owned runtime evidence instead of visible rendering work.

**Architecture:** Keep the proof slice Wii-owned by logging the generated-core `SceneManager` and `RuntimeSceneLoadService` trace state from `WiiApplication` during the first few update frames. Guard the new logging contract with source-audit tests, then rebuild and verify in Dolphin using the existing direct-`DOL` boot path and runtime trace artifacts.

**Tech Stack:** C#/.NET source-audit tests, generated C++ runtime, libogc Wii host, Dockerized devkitPPC build, Dolphin

---

## File Structure

- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
  Responsibility: lock the Wii source contract around startup-scene queue proof and first scene/content trace logging.
- Modify: `src/platform/wii/WiiApplication.cpp`
  Responsibility: emit narrow Wii-owned scene-manager and scene-load trace lines during the first update frames after startup-scene queueing.
- Read for reference only: `tmp/generated-core-wii/SceneManager.hpp`
  Responsibility: expose existing scene-manager trace getters already generated for Wii.
- Read for reference only: `tmp/generated-core-wii/RuntimeSceneLoadService.hpp`
  Responsibility: expose existing scene-load/content trace getters already generated for Wii.

### Task 1: Add The Failing Wii Trace-Contract Test

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Add the failing source-audit test for scene-load proof logging**

Insert this test after `PackagedBootstrap_UsesManifestCatalogAndStartupSceneQueue`:

```csharp
/// <summary>
/// Ensures the direct-DOL Wii runtime reports the queued startup scene and the first generated scene/content trace state needed to prove authored scene loading.
/// </summary>
[Fact]
public void DirectDolBootstrap_ReportsStartupSceneAndGeneratedSceneLoadTrace() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));

    Assert.Contains("SYS_Report(\"[Wii] Startup scene id: %s\\n\", startupSceneId.c_str());", applicationSource, StringComparison.Ordinal);
    Assert.Contains("EngineCore->get_SceneManager()->get_LastTraceStage()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("EngineCore->get_SceneManager()->get_LastTraceSceneId()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("EngineCore->get_SceneManager()->get_LastTraceLoadedSceneCount()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("EngineCore->get_SceneManager()->get_LastTracePendingOperationCount()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("EngineCore->get_SceneLoadService()->get_LastTraceStage()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("EngineCore->get_SceneLoadService()->get_LastTextLoadStage()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("EngineCore->get_SceneLoadService()->get_LastTextFontRelativePath()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("EngineCore->get_SceneLoadService()->get_LastTextureRelativePath()", applicationSource, StringComparison.Ordinal);
    Assert.Contains("[Wii] SceneManager trace stage=", applicationSource, StringComparison.Ordinal);
    Assert.Contains("[Wii] SceneLoad trace stage=", applicationSource, StringComparison.Ordinal);
    Assert.Contains("[WiiFile] SceneManager trace stage=", applicationSource, StringComparison.Ordinal);
    Assert.Contains("[WiiFile] SceneLoad trace stage=", applicationSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the focused test to verify it fails**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wii\builder.tests\helengine.wii.builder.tests.csproj --filter DirectDolBootstrap_ReportsStartupSceneAndGeneratedSceneLoadTrace --no-restore
```

Expected:

- FAIL because `WiiApplication.cpp` does not yet report the generated scene-manager and scene-load trace state.

- [ ] **Step 3: Commit the failing-test checkpoint**

```bash
rtk git add builder.tests/WiiRuntimeSourceTests.cs
rtk git commit -m "Add Wii main menu scene-load proof test"
```

### Task 2: Implement Narrow Wii-Owned Scene-Load Proof Logging

**Files:**
- Modify: `src/platform/wii/WiiApplication.cpp`

- [ ] **Step 1: Extend the direct-DOL scene queue log to keep the authored startup-scene id visible**

Keep this direct-DOL queue block in `WiiApplication::InitializeEngineCore()` intact:

```cpp
#if HELENGINE_WII_PACKAGED_DISC_BOOT
            const std::string packagedStartupSceneId = WiiSceneBootstrap::GetPackagedStartupSceneId();
            EngineCore->get_SceneManager()->LoadScene(packagedStartupSceneId, SceneLoadMode::Single);
#else
            const std::string startupSceneId = WiiSceneBootstrap::GetStartupSceneId();
            EngineCore->get_SceneManager()->LoadScene(startupSceneId, SceneLoadMode::Single);
#endif
            SYS_Report("[Wii] Runtime startup scene queued.\n");
            AppendRuntimeTrace("[WiiFile] Runtime startup scene queued.\n");
```

Do not replace it with a generic helper. This plan depends on the direct-DOL startup scene remaining explicitly visible in the source.

- [ ] **Step 2: Add first-frame scene-manager trace logging inside `UpdateEngineCore()`**

Inside the `try` block in `WiiApplication::UpdateEngineCore()`, immediately after:

```cpp
            EngineCore->Update();
```

insert:

```cpp
            if (UpdateFrameLogCount < 8U && EngineCore->get_SceneManager() != nullptr) {
                SYS_Report(
                    "[Wii] SceneManager trace stage=%s scene=%s loaded=%ld pending=%ld\n",
                    EngineCore->get_SceneManager()->get_LastTraceStage().c_str(),
                    EngineCore->get_SceneManager()->get_LastTraceSceneId().c_str(),
                    static_cast<long>(EngineCore->get_SceneManager()->get_LastTraceLoadedSceneCount()),
                    static_cast<long>(EngineCore->get_SceneManager()->get_LastTracePendingOperationCount()));
                AppendRuntimeTrace(
                    "[WiiFile] SceneManager trace stage=%s scene=%s loaded=%ld pending=%ld\n",
                    EngineCore->get_SceneManager()->get_LastTraceStage().c_str(),
                    EngineCore->get_SceneManager()->get_LastTraceSceneId().c_str(),
                    static_cast<long>(EngineCore->get_SceneManager()->get_LastTraceLoadedSceneCount()),
                    static_cast<long>(EngineCore->get_SceneManager()->get_LastTracePendingOperationCount()));
            }
```

- [ ] **Step 3: Add first-frame scene-load/content trace logging beside the scene-manager trace**

Immediately after the scene-manager trace block from Step 2, insert:

```cpp
            if (UpdateFrameLogCount < 8U && EngineCore->get_SceneLoadService() != nullptr) {
                SYS_Report(
                    "[Wii] SceneLoad trace stage=%s root=%ld depth=%ld component=%s textStage=%s textFont=%s texture=%s\n",
                    EngineCore->get_SceneLoadService()->get_LastTraceStage().c_str(),
                    static_cast<long>(EngineCore->get_SceneLoadService()->get_LastTraceRootEntityIndex()),
                    static_cast<long>(EngineCore->get_SceneLoadService()->get_LastTraceEntityDepth()),
                    EngineCore->get_SceneLoadService()->get_LastTraceComponentTypeId().c_str(),
                    EngineCore->get_SceneLoadService()->get_LastTextLoadStage().c_str(),
                    EngineCore->get_SceneLoadService()->get_LastTextFontRelativePath().c_str(),
                    EngineCore->get_SceneLoadService()->get_LastTextureRelativePath().c_str());
                AppendRuntimeTrace(
                    "[WiiFile] SceneLoad trace stage=%s root=%ld depth=%ld component=%s textStage=%s textFont=%s texture=%s\n",
                    EngineCore->get_SceneLoadService()->get_LastTraceStage().c_str(),
                    static_cast<long>(EngineCore->get_SceneLoadService()->get_LastTraceRootEntityIndex()),
                    static_cast<long>(EngineCore->get_SceneLoadService()->get_LastTraceEntityDepth()),
                    EngineCore->get_SceneLoadService()->get_LastTraceComponentTypeId().c_str(),
                    EngineCore->get_SceneLoadService()->get_LastTextLoadStage().c_str(),
                    EngineCore->get_SceneLoadService()->get_LastTextFontRelativePath().c_str(),
                    EngineCore->get_SceneLoadService()->get_LastTextureRelativePath().c_str());
            }
```

- [ ] **Step 4: Run the focused source-audit test to verify it passes**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wii\builder.tests\helengine.wii.builder.tests.csproj --filter DirectDolBootstrap_ReportsStartupSceneAndGeneratedSceneLoadTrace --no-restore
```

Expected:

- PASS

- [ ] **Step 5: Commit the Wii logging implementation**

```bash
rtk git add builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiApplication.cpp
rtk git commit -m "Add Wii main menu scene-load proof tracing"
```

### Task 3: Re-Run Focused Wii Validation

**Files:**
- No code changes. Verification only.

- [ ] **Step 1: Run the full Wii-focused builder test slice**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-wii\builder.tests\helengine.wii.builder.tests.csproj --filter Wii --no-restore
```

Expected:

- PASS

- [ ] **Step 2: Rebuild the direct-DOL Wii binary with a bounded verification frame limit**

Run:

```powershell
rtk proxy docker run --rm -v C:/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii -e HELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT=3 helengine-wii sh -lc "make clean all >/tmp/wii-main-menu-proof-build.log 2>&1; status=$?; tail -c 3000 /tmp/wii-main-menu-proof-build.log; exit $status"
```

Expected:

- `build/helengine_wii.dol` is emitted successfully

- [ ] **Step 3: Capture the rebuilt artifact timestamp**

Run:

```powershell
Get-Item 'C:\dev\helworks\helengine-wii\build\helengine_wii.dol' | Format-List FullName,Length,LastWriteTimeUtc
```

Expected:

- timestamp reflects the bounded verification rebuild

### Task 4: Verify The Main Menu Scene-Load Proof In Dolphin

**Files:**
- No code changes. Runtime verification only.

- [ ] **Step 1: Remove stale runtime trace artifacts before the emulator run**

Run:

```powershell
if (Test-Path 'C:\dev\helworks\helengine-wii\runtime_registry_trace.txt') { Remove-Item 'C:\dev\helworks\helengine-wii\runtime_registry_trace.txt' -Force }
if (Test-Path 'C:\dev\helworks\helengine-wii\tmp\dolphin-wii-sd-sync\runtime_registry_trace.txt') { Remove-Item 'C:\dev\helworks\helengine-wii\tmp\dolphin-wii-sd-sync\runtime_registry_trace.txt' -Force }
```

Expected:

- no stale Wii runtime trace file remains from earlier emulator sessions

- [ ] **Step 2: Launch Dolphin against the bounded direct-DOL build**

Run:

```powershell
$dolphin = 'C:\dev\helworks\emus\dolphin-2603a-x64\Dolphin-x64\Dolphin.exe'
$dol = 'C:\dev\helworks\helengine-wii\build\helengine_wii.dol'
$work = 'C:\dev\helworks\helengine-wii'
$process = Start-Process -FilePath $dolphin -ArgumentList '-b','-e',$dol -WorkingDirectory $work -PassThru
$exited = $process.WaitForExit(60000)
if (-not $exited) { Stop-Process -Id $process.Id -Force }
Write-Output ('Exited=' + $exited)
Write-Output ('ExitCode=' + $process.ExitCode)
```

Expected:

- Dolphin executes the title
- process exit may still be `False` for this emulator build; do not treat that alone as failure

- [ ] **Step 3: Read the Wii runtime trace if it was written**

Run:

```powershell
if (Test-Path 'C:\dev\helworks\helengine-wii\runtime_registry_trace.txt') {
    Get-Content 'C:\dev\helworks\helengine-wii\runtime_registry_trace.txt' -TotalCount 160
} elseif (Test-Path 'C:\dev\helworks\helengine-wii\tmp\dolphin-wii-sd-sync\runtime_registry_trace.txt') {
    Get-Content 'C:\dev\helworks\helengine-wii\tmp\dolphin-wii-sd-sync\runtime_registry_trace.txt' -TotalCount 160
}
```

Expected preferred evidence:

- `Startup scene id: Scenes/DemoDiscMainMenu.helen`
- `[WiiFile] SceneManager trace stage=... scene=Scenes/DemoDiscMainMenu.helen ...`
- `[WiiFile] SceneLoad trace stage=...`
- one of the expected authored asset paths, such as `DemoDiscMainMenu.hasset`, a menu font path, or a menu texture path

- [ ] **Step 4: If no runtime trace file exists, use Dolphin execution evidence as the fallback proof**

Run:

```powershell
$timePlayedPath = 'C:\Users\Helena\AppData\Roaming\Dolphin Emulator\Config\TimePlayed.ini'
Get-Content $timePlayedPath | Select-String 'ID-helengine_wii = '
Get-Item 'C:\Users\Helena\AppData\Roaming\Dolphin Emulator\Config\TimePlayed.ini' | Format-List FullName,Length,LastWriteTimeUtc
```

Expected fallback evidence:

- `ID-helengine_wii = ...` exists in `TimePlayed.ini`
- timestamp reflects the emulator run
- this is acceptable only if the trace file still does not materialize

- [ ] **Step 5: Commit the proof-slice completion**

```bash
rtk git add builder.tests/WiiRuntimeSourceTests.cs src/platform/wii/WiiApplication.cpp
rtk git commit -m "Add Wii main menu scene-load proof"
```
