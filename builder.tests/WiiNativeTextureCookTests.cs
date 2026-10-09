using helengine.editor;
using FilesAssetSerializer = helengine.files.AssetSerializer;

namespace helengine.wii.builder.tests;

/// <summary>Exercises every GX selection, persisted settings and the unchanged legacy RGB5A3 default.</summary>
public sealed class WiiNativeTextureCookTests {
    /// <summary>Supplies all direct, compressed, depth and TLUT selections published by the native catalog.</summary>
    public static IEnumerable<object[]> NativeFormats => GxNativeTextureFormatCatalog.Formats.Select(format => new object[] { format.Id });

    /// <summary>Checks both texture kinds retain their approved default while exposing precisely the native alpha combinations.</summary>
    [Fact]
    public void CapabilitiesKeepLegacyDefaultsAndExposeTwentyNativeSelections() {
        var definition = WiiPlatformDefinitionFactory.Create();
        Assert.Equal(20, GxNativeTextureFormatCatalog.Formats.Length);
        Assert.Equal(2, definition.AssetCookCapabilities.Length);
        foreach (var capability in definition.AssetCookCapabilities) {
            Assert.Equal("{\"maxResolution\":0,\"colorFormat\":\"GxRgb5A3\",\"alphaPrecision\":\"A8\"}", capability.DefaultSerializedPlatformSettings);
            Assert.Equal(21, capability.TextureFormatCapabilities.SupportedColorFormatIds.Length);
            foreach (var format in GxNativeTextureFormatCatalog.Formats) {
                Assert.Contains(format.Id, capability.TextureFormatCapabilities.SupportedColorFormatIds);
                Assert.Equal(format.SupportedAlphaPrecisions,
                    capability.TextureFormatCapabilities.SupportedCombinations.Where(item => item.ColorFormatId == format.Id).Select(item => item.AlphaPrecision));
            }
        }
        Assert.Contains(definition.AssetCookCapabilities, capability => capability.SourceAssetKind == "font-atlas-texture");
    }

    /// <summary>Checks every selection cooks through private RGBA/quantization and preserves native bytes through serialization.</summary>
    [Theory]
    [MemberData(nameof(NativeFormats))]
    public void EveryNativeFormatCooksAndSerializesWithoutMutatingSource(string id) {
        GxNativeTextureFormatCatalog.TryGetFormat(id, out var format);
        using TextureAsset source = Source(5, 3);
        byte[] original = (byte[])source.Colors.Clone();
        var settings = WiiTextureCookSettings.Parse(Settings(id, format.DefaultAlphaPrecision).Serialize());
        using TextureAsset cooked = new WiiTextureCooker().CookTexture(source, settings);
        Assert.Equal(TextureAssetColorFormat.GxNative, cooked.ColorFormat);
        Assert.Equal(source.Id, cooked.Id);
        Assert.Equal(source.RuntimeAssetId, cooked.RuntimeAssetId);
        Assert.Equal(source.AuthoringAssetId, cooked.AuthoringAssetId);
        Assert.Equal(source.FormerAuthoringAssetIds, cooked.FormerAuthoringAssetIds);
        Assert.NotSame(source.FormerAuthoringAssetIds, cooked.FormerAuthoringAssetIds);
        Assert.Equal(original, source.Colors);
        Assert.Equal(TextureAssetAlphaPrecision.A8, source.AlphaPrecision);
        using TextureAsset read = Assert.IsType<TextureAsset>(FilesAssetSerializer.DeserializeFromBytes(FilesAssetSerializer.SerializeToBytes(cooked)));
        Assert.Equal(cooked.Colors, read.Colors);
        Assert.Equal(cooked.PaletteColors, read.PaletteColors);
        var layout = GxNativeTextureCodec.ReadLayout(read);
        Assert.Equal(id, layout.Format.Id);
        Assert.Equal(format.GetPixelByteLength(5, 3), layout.TexelLength);
        Assert.Equal(5, layout.RealWidth);
        Assert.Equal(3, layout.RealHeight);
        Assert.Equal(48 + layout.TexelLength, read.Colors.Length);
        if (format.IsIndexed) {
            Assert.InRange(layout.PaletteEntryCount, 16, 256);
            Assert.Equal(layout.PaletteEntryCount * 2, read.PaletteColors.Length);
        } else {
            Assert.Equal(0, layout.PaletteEntryCount);
        }
        Assert.Equal(5 * 3 * 4, TextureAssetPixelCodec.DecodeToRgba32(read).Length);
    }

    /// <summary>Preserves the established rounded RGB5A3 conversion and alpha224 branch for legacy authored settings.</summary>
    [Theory]
    [InlineData(128, 128, 128, 224, 0xC2, 0x10)]
    [InlineData(128, 128, 128, 223, 0x68, 0x88)]
    [InlineData(0, 0, 255, 255, 0x80, 0x1F)]
    public void LegacyGoldenBytesRemainUnchanged(byte red, byte green, byte blue, byte alpha, byte high, byte low) {
        using TextureAsset source = Source(1, 1);
        source.Colors = [red, green, blue, alpha];
        using TextureAsset cooked = new WiiTextureCooker().CookTexture(source, Settings("GxRgb5A3", TextureAssetAlphaPrecision.A8));
        Assert.Equal(TextureAssetColorFormat.GxRgb5A3, cooked.ColorFormat);
        Assert.Equal(32, cooked.Colors.Length);
        for (int index = 0; index < cooked.Colors.Length; index += 2) {
            Assert.Equal(high, cooked.Colors[index]);
            Assert.Equal(low, cooked.Colors[index + 1]);
        }
    }

    /// <summary>Checks the legacy prepacked path copies storage instead of decoding or mutating it.</summary>
    [Fact]
    public void LegacyPrepackedStorageIsRetainedIndependently() {
        using TextureAsset source = Source(1, 1);
        using TextureAsset packed = new WiiTextureCooker().CookTexture(source, Settings("GxRgb5A3", TextureAssetAlphaPrecision.A8));
        using TextureAsset copied = new WiiTextureCooker().CookTexture(packed, Settings("GxRgb5A3", TextureAssetAlphaPrecision.A8));
        Assert.Equal(packed.Colors, copied.Colors);
        Assert.NotSame(packed.Colors, copied.Colors);
        Assert.Equal(packed.AuthoringAssetId, copied.AuthoringAssetId);
    }

    /// <summary>Uses historical headerless tiles as native direct or indexed sources without changing their authored bytes or identity.</summary>
    [Theory]
    [InlineData("Gx.RGBA8", TextureAssetAlphaPrecision.A8)]
    [InlineData("Gx.CI8.TLUT.RGB5A3", TextureAssetAlphaPrecision.A3)]
    public void LegacyPrepackedSourceCanBeRecookedIntoNativeStorage(string id, TextureAssetAlphaPrecision alpha) {
        using TextureAsset source = Source(1, 1);
        source.Colors = [0, 0, 255, 255];
        WiiTextureCooker cooker = new();
        using TextureAsset packed = cooker.CookTexture(source, Settings("GxRgb5A3", TextureAssetAlphaPrecision.A8));
        byte[] original = (byte[])packed.Colors.Clone();
        using TextureAsset native = cooker.CookTexture(packed, Settings(id, alpha));
        Assert.Equal(id, GxNativeTextureCodec.ReadLayout(native).Format.Id);
        Assert.Equal(new byte[] { 0, 0, 255, 255 }, GxNativeTextureCodec.Decode(native));
        Assert.Equal(original, packed.Colors);
        Assert.Equal(packed.AuthoringAssetId, native.AuthoringAssetId);
    }

    /// <summary>Resizes logical RGBA dimensions before native edge-tile allocation while retaining the source dimensions.</summary>
    [Fact]
    public void NativeResizeKeepsAspectAndAuthoredSource() {
        using TextureAsset source = Source(8, 4);
        var settings = new WiiTextureCookSettings(4, "Gx.RGBA8", TextureAssetAlphaPrecision.A8);
        using TextureAsset cooked = new WiiTextureCooker().CookTexture(source, settings);
        Assert.Equal((ushort)4, cooked.Width);
        Assert.Equal((ushort)2, cooked.Height);
        Assert.Equal((ushort)8, source.Width);
        Assert.Equal((ushort)4, source.Height);
        Assert.Equal(4 * 2 * 4, GxNativeTextureCodec.Decode(cooked).Length);
    }

    /// <summary>Keeps the uncapped default unchanged while rejecting dimensions GX cannot fetch, and permits an explicit authored cap.</summary>
    [Fact]
    public void LegacyOversizedSourceIsRejectedAfterApplyingExplicitResize() {
        using TextureAsset source = Source(2048, 1);
        Assert.Throws<InvalidOperationException>(() => new WiiTextureCooker().CookTexture(source, Settings("GxRgb5A3", TextureAssetAlphaPrecision.A8)));
        using TextureAsset resized = new WiiTextureCooker().CookTexture(source, new WiiTextureCookSettings(1024, "GxRgb5A3", TextureAssetAlphaPrecision.A8));
        Assert.Equal((ushort)1024, resized.Width);
        Assert.Equal((ushort)2048, source.Width);
    }

    /// <summary>Rejects malformed, missing, duplicate, numeric or unknown persisted settings before source decoding.</summary>
    [Theory]
    [InlineData("{}")]
    [InlineData("[]")]
    [InlineData("{\"maxResolution\":0,\"colorFormat\":\"Gx.RGBA8\",\"alphaPrecision\":\"3\"}")]
    [InlineData("{\"maxResolution\":0,\"colorFormat\":\"Gx.Unknown\",\"alphaPrecision\":\"A8\"}")]
    [InlineData("{\"maxResolution\":0,\"colorFormat\":\"Gx.RGBA8\",\"alphaPrecision\":\"A8\",\"unknown\":0}")]
    [InlineData("{\"maxResolution\":0,\"maxResolution\":1,\"colorFormat\":\"Gx.RGBA8\",\"alphaPrecision\":\"A8\"}")]
    public void InvalidSettingsAreRejected(string json) {
        Assert.ThrowsAny<Exception>(() => WiiTextureCookSettings.Parse(json));
    }

    /// <summary>Checks alpha policies and selected indexing strategies cannot silently become another format or quantizer.</summary>
    [Theory]
    [InlineData("Gx.RGB565", TextureAssetAlphaPrecision.A8, "")]
    [InlineData("Gx.CMPR", TextureAssetAlphaPrecision.A8, "")]
    [InlineData("Gx.RGB5A3", TextureAssetAlphaPrecision.A8, "")]
    [InlineData("Gx.RGBA8", TextureAssetAlphaPrecision.A8, "QuantizedIndexed")]
    [InlineData("Gx.CI8.TLUT.IA8", TextureAssetAlphaPrecision.A8, "UnknownQuantizer")]
    [InlineData("GxRgb5A3", TextureAssetAlphaPrecision.A2, "")]
    [InlineData("GxRgb5A3", TextureAssetAlphaPrecision.A3, "")]
    public void UnsupportedPolicyIsRejected(string id, TextureAssetAlphaPrecision alpha, string indexing) {
        Assert.Throws<InvalidOperationException>(() => new WiiTextureCookSettings(0, id, alpha, indexing));
    }

    /// <summary>Chooses the explicit shared quantizer only when the native format stores palette indices.</summary>
    public static WiiTextureCookSettings Settings(string id, TextureAssetAlphaPrecision alpha) {
        GxNativeTextureFormatCatalog.TryGetFormat(id, out var format);
        return new WiiTextureCookSettings(0, id, alpha, format != null && format.IsIndexed ? TextureAssetIndexingMethod.QuantizedIndexed.ToString() : "");
    }

    /// <summary>Creates deterministic transparent black and opaque white texels with authored identity aliases.</summary>
    public static TextureAsset Source(ushort width, ushort height) {
        byte[] colors = new byte[width * height * 4];
        for (int index = 0; index < width * height; index++) {
            byte channel = index % 2 == 0 ? (byte)0 : (byte)255;
            colors[index * 4] = channel; colors[index * 4 + 1] = channel;
            colors[index * 4 + 2] = channel; colors[index * 4 + 3] = channel;
        }
        return new TextureAsset {
            Id = "textures/authored", RuntimeAssetId = 123, AuthoringAssetId = "authored-id", FormerAuthoringAssetIds = ["old-id"],
            Width = width, Height = height, ColorFormat = TextureAssetColorFormat.Rgba32, AlphaPrecision = TextureAssetAlphaPrecision.A8,
            Colors = colors, PaletteColors = Array.Empty<byte>()
        };
    }
}
