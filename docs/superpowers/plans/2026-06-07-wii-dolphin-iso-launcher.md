# Wii Dolphin ISO Launcher Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a repo-local PowerShell launcher that force-closes Dolphin, prints the selected ISO timestamp, seeds an isolated logging profile, and launches an explicitly requested Wii ISO.

**Architecture:** Keep the implementation as one focused PowerShell script under `tmp/`, plus one source-audit test file and one short README update. Reuse the existing Dolphin executable path and logging-profile conventions already present in the current Wii helper scripts instead of inventing a new configuration system.

**Tech Stack:** PowerShell, xUnit source-audit tests in `builder.tests`, existing Dolphin helper conventions, README documentation.

---

## File Map

- Create: `tmp/launch_wii_iso_in_dolphin.ps1`
  - Wii developer utility that validates `-IsoPath`, force-closes Dolphin, recreates an isolated user directory, seeds logging config, prints launch metadata, and starts Dolphin.
- Create: `builder.tests/WiiDolphinLauncherScriptTests.cs`
  - Source-audit coverage for the launcher contract so the script remains explicit-path, force-close, and logging-profile based.
- Modify: `README.md`
  - Add one concise usage example for the new launcher script in the Dolphin-launching section.

### Task 1: Add Script Contract Coverage

**Files:**
- Create: `builder.tests/WiiDolphinLauncherScriptTests.cs`
- Test: `builder.tests/helengine.wii.builder.tests.csproj`

- [ ] **Step 1: Write the failing test**

Create `builder.tests/WiiDolphinLauncherScriptTests.cs` with this content:

```csharp
namespace helengine.wii.builder.tests;

/// <summary>
/// Guards the developer launcher contract for running explicit Wii ISO files in Dolphin.
/// </summary>
public sealed class WiiDolphinLauncherScriptTests {
    /// <summary>
    /// Ensures the launcher keeps an explicit ISO path contract, force-closes Dolphin, prints ISO timestamp data, and seeds the logging profile.
    /// </summary>
    [Fact]
    public void DolphinIsoLauncher_KeepsExplicitIsoPathAndLoggingProfileContract() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string scriptPath = Path.Combine(repositoryRootPath, "tmp", "launch_wii_iso_in_dolphin.ps1");

        Assert.True(File.Exists(scriptPath), "Expected tmp/launch_wii_iso_in_dolphin.ps1 to exist.");

        string scriptSource = File.ReadAllText(scriptPath);

        Assert.Contains("[Parameter(Mandatory = $true)]", scriptSource, StringComparison.Ordinal);
        Assert.Contains("[string]$IsoPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Get-Process -Name 'Dolphin'", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Stop-Process", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Get-Item -LiteralPath $resolvedIsoPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("LastWriteTime", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Qt.ini", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Dolphin.ini", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Logger.ini", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Start-Process", scriptSource, StringComparison.Ordinal);
        Assert.Contains("'-u', $userDir, '-e', $resolvedIsoPath", scriptSource, StringComparison.Ordinal);
        Assert.DoesNotContain("city.iso", scriptSource, StringComparison.Ordinal);
    }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter WiiDolphinLauncherScriptTests -v minimal
```

Expected: FAIL because `tmp/launch_wii_iso_in_dolphin.ps1` does not exist yet.

- [ ] **Step 3: Write minimal implementation**

Create `tmp/launch_wii_iso_in_dolphin.ps1` with this content:

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
$seedConfigDirectory = 'C:\Users\Helena\AppData\Roaming\Dolphin Emulator\Config'
$userDir = 'C:\dev\helworks\helengine-wii\tmp\dolphin-launcher-user'

if (-not (Test-Path -LiteralPath $dolphinPath)) {
    throw "Dolphin executable was not found: $dolphinPath"
}

foreach ($fileName in @('Qt.ini', 'Dolphin.ini', 'Logger.ini')) {
    $seedPath = Join-Path $seedConfigDirectory $fileName
    if (-not (Test-Path -LiteralPath $seedPath)) {
        throw "Dolphin logging profile file was not found: $seedPath"
    }
}

$existingDolphinProcesses = @(Get-Process -Name 'Dolphin' -ErrorAction SilentlyContinue)
foreach ($process in $existingDolphinProcesses) {
    Stop-Process -Id $process.Id -Force
}

if (Test-Path -LiteralPath $userDir) {
    Remove-Item -LiteralPath $userDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $userDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $userDir 'Config') | Out-Null

foreach ($fileName in @('Qt.ini', 'Dolphin.ini', 'Logger.ini')) {
    $sourcePath = Join-Path $seedConfigDirectory $fileName
    $destinationPath = Join-Path $userDir ("Config\" + $fileName)
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
}

$isoItem = Get-Item -LiteralPath $resolvedIsoPath

Write-Output ("ISO=" + $resolvedIsoPath)
Write-Output ("ISO_LAST_WRITE_TIME=" + $isoItem.LastWriteTime.ToString('O'))
Write-Output ("DOLPHIN=" + $dolphinPath)
Write-Output ("USER_DIR=" + $userDir)

Start-Process -FilePath $dolphinPath -ArgumentList '-u', $userDir, '-e', $resolvedIsoPath
```

- [ ] **Step 4: Run test to verify it passes**

Run:

```bash
dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter WiiDolphinLauncherScriptTests -v minimal
```

Expected: PASS with `1 passed`.

- [ ] **Step 5: Commit**

```bash
git add builder.tests/WiiDolphinLauncherScriptTests.cs tmp/launch_wii_iso_in_dolphin.ps1
git commit -m "feat: add Wii Dolphin ISO launcher"
```

### Task 2: Document the Launcher

**Files:**
- Modify: `README.md`
- Test: `builder.tests/helengine.wii.builder.tests.csproj`

- [ ] **Step 1: Write the failing documentation test**

Add this second test to `builder.tests/WiiDolphinLauncherScriptTests.cs`:

```csharp
/// <summary>
/// Ensures the README documents the explicit ISO launcher workflow for Dolphin.
/// </summary>
[Fact]
public void Readme_DocumentsExplicitIsoLauncherWorkflow() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string readmeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "README.md"));

    Assert.Contains("launch_wii_iso_in_dolphin.ps1", readmeSource, StringComparison.Ordinal);
    Assert.Contains("-IsoPath", readmeSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter Readme_DocumentsExplicitIsoLauncherWorkflow -v minimal
```

Expected: FAIL because the README does not mention the new launcher yet.

- [ ] **Step 3: Write minimal documentation**

Update the `## Launching in Dolphin` section in `README.md` to include this block directly after the explanatory sentence:

```markdown
Launch an existing ISO with the repo helper:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tmp\launch_wii_iso_in_dolphin.ps1 `
  -IsoPath .\tmp\packaged-disc-proof-life\city.iso
```
```

Keep the rest of the section concise and do not add build logic to this launcher description.

- [ ] **Step 4: Run the focused tests**

Run:

```bash
dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter WiiDolphinLauncherScriptTests -v minimal
```

Expected: PASS with `2 passed`.

- [ ] **Step 5: Manual verification**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tmp\launch_wii_iso_in_dolphin.ps1 -IsoPath .\tmp\packaged-disc-proof-life\city.iso
```

Expected:

- any running Dolphin instances are force-closed
- the script prints `ISO=`, `ISO_LAST_WRITE_TIME=`, `DOLPHIN=`, and `USER_DIR=`
- Dolphin launches using the provided ISO

Then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tmp\launch_wii_iso_in_dolphin.ps1
```

Expected: PowerShell reports that the mandatory `IsoPath` parameter is required.

- [ ] **Step 6: Commit**

```bash
git add README.md builder.tests/WiiDolphinLauncherScriptTests.cs
git commit -m "docs: document Wii Dolphin ISO launcher"
```

## Self-Review

- Spec coverage: the plan covers explicit `-IsoPath`, force-closing Dolphin, printing ISO timestamp data, isolated logging-profile seeding, repo-local script placement, and README usage documentation.
- Placeholder scan: no `TODO`, `TBD`, or implicit “add validation later” instructions remain.
- Type consistency: the script path, parameter name, printed fields, and README command all use the same `launch_wii_iso_in_dolphin.ps1` and `-IsoPath` contract.
