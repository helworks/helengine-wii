using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using helengine.wii.builder.tests.Builders;
using FilesAssetSerializer = helengine.files.AssetSerializer;
using FilesFontAssetBinarySerializer = helengine.files.FontAssetBinarySerializer;

namespace helengine.wii.builder.tests;

/// <summary>
/// Verifies the packaged Wii builder workspace emits staged disc-layout outputs when supplied fake native-build collaborators.
/// </summary>
public sealed class WiiPackagedBuildWorkspaceTests {
    /// <summary>
    /// Ensures the packaged builder stages cooked artifacts, writes the runtime manifest, builds the DOL, writes the extracted disc layout, and emits the packaged image artifact.
    /// </summary>
    [Fact]
    public async Task BuildPackagedAsync_WritesDiscRootAndImageArtifact() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wii-packaged-build-tests", Guid.NewGuid().ToString("N"));
        string outputRootPath = Path.Combine(workingRootPath, "out");
        string sourceRootPath = Path.Combine(workingRootPath, "project");
        string generatedCoreRootPath = Path.Combine(workingRootPath, "generated-core");
        string sceneSourcePath = Path.Combine(sourceRootPath, "cooked", "scenes", "DemoDiscMainMenu.hasset");
        string fontSourcePath = Path.Combine(sourceRootPath, "cooked", "fonts", "DemoDiscBody.hefont");
        string defaultFontSourcePath = Path.Combine(sourceRootPath, "cooked", "fonts", "default.hefont");
        string previousDirectory = Directory.GetCurrentDirectory();

        try {
            Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath) ?? throw new InvalidOperationException("Scene directory path could not be resolved."));
            Directory.CreateDirectory(Path.GetDirectoryName(fontSourcePath) ?? throw new InvalidOperationException("Font directory path could not be resolved."));
            Directory.CreateDirectory(Path.GetDirectoryName(defaultFontSourcePath) ?? throw new InvalidOperationException("Default font directory path could not be resolved."));
            Directory.CreateDirectory(generatedCoreRootPath);
            await File.WriteAllTextAsync(sceneSourcePath, "scene");
            WriteFontAsset(fontSourcePath, CreateExternalCookedAtlasFontAsset("Fonts/DemoDiscBody", "cooked/fonts/DemoDiscBody.ps2tex"));
            WriteFontAsset(defaultFontSourcePath, CreateEmbeddedAtlasFontAsset("fonts/default.hefont"));
            Directory.SetCurrentDirectory(sourceRootPath);

            PlatformBuildManifest manifest = new(
                1,
                "project",
                "1.0.0",
                "1.0.0",
                "wii",
                "1.0.0",
                "Scenes/DemoDiscMainMenu.helen",
                [
                    new PlatformBuildScene(
                        "Scenes/DemoDiscMainMenu.helen",
                        "Demo Disc Main Menu",
                        "Scenes/DemoDiscMainMenu.helen",
                        [],
                        [new KeyValuePair<string, string>("cooked-relative-path", "cooked/scenes/DemoDiscMainMenu.hasset")])
                ],
                Array.Empty<PlatformBuildAsset>(),
                [
                    new PlatformBuildArtifact("cooked/scenes/DemoDiscMainMenu.hasset", "scene-content-hash", "scene", "wii-default"),
                    new PlatformBuildArtifact("cooked/fonts/DemoDiscBody.hefont", "font-content-hash", "font", "wii-default"),
                    new PlatformBuildArtifact("cooked/fonts/default.hefont", "default-font-content-hash", "font", "wii-default")
                ],
                Array.Empty<PlatformBuildCodeModule>(),
                Array.Empty<PlatformArtifactPlacement>(),
                new PlatformContainerWritePlan("wii-disc-layout", Array.Empty<PlatformContainerArtifact>()));

            PlatformBuildRequest request = new(
                manifest,
                [new PlatformBuildTargetVariant("wii-default", "wii", "wii", "wii-default")],
                [new PlatformCookProfile(
                    "wii-default",
                    "Wii Default",
                    new PlatformCookProfileCapabilities(
                        "wii",
                        "raw",
                        "rgba",
                        "wii-scene-v1",
                        PlatformSerializationEndianness.BigEndian))],
                outputRootPath,
                Path.Combine(workingRootPath, "tmp"),
                string.Empty,
                string.Empty,
                string.Empty,
                null,
                null,
                null,
                generatedCoreRootPath);

            RecordingProgressReporter progressReporter = new();
            RecordingDiagnosticReporter diagnosticReporter = new();
            FakeWiiNativeBuildExecutor nativeBuildExecutor = new();
            FakeWiiImagePackager imagePackager = new();

            PlatformBuildReport report = await WiiBuildWorkspace.BuildPackagedAsync(
                request,
                progressReporter,
                diagnosticReporter,
                CancellationToken.None,
                nativeBuildExecutor,
                imagePackager,
                new WiiDiscSystemAreaOptions("HELWII", "helengine-wii"));

            Assert.True(report.Succeeded);
            Assert.Empty(diagnosticReporter.Diagnostics);
            Assert.True(File.Exists(Path.Combine(outputRootPath, "wii-build-phase.txt")));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "native", WiiNativeApploaderTemplatePathResolver.FileName)));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "sys", "main.dol")));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "files", "cooked", "scenes", "DemoDiscMainMenu.hasset")));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "files", "cooked", "fonts", "DemoDiscBody.hefont")));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "files", "cooked", "fonts", "default.hefont")));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "files", "cooked", "fonts", "default.ps2tex")));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "sys", "boot.bin")));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "sys", "bi2.bin")));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "sys", "apploader.img")));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "setup.txt")));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "game.iso")));
            Assert.True(File.Exists(Path.Combine(generatedCoreRootPath, "runtime", "wii_runtime_scene_manifest.hpp")));
            string runtimeManifestSource = File.ReadAllText(Path.Combine(generatedCoreRootPath, "runtime", "wii_runtime_scene_manifest.inl"));
            Assert.Contains("\"files/cooked/scenes/demodiscmainmenu.hasset\"", runtimeManifestSource, StringComparison.Ordinal);

            using FileStream defaultFontStream = new FileStream(Path.Combine(outputRootPath, "disc", "files", "cooked", "fonts", "default.hefont"), FileMode.Open, FileAccess.Read, FileShare.Read);
            FontAsset stagedDefaultFont = FilesFontAssetBinarySerializer.Deserialize(defaultFontStream);
            Assert.Null(stagedDefaultFont.SourceTextureAsset);
            Assert.Equal("cooked/fonts/default.ps2tex", stagedDefaultFont.CookedAtlasTextureRelativePath);

            using FileStream defaultAtlasStream = new FileStream(Path.Combine(outputRootPath, "disc", "files", "cooked", "fonts", "default.ps2tex"), FileMode.Open, FileAccess.Read, FileShare.Read);
            TextureAsset stagedDefaultAtlasTexture = Assert.IsType<TextureAsset>(FilesAssetSerializer.Deserialize(defaultAtlasStream));
            Assert.Equal(TextureAssetColorFormat.GxRgb5A3, stagedDefaultAtlasTexture.ColorFormat);
        } finally {
            Directory.SetCurrentDirectory(previousDirectory);
            if (Directory.Exists(workingRootPath)) {
                Directory.Delete(workingRootPath, recursive: true);
            }
        }
    }

    /// <summary>
    /// Ensures the packaged builder stages the builder-owned Wii system files required by the real <c>wit</c> compose path.
    /// </summary>
    [Fact]
    public async Task BuildPackagedAsync_WithSystemAreaOptions_WritesRequiredWiiSystemFiles() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wii-packaged-build-tests", Guid.NewGuid().ToString("N"));
        string outputRootPath = Path.Combine(workingRootPath, "out");
        string sourceRootPath = Path.Combine(workingRootPath, "project");
        string generatedCoreRootPath = Path.Combine(workingRootPath, "generated-core");
        string sceneSourcePath = Path.Combine(sourceRootPath, "cooked", "scenes", "DemoDiscMainMenu.hasset");
        string previousDirectory = Directory.GetCurrentDirectory();

        try {
            Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath) ?? throw new InvalidOperationException("Scene directory path could not be resolved."));
            Directory.CreateDirectory(generatedCoreRootPath);
            await File.WriteAllTextAsync(sceneSourcePath, "scene");
            Directory.SetCurrentDirectory(sourceRootPath);

            PlatformBuildManifest manifest = new(
                1,
                "project",
                "1.0.0",
                "1.0.0",
                "wii",
                "1.0.0",
                "Scenes/DemoDiscMainMenu.helen",
                [
                    new PlatformBuildScene(
                        "Scenes/DemoDiscMainMenu.helen",
                        "Demo Disc Main Menu",
                        "Scenes/DemoDiscMainMenu.helen",
                        [],
                        [new KeyValuePair<string, string>("cooked-relative-path", "cooked/scenes/DemoDiscMainMenu.hasset")])
                ],
                Array.Empty<PlatformBuildAsset>(),
                [
                    new PlatformBuildArtifact("cooked/scenes/DemoDiscMainMenu.hasset", "scene-content-hash", "scene", "wii-default")
                ],
                Array.Empty<PlatformBuildCodeModule>(),
                Array.Empty<PlatformArtifactPlacement>(),
                new PlatformContainerWritePlan("wii-disc-layout", Array.Empty<PlatformContainerArtifact>()));

            PlatformBuildRequest request = new(
                manifest,
                [new PlatformBuildTargetVariant("wii-default", "wii", "wii", "wii-default")],
                [new PlatformCookProfile(
                    "wii-default",
                    "Wii Default",
                    new PlatformCookProfileCapabilities(
                        "wii",
                        "raw",
                        "rgba",
                        "wii-scene-v1",
                        PlatformSerializationEndianness.BigEndian))],
                outputRootPath,
                Path.Combine(workingRootPath, "tmp"),
                string.Empty,
                string.Empty,
                string.Empty,
                null,
                null,
                null,
                generatedCoreRootPath);

            PlatformBuildReport report = await WiiBuildWorkspace.BuildPackagedAsync(
                request,
                new RecordingProgressReporter(),
                new RecordingDiagnosticReporter(),
                CancellationToken.None,
                new FakeWiiNativeBuildExecutor(),
                new FakeWiiImagePackager(),
                new WiiDiscSystemAreaOptions("HELWII", "helengine-wii"));

            Assert.True(report.Succeeded);
            Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "setup.txt")));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "sys", "boot.bin")));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "sys", "bi2.bin")));
            Assert.True(File.Exists(Path.Combine(outputRootPath, "disc", "sys", "apploader.img")));
            Assert.False(File.Exists(Path.Combine(outputRootPath, "disc", "cert.bin")));
            Assert.False(File.Exists(Path.Combine(outputRootPath, "disc", "ticket.bin")));
            Assert.False(File.Exists(Path.Combine(outputRootPath, "disc", "tmd.bin")));
        } finally {
            Directory.SetCurrentDirectory(previousDirectory);
            if (Directory.Exists(workingRootPath)) {
                Directory.Delete(workingRootPath, recursive: true);
            }
        }
    }

    /// <summary>
    /// Creates one packaged font asset that still embeds one RGBA32 atlas texture.
    /// </summary>
    /// <param name="fontAssetId">Identifier assigned to the embedded source texture.</param>
    /// <returns>Packaged font asset with one embedded atlas payload.</returns>
    static FontAsset CreateEmbeddedAtlasFontAsset(string fontAssetId) {
        return new FontAsset(
            new FontInfo("Default", 16, 8),
            null,
            new Dictionary<char, FontChar>(),
            16,
            1,
            1) {
            SourceTextureAsset = new TextureAsset {
                Id = fontAssetId,
                Width = 1,
                Height = 1,
                ColorFormat = TextureAssetColorFormat.Rgba32,
                AlphaPrecision = TextureAssetAlphaPrecision.A8,
                Colors = [0xFF, 0x00, 0x00, 0xFF],
                PaletteColors = Array.Empty<byte>()
            }
        };
    }

    /// <summary>
    /// Creates one packaged font asset that already resolves its atlas through one external cooked texture path.
    /// </summary>
    /// <param name="fontId">Identifier assigned to the font asset.</param>
    /// <param name="cookedAtlasTextureRelativePath">Cooked relative path of the staged atlas texture payload.</param>
    /// <returns>Packaged font asset with one external atlas texture reference.</returns>
    static FontAsset CreateExternalCookedAtlasFontAsset(string fontId, string cookedAtlasTextureRelativePath) {
        return new FontAsset(
            new FontInfo(fontId, 16, 8),
            null,
            new Dictionary<char, FontChar>(),
            16,
            1,
            1) {
            CookedAtlasTextureRelativePath = cookedAtlasTextureRelativePath
        };
    }

    /// <summary>
    /// Serializes one font asset to the supplied cooked output path.
    /// </summary>
    /// <param name="path">Cooked output path to write.</param>
    /// <param name="fontAsset">Serialized font asset payload.</param>
    static void WriteFontAsset(string path, FontAsset fontAsset) {
        string directoryPath = Path.GetDirectoryName(path) ?? throw new InvalidOperationException("Font output directory path could not be resolved.");
        Directory.CreateDirectory(directoryPath);
        using FileStream stream = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None);
        FilesFontAssetBinarySerializer.Serialize(stream, fontAsset);
    }

    /// <summary>
    /// Writes one file beneath the supplied root path.
    /// </summary>
    /// <param name="rootPath">Root path that owns the file.</param>
    /// <param name="relativePath">Relative file path to write beneath the supplied root.</param>
    /// <param name="contents">String contents to write.</param>
    static void WriteFile(string rootPath, string relativePath, string contents) {
        string fullPath = Path.Combine(rootPath, relativePath);
        string directoryPath = Path.GetDirectoryName(fullPath) ?? throw new InvalidOperationException("Output directory path could not be resolved.");
        Directory.CreateDirectory(directoryPath);
        File.WriteAllText(fullPath, contents);
    }
}
