using FilesAssetSerializer = helengine.files.AssetSerializer;

namespace helengine.wii.builder.tests;

/// <summary>
/// Verifies Wii-native texture cooking produces prepacked GX payloads from shared-engine RGBA32 textures.
/// </summary>
public sealed class WiiTextureCookerTests {
    /// <summary>
    /// Ensures RGBA32 source textures are converted into big-endian tiled GX RGB5A3 payloads.
    /// </summary>
    [Fact]
    public void CookTexture_WhenUsingRgba32Source_ProducesPrepackedGxRgb5A3Payload() {
        WiiTextureCooker cooker = new WiiTextureCooker();
        TextureAsset sourceTexture = new TextureAsset {
            Id = "Images/Menu/logo.png",
            RuntimeAssetId = 77,
            Width = 1,
            Height = 1,
            ColorFormat = TextureAssetColorFormat.Rgba32,
            AlphaPrecision = TextureAssetAlphaPrecision.A8,
            Colors = [0x00, 0x00, 0xFF, 0xFF],
            PaletteColors = Array.Empty<byte>()
        };

        TextureAsset cookedTexture = cooker.CookTexture(
            sourceTexture,
            new WiiTextureCookSettings(0, "GxRgb5A3", TextureAssetAlphaPrecision.A8));

        Assert.Equal(sourceTexture.Id, cookedTexture.Id);
        Assert.Equal(sourceTexture.RuntimeAssetId, cookedTexture.RuntimeAssetId);
        Assert.Equal(TextureAssetColorFormat.GxRgb5A3, cookedTexture.ColorFormat);
        Assert.Equal(TextureAssetAlphaPrecision.A8, cookedTexture.AlphaPrecision);
        Assert.Equal((ushort)1, cookedTexture.Width);
        Assert.Equal((ushort)1, cookedTexture.Height);
        Assert.Equal(32, cookedTexture.Colors.Length);
        Assert.Equal(0x80, cookedTexture.Colors[0]);
        Assert.Equal(0x1F, cookedTexture.Colors[1]);

        using MemoryStream stream = new MemoryStream();
        FilesAssetSerializer.Serialize(stream, cookedTexture);
        stream.Position = 0;
        TextureAsset roundTripped = Assert.IsType<TextureAsset>(FilesAssetSerializer.Deserialize(stream));
        Assert.Equal(TextureAssetColorFormat.GxRgb5A3, roundTripped.ColorFormat);
        Assert.Equal(32, roundTripped.Colors.Length);
        Assert.Equal(0x80, roundTripped.Colors[0]);
        Assert.Equal(0x1F, roundTripped.Colors[1]);
    }
}
