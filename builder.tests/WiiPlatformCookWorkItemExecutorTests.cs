using helengine.baseplatform.Manifest;
using FilesAssetSerializer = helengine.files.AssetSerializer;
using FilesFontAssetBinarySerializer = helengine.files.FontAssetBinarySerializer;

namespace helengine.wii.builder.tests;

/// <summary>
/// Verifies the Wii builder executes editor-emitted platform cook work items into staged runtime artifacts.
/// </summary>
public sealed class WiiPlatformCookWorkItemExecutorTests {
    /// <summary>
    /// Ensures a texture work item imports one source PNG and writes a prepacked Wii texture asset.
    /// </summary>
    [Fact]
    public void Execute_WhenUsingTextureWorkItem_WritesCookedTextureAssetIntoStagingRoot() {
        string workspacePath = Path.Combine(Path.GetTempPath(), "wii-platform-cook-work-item-tests", Guid.NewGuid().ToString("N"));
        string projectRootPath = Path.Combine(workspacePath, "project");
        string stagingRootPath = Path.Combine(workspacePath, "staging");
        string sourceTexturePath = Path.Combine(projectRootPath, "assets", "Images", "Menu", "logo.png");
        string outputRelativePath = "cooked/imported/logo-runtime.hasset";

        try {
            Directory.CreateDirectory(Path.GetDirectoryName(sourceTexturePath) ?? throw new InvalidOperationException("Texture source directory path could not be resolved."));
            File.WriteAllBytes(sourceTexturePath, CreateSinglePixelPngBytes());

            WiiPlatformCookWorkItemExecutor executor = new WiiPlatformCookWorkItemExecutor();
            executor.Execute(
                [
                    new PlatformCookWorkItem(
                        "wii:texture:cooked/imported/logo-runtime.hasset",
                        "Images/Menu/logo.png",
                        "texture",
                        "wii",
                        "runtime-texture",
                        outputRelativePath,
                        "runtime-texture:cooked/imported/logo-runtime.hasset",
                        "sha256:source",
                        "sha256:settings",
                        "{\"maxResolution\":0,\"colorFormat\":\"GxRgb5A3\",\"alphaPrecision\":\"A8\"}",
                        [new PlatformCookWorkItemMetadata("source-asset-id", "Images/Menu/logo.png")])
                ],
                projectRootPath,
                stagingRootPath);

            string outputPath = Path.Combine(stagingRootPath, "cooked", "imported", "logo-runtime.hasset");
            Assert.True(File.Exists(outputPath));

            using FileStream stream = new FileStream(outputPath, FileMode.Open, FileAccess.Read, FileShare.Read);
            TextureAsset cookedTexture = Assert.IsType<TextureAsset>(FilesAssetSerializer.Deserialize(stream));
            Assert.Equal(TextureAssetColorFormat.GxRgb5A3, cookedTexture.ColorFormat);
            Assert.Equal(32, cookedTexture.Colors.Length);
        } finally {
            if (Directory.Exists(workspacePath)) {
                Directory.Delete(workspacePath, recursive: true);
            }
        }
    }

    /// <summary>
    /// Ensures a font-atlas work item rewrites the serialized font atlas texture into a prepacked Wii payload.
    /// </summary>
    [Fact]
    public void Execute_WhenUsingFontAtlasTextureWorkItem_WritesCookedFontAssetIntoStagingRoot() {
        string workspacePath = Path.Combine(Path.GetTempPath(), "wii-platform-cook-work-item-tests", Guid.NewGuid().ToString("N"));
        string projectRootPath = Path.Combine(workspacePath, "project");
        string stagingRootPath = Path.Combine(workspacePath, "staging");
        string sourceFontPath = Path.Combine(projectRootPath, "assets", "Fonts", "Body.hefont");
        string outputRelativePath = "cooked/fonts/Body.ps2tex";

        try {
            Directory.CreateDirectory(Path.GetDirectoryName(sourceFontPath) ?? throw new InvalidOperationException("Font source directory path could not be resolved."));
            WriteSourceFontAsset(sourceFontPath);

            WiiPlatformCookWorkItemExecutor executor = new WiiPlatformCookWorkItemExecutor();
            executor.Execute(
                [
                    new PlatformCookWorkItem(
                        "wii:font-atlas:cooked/fonts/Body.ps2tex",
                        "Fonts/Body.hefont",
                        "font-atlas-texture",
                        "wii",
                        "runtime-texture",
                        outputRelativePath,
                        "runtime-texture:cooked/fonts/Body.ps2tex",
                        "sha256:source",
                        "sha256:settings",
                        "{\"maxResolution\":0,\"colorFormat\":\"GxRgb5A3\",\"alphaPrecision\":\"A8\"}",
                        [new PlatformCookWorkItemMetadata("source-asset-id", "Fonts/Body.hefont")])
                ],
                projectRootPath,
                stagingRootPath);

            string outputPath = Path.Combine(stagingRootPath, "cooked", "fonts", "Body.ps2tex");
            Assert.True(File.Exists(outputPath));

            using FileStream stream = new FileStream(outputPath, FileMode.Open, FileAccess.Read, FileShare.Read);
            TextureAsset cookedTexture = Assert.IsType<TextureAsset>(FilesAssetSerializer.Deserialize(stream));
            Assert.Equal(TextureAssetColorFormat.GxRgb5A3, cookedTexture.ColorFormat);
            Assert.Equal(32, cookedTexture.Colors.Length);
        } finally {
            if (Directory.Exists(workspacePath)) {
                Directory.Delete(workspacePath, recursive: true);
            }
        }
    }

    /// <summary>
    /// Creates one 1x1 opaque blue PNG payload used by the texture work-item test.
    /// </summary>
    /// <returns>Serialized 1x1 PNG bytes.</returns>
    static byte[] CreateSinglePixelPngBytes() {
        const string base64 =
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADUlEQVR4nGNgYPj/HwADAgH/5ncLrgAAAABJRU5ErkJggg==";
        return Convert.FromBase64String(base64);
    }

    /// <summary>
    /// Writes one minimal serialized source font asset for font-atlas cook testing.
    /// </summary>
    /// <param name="destinationPath">Absolute font-asset output path.</param>
    static void WriteSourceFontAsset(string destinationPath) {
        FontAsset fontAsset = new FontAsset(
            new FontInfo("Body", 16, 8),
            null,
            new Dictionary<char, FontChar>(),
            16,
            1,
            1) {
            SourceTextureAsset = new TextureAsset {
                Id = "Fonts/Body.hefont",
                Width = 1,
                Height = 1,
                ColorFormat = TextureAssetColorFormat.Rgba32,
                AlphaPrecision = TextureAssetAlphaPrecision.A8,
                Colors = [0xFF, 0x00, 0x00, 0xFF],
                PaletteColors = Array.Empty<byte>()
            }
        };

        using FileStream stream = new FileStream(destinationPath, FileMode.Create, FileAccess.Write, FileShare.None);
        FilesFontAssetBinarySerializer.Serialize(stream, fontAsset);
    }
}
