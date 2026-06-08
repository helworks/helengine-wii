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
        Assert.Contains("'-u', $userDir, '-e', $resolvedIsoPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("PROCESS_ID=", scriptSource, StringComparison.Ordinal);
        Assert.DoesNotContain("city.iso", scriptSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the README documents the explicit ISO launcher workflow for Dolphin.
    /// </summary>
    [Fact]
    public void Readme_DocumentsExplicitIsoLauncherWorkflow() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string readmeSource = File.ReadAllText(Path.Combine(repositoryRootPath, "README.md"));

        Assert.Contains("launch_wii_iso_in_dolphin.ps1", readmeSource, StringComparison.Ordinal);
        Assert.Contains("-IsoPath", readmeSource, StringComparison.Ordinal);
        Assert.Contains("process id", readmeSource, StringComparison.OrdinalIgnoreCase);
        Assert.Contains("logger window", readmeSource, StringComparison.OrdinalIgnoreCase);
        Assert.Contains("Logger.ini", readmeSource, StringComparison.Ordinal);
        Assert.Contains("global Dolphin profile", readmeSource, StringComparison.OrdinalIgnoreCase);
    }
}
