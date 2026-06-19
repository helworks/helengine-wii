namespace helengine.wii.builder.tests;

/// <summary>
/// Guards the canonical Wii launcher contract for running explicit Wii artifacts in Dolphin.
/// </summary>
public sealed class WiiDolphinLauncherScriptTests {
    /// <summary>
    /// Ensures the canonical launcher requires one explicit artifact path, force-closes Dolphin, prints artifact timestamp data, and seeds the logging profile.
    /// </summary>
    [Fact]
    public void Launcher_RequiresArtifactPath_AndKeepsLoggingProfileContract() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string scriptPath = Path.Combine(repositoryRootPath, "scripts", "launch_in_emulator.ps1");

        Assert.True(File.Exists(scriptPath), "Expected scripts/launch_in_emulator.ps1 to exist.");

        string scriptSource = File.ReadAllText(scriptPath);

        Assert.Contains("[Parameter(Mandatory = $true)]", scriptSource, StringComparison.Ordinal);
        Assert.Contains("[string]$ArtifactPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Get-Process -Name 'Dolphin'", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Stop-Process", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Get-Item -LiteralPath $resolvedArtifactPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("LastWriteTime", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Qt.ini", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Wii", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Backup", scriptSource, StringComparison.Ordinal);
        Assert.Contains("ResourcePacks", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Dolphin.ini", scriptSource, StringComparison.Ordinal);
        Assert.Contains("sd-sync", scriptSource, StringComparison.Ordinal);
        Assert.Contains("WiiSDCardSyncFolder", scriptSource, StringComparison.Ordinal);
        Assert.Contains("WiiSDCard = True", scriptSource, StringComparison.Ordinal);
        Assert.Contains("WiiSDCardAllowWrites = True", scriptSource, StringComparison.Ordinal);
        Assert.Contains("WiiSDCardEnableFolderSync = True", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Logger.ini", scriptSource, StringComparison.Ordinal);
        Assert.Contains("$globalLoggerPath = Join-Path $globalProfileRoot 'Config\\Logger.ini'", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Get-Content -LiteralPath $globalLoggerPath -Raw", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Set-Content -LiteralPath (Join-Path $userDir 'Config\\Logger.ini')", scriptSource, StringComparison.Ordinal);
        Assert.Contains("WriteToWindow = True", scriptSource, StringComparison.Ordinal);
        Assert.Contains("WriteToConsole = True", scriptSource, StringComparison.Ordinal);
        Assert.Contains("WriteToFile = True", scriptSource, StringComparison.Ordinal);
        Assert.Contains("logvisible=true", scriptSource, StringComparison.Ordinal);
        Assert.Contains("logconfigvisible=true", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Start-Process", scriptSource, StringComparison.Ordinal);
        Assert.Contains("-PassThru", scriptSource, StringComparison.Ordinal);
        Assert.Contains("'-u', $userDir, '-e', $resolvedArtifactPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("PROCESS_ID=", scriptSource, StringComparison.Ordinal);
        Assert.DoesNotContain("[string]$IsoPath", scriptSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the README documents the canonical Wii launcher workflow for Dolphin.
    /// </summary>
    [Fact]
    public void Readme_DocumentsCanonicalLauncherWorkflow() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string readmeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "README.md"));

        Assert.Contains("launch_in_emulator.ps1", readmeSource, StringComparison.Ordinal);
        Assert.Contains("-ArtifactPath", readmeSource, StringComparison.Ordinal);
        Assert.DoesNotContain("launch_wii_iso_in_dolphin.ps1", readmeSource, StringComparison.Ordinal);
    }
}
