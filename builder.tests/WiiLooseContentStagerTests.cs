namespace helengine.wii.builder.tests;

/// <summary>
/// Verifies the builder-side loose-content staging flow used by the Wii runtime bootstrap.
/// </summary>
public sealed class WiiLooseContentStagerTests {
    /// <summary>
    /// Ensures staged loose content is copied into the runtime root without changing its relative layout.
    /// </summary>
    [Fact]
    public void Stage_CopiesSourceTreeIntoRuntimeRoot() {
        string tempRootPath = Path.Combine(Path.GetTempPath(), "helengine-wii-builder-tests", Guid.NewGuid().ToString("N"));
        string sourceRootPath = Path.Combine(tempRootPath, "source");
        string runtimeRootPath = Path.Combine(tempRootPath, "runtime");
        string sourceFilePath = Path.Combine(sourceRootPath, "cooked", "scenes", "demodiscmainmenu.hasset");
        string sourceFontPath = Path.Combine(sourceRootPath, "cooked", "fonts", "default.hefont");

        try {
            Directory.CreateDirectory(Path.GetDirectoryName(sourceFilePath) ?? throw new InvalidOperationException("Scene directory was not resolved."));
            Directory.CreateDirectory(Path.GetDirectoryName(sourceFontPath) ?? throw new InvalidOperationException("Font directory was not resolved."));
            File.WriteAllText(sourceFilePath, "scene");
            File.WriteAllText(sourceFontPath, "font");

            new WiiLooseContentStager().Stage(sourceRootPath, runtimeRootPath);

            string stagedFilePath = Path.Combine(runtimeRootPath, "cooked", "scenes", "demodiscmainmenu.hasset");
            string stagedFontPath = Path.Combine(runtimeRootPath, "cooked", "fonts", "default.hefont");
            Assert.True(File.Exists(stagedFilePath));
            Assert.True(File.Exists(stagedFontPath));
            Assert.Equal("scene", File.ReadAllText(stagedFilePath));
            Assert.Equal("font", File.ReadAllText(stagedFontPath));
        }
        finally {
            if (Directory.Exists(tempRootPath)) {
                Directory.Delete(tempRootPath, true);
            }
        }
    }
}
