using helengine.wii.builder.tests.Builders;

namespace helengine.wii.builder.tests;

/// <summary>
/// Verifies the explicit Wii Wiimms ISO Tools configuration boundary before real packaging logic is added.
/// </summary>
public sealed class WiiWiimmsIsoToolsImagePackagerTests {
    /// <summary>
    /// Ensures the packager rejects a missing configured executable path.
    /// </summary>
    [Fact]
    public void Package_WhenExecutablePathIsBlank_ThrowsInvalidOperationException() {
        WiiWiimmsIsoToolsImagePackager packager = new(
            new WiiWiimmsIsoToolsOptions(string.Empty),
            new FakeWiiProcessRunner(new WiiProcessRunResult(0, string.Empty, string.Empty)));

        InvalidOperationException exception = Assert.Throws<InvalidOperationException>(() =>
            packager.Package(CreateLayout(), Path.Combine(Path.GetTempPath(), "game.iso"), CancellationToken.None));

        Assert.Contains("wit", exception.Message, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Ensures the packager rejects a configured executable path that does not exist.
    /// </summary>
    [Fact]
    public void Package_WhenExecutablePathDoesNotExist_ThrowsFileNotFoundException() {
        WiiWiimmsIsoToolsImagePackager packager = new(
            new WiiWiimmsIsoToolsOptions(Path.Combine(Path.GetTempPath(), "missing", "wit.exe")),
            new FakeWiiProcessRunner(new WiiProcessRunResult(0, string.Empty, string.Empty)));

        Assert.Throws<FileNotFoundException>(() =>
            packager.Package(CreateLayout(), Path.Combine(Path.GetTempPath(), "game.iso"), CancellationToken.None));
    }

    /// <summary>
    /// Ensures the packager surfaces non-zero Wiimms ISO Tools exits as build failures.
    /// </summary>
    [Fact]
    public void Package_WhenWitReturnsNonZero_ThrowsInvalidOperationException() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wii-wit-tests", Guid.NewGuid().ToString("N"));
        string executablePath = Path.Combine(workingRootPath, "wit.exe");
        Directory.CreateDirectory(workingRootPath);
        File.WriteAllText(executablePath, "fake");

        WiiWiimmsIsoToolsImagePackager packager = new(
            new WiiWiimmsIsoToolsOptions(executablePath),
            new FakeWiiProcessRunner(new WiiProcessRunResult(7, "stdout", "stderr")));

        InvalidOperationException exception = Assert.Throws<InvalidOperationException>(() =>
            packager.Package(CreateLayout(), Path.Combine(workingRootPath, "game.iso"), CancellationToken.None));

        Assert.Contains("exit code", exception.Message, StringComparison.OrdinalIgnoreCase);
        Assert.Contains("stderr", exception.Message, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Ensures the packager accepts a successful Wiimms ISO Tools process when an image artifact is materialized.
    /// </summary>
    [Fact]
    public void Package_WhenWitSucceedsAndImageExists_WritesWiiImageArtifact() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wii-wit-tests", Guid.NewGuid().ToString("N"));
        string executablePath = Path.Combine(workingRootPath, "wit.exe");
        string outputImagePath = Path.Combine(workingRootPath, "game.iso");
        Directory.CreateDirectory(workingRootPath);
        File.WriteAllText(executablePath, "fake");

        WiiWiimmsIsoToolsImagePackager packager = new(
            new WiiWiimmsIsoToolsOptions(executablePath),
            new FakeWiiProcessRunner(new WiiProcessRunResult(0, "ok", string.Empty)),
            static (layout, destinationImagePath) => File.WriteAllText(destinationImagePath, "iso"));

        packager.Package(CreateLayout(), outputImagePath, CancellationToken.None);

        Assert.True(File.Exists(outputImagePath));
    }

    /// <summary>
    /// Ensures the packager forces plain ISO output when invoking <c>wit</c>.
    /// </summary>
    [Fact]
    public void Package_WhenWitRunsDirectly_RequestsIsoOutputMode() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wii-wit-tests", Guid.NewGuid().ToString("N"));
        string executablePath = Path.Combine(workingRootPath, "wit.exe");
        string outputImagePath = Path.Combine(workingRootPath, "game.iso");
        Directory.CreateDirectory(workingRootPath);
        File.WriteAllText(executablePath, "fake");
        FakeWiiProcessRunner processRunner = new(new WiiProcessRunResult(0, "ok", string.Empty));

        WiiWiimmsIsoToolsImagePackager packager = new(
            new WiiWiimmsIsoToolsOptions(executablePath),
            processRunner,
            static (layout, destinationImagePath) => File.WriteAllText(destinationImagePath, "iso"));

        packager.Package(CreateLayout(), outputImagePath, CancellationToken.None);

        Assert.Contains("--iso", processRunner.LastStartInfo.ArgumentList);
        Assert.Equal("COPY", processRunner.LastStartInfo.ArgumentList[0]);
    }

    /// <summary>
    /// Ensures the packager normalizes extracted-disc and image paths to absolute paths before invoking <c>wit</c>.
    /// </summary>
    [Fact]
    public void Package_WhenLayoutUsesRelativePaths_PassesAbsolutePathsToWit() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wii-wit-tests", Guid.NewGuid().ToString("N"));
        string executablePath = Path.Combine(workingRootPath, "wit.exe");
        Directory.CreateDirectory(workingRootPath);
        File.WriteAllText(executablePath, "fake");
        FakeWiiProcessRunner processRunner = new(new WiiProcessRunResult(0, "ok", string.Empty));
        string previousDirectoryPath = Directory.GetCurrentDirectory();

        try {
            Directory.SetCurrentDirectory(workingRootPath);
            WiiWiimmsIsoToolsImagePackager packager = new(
                new WiiWiimmsIsoToolsOptions(executablePath),
                processRunner,
                static (layout, destinationImagePath) => File.WriteAllText(destinationImagePath, "iso"));

            packager.Package(CreateLayout("disc"), ".\\game.iso", CancellationToken.None);

            Assert.Equal(Path.GetFullPath(".\\disc"), processRunner.LastStartInfo.ArgumentList[1]);
            Assert.Equal(Path.GetFullPath(".\\game.iso"), processRunner.LastStartInfo.ArgumentList[2]);
        }
        finally {
            Directory.SetCurrentDirectory(previousDirectoryPath);
            if (Directory.Exists(workingRootPath)) {
                Directory.Delete(workingRootPath, true);
            }
        }
    }

    /// <summary>
    /// Ensures the packager fails early with an actionable diagnostic when the extracted-disc source is missing the required Wii executable.
    /// </summary>
    [Fact]
    public void Package_WhenDiscSourceIsMissingMainDol_ThrowsInvalidOperationException() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wii-wit-tests", Guid.NewGuid().ToString("N"));
        string executablePath = Path.Combine(workingRootPath, "wit.exe");
        Directory.CreateDirectory(workingRootPath);
        File.WriteAllText(executablePath, "fake");

        WiiWiimmsIsoToolsImagePackager packager = new(
            new WiiWiimmsIsoToolsOptions(executablePath),
            new FakeWiiProcessRunner(new WiiProcessRunResult(0, "ok", string.Empty)));

        InvalidOperationException exception = Assert.Throws<InvalidOperationException>(() =>
            packager.Package(CreateLayout(includeMainDol: false), Path.Combine(workingRootPath, "game.iso"), CancellationToken.None));

        Assert.Contains("sys/main.dol", exception.Message, StringComparison.Ordinal);
    }

    /// <summary>
    /// Creates a minimal extracted-disc layout for packager tests.
    /// </summary>
    /// <returns>Minimal extracted-disc layout.</returns>
    static WiiDiscLayoutResult CreateLayout(bool includeMainDol = true) {
        return CreateLayout(Path.Combine(Path.GetTempPath(), "wii-wit-tests", Guid.NewGuid().ToString("N"), "disc"), includeMainDol);
    }

    /// <summary>
    /// Creates a minimal extracted-disc layout rooted at one explicit path for packager tests.
    /// </summary>
    /// <param name="discRootPath">Extracted-disc root to materialize.</param>
    /// <param name="includeMainDol">Whether to stage the required Wii executable.</param>
    /// <returns>Minimal extracted-disc layout.</returns>
    static WiiDiscLayoutResult CreateLayout(string discRootPath, bool includeMainDol = true) {
        string mainDolPath = Path.Combine(discRootPath, "sys", "main.dol");
        string bootPath = Path.Combine(discRootPath, "sys", "boot.bin");
        string apploaderPath = Path.Combine(discRootPath, "sys", "apploader.img");
        string scenePath = Path.Combine(discRootPath, "files", "cooked", "scenes", "rendering", "cube_test.hasset");
        Directory.CreateDirectory(Path.GetDirectoryName(mainDolPath) ?? throw new InvalidOperationException("Main DOL directory path could not be resolved."));
        Directory.CreateDirectory(Path.GetDirectoryName(scenePath) ?? throw new InvalidOperationException("Scene directory path could not be resolved."));
        File.WriteAllText(bootPath, "boot");
        File.WriteAllText(apploaderPath, "apploader");
        if (includeMainDol) {
            File.WriteAllText(mainDolPath, "dol");
        }
        File.WriteAllText(scenePath, "scene");
        return new WiiDiscLayoutResult(
            discRootPath,
            mainDolPath,
            new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) {
                ["cooked/scenes/rendering/cube_test.hasset"] = "files/cooked/scenes/rendering/cube_test.hasset"
            });
    }
}
