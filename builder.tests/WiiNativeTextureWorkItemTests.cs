using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using helengine.wii.builder.tests.Builders;
using FilesAssetSerializer = helengine.files.AssetSerializer;

namespace helengine.wii.builder.tests;

/// <summary>Checks real texture and atlas cooking plus complete batch preflight before resetting a packaged staging tree.</summary>
[Collection("WiiTextureStaging")]
public sealed class WiiNativeTextureWorkItemTests : IDisposable {
    /// <summary>Loads material bytes with the exact target policy instead of the managed host's policy.</summary>
    [Theory]
    [InlineData(16)]
    [InlineData(32)]
    [InlineData(64)]
    public void MaterialPayloadLoadsWithTargetNumericPolicy(int bits) {
        var policy = new NumericPrecisionPolicy(bits, bits);
        var request = new PlatformMaterialCookRequest("material", "materials/sample.hasset", "wii", "wii-default", "wii-default",
            WiiMaterialSchemaIds.StandardTexturedSchemaId, new Dictionary<string, string>(), numericPolicy: policy);
        var result = new WiiMaterialCooker().Cook(request);
        using var stream = new MemoryStream(result.CookedMaterialBytes);
        Assert.IsType<PlatformMaterialAsset>(PackagedAssetBinarySerializer.Deserialize(stream, policy));
    }

    /// <summary>Uses one fixed fixture tree beneath the explicit dotnet artifacts directory.</summary>
    readonly string Root = Path.Combine(AppContext.BaseDirectory, "gx-texture-work-items");
    /// <summary>Retains the caller's repository override while fixtures run from relocated artifacts.</summary>
    readonly string PreviousRepositoryRoot = Environment.GetEnvironmentVariable("HELENGINE_WII_REPOSITORY_ROOT");
    /// <summary>Retains the package source directory selected temporarily by managed staging regression tests.</summary>
    readonly string PreviousDirectory = Directory.GetCurrentDirectory();
    /// <summary>Gets the independent source serialized by the real files serializer.</summary>
    string Source => Path.Combine(Root, "source.hetex");
    /// <summary>Gets the existing packaged staging tree whose contents must survive preflight failure.</summary>
    string Stage => Path.Combine(Root, "working", "staged-content");

    /// <summary>Creates authored source data, generated-root metadata and an existing stage marker.</summary>
    public WiiNativeTextureWorkItemTests() {
        Environment.SetEnvironmentVariable("HELENGINE_WII_REPOSITORY_ROOT", ResolveRepositoryRoot());
        Directory.CreateDirectory(Stage);
        Directory.CreateDirectory(Path.Combine(Root, "generated-core"));
        using TextureAsset source = WiiNativeTextureCookTests.Source(4, 2);
        File.WriteAllBytes(Source, FilesAssetSerializer.SerializeToBytes(source));
        File.WriteAllText(Path.Combine(Stage, "previous.txt"), "previous");
    }

    /// <summary>Cooks all native selections for both texture and atlas work-item kinds using real source, codec and serialization paths.</summary>
    [Fact]
    public void EveryNativeSelectionCooksTextureAndFontAtlasWorkItems() {
        byte[] original = File.ReadAllBytes(Source);
        List<PlatformCookWorkItem> items = [];
        for (int index = 0; index < GxNativeTextureFormatCatalog.Formats.Length; index++) {
            var format = GxNativeTextureFormatCatalog.Formats[index];
            items.Add(Item($"textures/{index}.hetex", format.Id));
            items.Add(Item($"fonts/{index}.hetex", format.Id, "font-atlas-texture"));
        }
        new WiiPlatformCookWorkItemExecutor().Execute(items.ToArray(), Root, Stage);
        foreach (var item in items) {
            using TextureAsset cooked = Assert.IsType<TextureAsset>(FilesAssetSerializer.DeserializeFromBytes(File.ReadAllBytes(Path.Combine(Stage, item.OutputRelativePath))));
            Assert.Equal(TextureAssetColorFormat.GxNative, cooked.ColorFormat);
            Assert.Equal(item.OutputLogicalArtifactId, cooked.Id);
            Assert.NotEqual(0ul, cooked.RuntimeAssetId);
            Assert.Equal(4 * 2 * 4, TextureAssetPixelCodec.DecodeToRgba32(cooked).Length);
        }
        Assert.Equal(original, File.ReadAllBytes(Source));
    }

    /// <summary>Checks the unchanged atlas default produces the legacy tag4 payload without altering its source.</summary>
    [Fact]
    public void LegacyFontAtlasDefaultStillCooks() {
        var capability = Assert.Single(WiiPlatformDefinitionFactory.Create().AssetCookCapabilities, item => item.SourceAssetKind == "font-atlas-texture");
        var item = new PlatformCookWorkItem("atlas", Source, capability.SourceAssetKind, "wii", capability.TargetArtifactKind, "fonts/legacy.hetex", "fonts/legacy", "source", "settings", capability.DefaultSerializedPlatformSettings, []);
        new WiiPlatformCookWorkItemExecutor().Execute([item], Root, Stage);
        using TextureAsset cooked = Assert.IsType<TextureAsset>(FilesAssetSerializer.DeserializeFromBytes(File.ReadAllBytes(Path.Combine(Stage, item.OutputRelativePath))));
        Assert.Equal(TextureAssetColorFormat.GxRgb5A3, cooked.ColorFormat);
        Assert.Equal(TextureAssetAlphaPrecision.A8, cooked.AlphaPrecision);
    }

    /// <summary>Rejects invalid later output paths before an earlier valid item can publish.</summary>
    [Theory]
    [InlineData("../outside.hetex")]
    [InlineData("C:/outside.hetex")]
    [InlineData("bad.hetex:stream")]
    [InlineData("NUL.hetex")]
    [InlineData("bad?.hetex")]
    public void UnsafePathsPreservePreviousOutputs(string path) {
        Assert.Throws<InvalidOperationException>(() => new WiiPlatformCookWorkItemExecutor().Execute([Item("first.hetex"), Item(path)], Root, Stage));
        Assert.False(File.Exists(Path.Combine(Stage, "first.hetex")));
        Assert.Equal("previous", File.ReadAllText(Path.Combine(Stage, "previous.txt")));
    }

    /// <summary>Rejects exact, parent-child, existing file and physical files-prefix aliases before publishing.</summary>
    [Theory]
    [InlineData("same.hetex", "SAME.hetex")]
    [InlineData("texture", "texture/child.hetex")]
    [InlineData("first.hetex", "previous.txt/child.hetex")]
    [InlineData("image.hetex", "files/image.hetex")]
    public void NamespaceConflictsPreservePreviousOutputs(string first, string second) {
        Assert.Throws<InvalidOperationException>(() => new WiiPlatformCookWorkItemExecutor().Execute([Item(first), Item(second)], Root, Stage));
        Assert.False(File.Exists(Path.Combine(Stage, first)));
    }

    /// <summary>Checks a later source decode failure cannot publish an earlier prepared payload.</summary>
    [Fact]
    public void LaterDecodeFailurePublishesNothing() {
        string bad = Path.Combine(Root, "bad.hetex");
        File.WriteAllBytes(bad, [1, 2, 3]);
        Assert.ThrowsAny<Exception>(() => new WiiPlatformCookWorkItemExecutor().Execute([Item("first.hetex"), Item("second.hetex", source: bad)], Root, Stage));
        Assert.False(File.Exists(Path.Combine(Stage, "first.hetex")));
        Assert.Equal("previous", File.ReadAllText(Path.Combine(Stage, "previous.txt")));
    }

    /// <summary>Rejects unknown cook contracts before any source or prior output is changed.</summary>
    [Theory]
    [InlineData("material", "wii", "runtime-texture")]
    [InlineData("texture", "ps2", "runtime-texture")]
    [InlineData("texture", "wii", "runtime-material")]
    public void UnsupportedContractPublishesNothing(string kind, string platform, string artifact) {
        var item = new PlatformCookWorkItem("bad", Source, kind, platform, artifact, "first.hetex", "first", "source", "settings",
            WiiNativeTextureCookTests.Settings("Gx.RGBA8", TextureAssetAlphaPrecision.A8).Serialize(), []);
        Assert.Throws<InvalidOperationException>(() => new WiiPlatformCookWorkItemExecutor().Execute([item], Root, Stage));
        Assert.False(File.Exists(Path.Combine(Stage, "first.hetex")));
    }

    /// <summary>Checks manifest collisions fail before the packaged workspace resets its preceding staging tree.</summary>
    [Fact]
    public async Task ManifestCollisionPreservesStageBeforeNativeBuild() {
        var artifact = new PlatformBuildArtifact("files/image.hetex", "image", "hash", "texture", "wii-default");
        var exception = await Assert.ThrowsAsync<InvalidOperationException>(() => new WiiPlatformAssetBuilder().BuildAsync(
            Request([Item("image.hetex")], [artifact]), new RecordingProgressReporter(), new RecordingDiagnosticReporter(), CancellationToken.None));
        Assert.Contains("conflicts", exception.Message, StringComparison.Ordinal);
        Assert.Equal("previous", File.ReadAllText(Path.Combine(Stage, "previous.txt")));
    }

    /// <summary>Checks complete source cooking happens before the old packaged stage is reset.</summary>
    [Fact]
    public async Task InvalidCookPreservesPackagedStageBeforeNativeBuild() {
        var exception = await Assert.ThrowsAsync<InvalidOperationException>(() => new WiiPlatformAssetBuilder().BuildAsync(
            Request([Item("../outside.hetex")], []), new RecordingProgressReporter(), new RecordingDiagnosticReporter(), CancellationToken.None));
        Assert.Contains("Unsafe Wii cooked path", exception.Message, StringComparison.Ordinal);
        Assert.Equal("previous", File.ReadAllText(Path.Combine(Stage, "previous.txt")));
    }

    /// <summary>Checks an absent manifest source is diagnosed before resetting stage or publishing prepared textures.</summary>
    [Fact]
    public async Task MissingManifestSourcePreservesStageBeforeNativeBuild() {
        var artifact = new PlatformBuildArtifact("missing-native-format-source.hetex", "missing", "hash", "texture", "wii-default");
        var report = await new WiiPlatformAssetBuilder().BuildAsync(Request([Item("first.hetex")], [artifact]),
            new RecordingProgressReporter(), new RecordingDiagnosticReporter(), CancellationToken.None);
        Assert.False(report.Succeeded);
        Assert.Equal("previous", File.ReadAllText(Path.Combine(Stage, "previous.txt")));
        Assert.False(File.Exists(Path.Combine(Stage, "first.hetex")));
    }

    /// <summary>Checks legacy font externalization retains the selected native atlas from either a work item or an existing manifest artifact.</summary>
    [Theory]
    [InlineData(0, 16)]
    [InlineData(0, 32)]
    [InlineData(0, 64)]
    [InlineData(1, 16)]
    [InlineData(1, 32)]
    [InlineData(1, 64)]
    [InlineData(2, 16)]
    [InlineData(2, 32)]
    [InlineData(2, 64)]
    [InlineData(3, 16)]
    [InlineData(3, 32)]
    [InlineData(3, 64)]
    public async Task EmbeddedFontExternalizationPreservesExplicitNativeAtlas(int atlasSourceKind, int bits) {
        var policy = new NumericPrecisionPolicy(bits, bits);
        string package = Path.Combine(Root, "package");
        string fontRelative = "cooked/fonts/body.hefont";
        string atlasRelative = "cooked/fonts/body.hetex";
        string fontPath = Path.Combine(package, fontRelative);
        Directory.CreateDirectory(Path.GetDirectoryName(fontPath));
        using TextureAsset raw = WiiNativeTextureCookTests.Source(4, 2);
        using FontAsset font = new(new FontInfo("Body", 16, 8), null, new Dictionary<char, FontChar>(), 16, 4, 2) {
            SourceTextureAsset = raw, CookedAtlasTextureRelativePath = atlasRelative
        };
        using (FileStream fontStream = File.Create(fontPath)) helengine.files.FontAssetBinarySerializer.Serialize(fontStream, font);
        font.SourceTextureAsset = null;
        byte[] originalFont = File.ReadAllBytes(fontPath);
        byte[] originalTexture = File.ReadAllBytes(Source);
        var nativeSettings = atlasSourceKind >= 2
            ? new WiiTextureCookSettings(0, "GxRgb5A3", TextureAssetAlphaPrecision.A8)
            : WiiNativeTextureCookTests.Settings("Gx.CI8.TLUT.RGB5A3", TextureAssetAlphaPrecision.A3);
        List<PlatformBuildArtifact> artifacts = [new(fontRelative, "font", "hash", "font", "wii-default")];
        PlatformCookWorkItem[] items = [];
        byte[] explicitAtlasBytes = null;
        if (atlasSourceKind == 1 || atlasSourceKind == 2) {
            using TextureAsset atlas = new WiiTextureCooker().CookTexture(raw, nativeSettings);
            explicitAtlasBytes = FilesAssetSerializer.SerializeToBytes(atlas, policy);
            File.WriteAllBytes(Path.Combine(package, atlasRelative), explicitAtlasBytes);
            artifacts.Add(new PlatformBuildArtifact(atlasRelative, "atlas", "hash", "texture", "wii-default"));
        } else if (atlasSourceKind == 0) {
            items = [new PlatformCookWorkItem("atlas", Source, "font-atlas-texture", "wii", "runtime-texture", atlasRelative, "atlas", "source", "settings", nativeSettings.Serialize(), [])];
        }
        Directory.SetCurrentDirectory(package);
        WiiStopAfterStagingNativeBuildExecutor nativeBoundary = new();
        var exception = await Assert.ThrowsAsync<InvalidOperationException>(() => new WiiPlatformAssetBuilder(nativeBoundary, null, null).BuildAsync(
            Request(items, artifacts.ToArray(), policy), new RecordingProgressReporter(), new RecordingDiagnosticReporter(), CancellationToken.None));
        Assert.Equal(WiiStopAfterStagingNativeBuildExecutor.StopMessage, exception.Message);
        Assert.True(nativeBoundary.WasInvoked);
        using var atlasStream = File.OpenRead(Path.Combine(Stage, atlasRelative));
        using TextureAsset stagedAtlas = Assert.IsType<TextureAsset>(PackagedAssetBinarySerializer.Deserialize(atlasStream, policy));
        Assert.Equal(atlasSourceKind >= 2 ? TextureAssetColorFormat.GxRgb5A3 : TextureAssetColorFormat.GxNative, stagedAtlas.ColorFormat);
        Assert.Equal(nativeSettings.AlphaPrecision, stagedAtlas.AlphaPrecision);
        if (atlasSourceKind < 2) Assert.Equal(nativeSettings.ColorFormatId, GxNativeTextureCodec.ReadLayout(stagedAtlas).Format.Id);
        if (explicitAtlasBytes != null) Assert.Equal(explicitAtlasBytes, File.ReadAllBytes(Path.Combine(Stage, atlasRelative)));
        using FileStream stagedFontStream = File.OpenRead(Path.Combine(Stage, fontRelative));
        using FontAsset stagedFont = helengine.files.FontAssetBinarySerializer.Deserialize(stagedFontStream);
        Assert.Null(stagedFont.SourceTextureAsset);
        Assert.Equal(atlasRelative, stagedFont.CookedAtlasTextureRelativePath);
        Assert.Equal(originalFont, File.ReadAllBytes(fontPath));
        Assert.Equal(originalTexture, File.ReadAllBytes(Source));
    }

    /// <summary>Builds a cook item using the native catalog's exact preferred alpha policy and selected quantizer.</summary>
    PlatformCookWorkItem Item(string output, string formatId = "Gx.RGBA8", string kind = "texture", string source = null) {
        GxNativeTextureFormatCatalog.TryGetFormat(formatId, out var format);
        return new PlatformCookWorkItem(output, source ?? Source, kind, "wii", "runtime-texture", output, output, "source", "settings",
            WiiNativeTextureCookTests.Settings(formatId, format.DefaultAlphaPrecision).Serialize(), []);
    }

    /// <summary>Creates a packaged build request whose failures occur before the real native executor can run.</summary>
    PlatformBuildRequest Request(PlatformCookWorkItem[] items, PlatformBuildArtifact[] artifacts, NumericPrecisionPolicy numericPolicy = null) {
        Dictionary<string, string> numericOptions = [];
        helengine.baseplatform.Definitions.PlatformNumericPolicyResolver.ApplyToOptions(numericPolicy ?? NumericPrecisionPolicy.Default, numericOptions);
        var manifest = new PlatformBuildManifest(3, "project", "1.0.0", "1.0.0", "wii", "1.0.0", "fixture-scene",
            [new PlatformBuildScene("fixture-scene", "Fixture", "fixture.helen", [], [new KeyValuePair<string, string>(PlatformBuildSceneMetadataKeys.CookedRelativePath, "cooked/scenes/fixture.heasset")])],
            [], artifacts, [], [], new PlatformContainerWritePlan("wii-disc-layout", []), items, PlatformBuildRuntimeFeatureManifest.Empty);
        return new PlatformBuildRequest(manifest,
            [new PlatformBuildTargetVariant("wii-default", "wii", "wii", "wii-default")],
            [new PlatformCookProfile("wii-default", "Wii", new PlatformCookProfileCapabilities("wii", "raw", "pcm", "scene", PlatformSerializationEndianness.BigEndian))],
            Path.Combine(Root, "output"), Path.Combine(Root, "working"), "wii-default", "gx", "default", null, null,
            numericOptions, Path.Combine(Root, "generated-core"));
    }

    /// <summary>Removes only this test class's contained fixed fixture directory, preserving unrelated outputs.</summary>
    public void Dispose() {
        Directory.SetCurrentDirectory(PreviousDirectory);
        Environment.SetEnvironmentVariable("HELENGINE_WII_REPOSITORY_ROOT", PreviousRepositoryRoot);
        string expected = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "gx-texture-work-items"));
        if (!Path.GetFullPath(Root).Equals(expected, StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("Fixture cleanup escaped the test artifacts root.");
        if (Directory.Exists(Root)) Directory.Delete(Root, true);
    }

    /// <summary>Finds the sibling checkout from relocated test artifacts without relying on the host's current directory.</summary>
    static string ResolveRepositoryRoot() {
        string configured = Environment.GetEnvironmentVariable("HELENGINE_WII_REPOSITORY_ROOT");
        if (!string.IsNullOrEmpty(configured) && File.Exists(Path.Combine(configured, "builder", "helengine.wii.builder.csproj"))) return configured;
        string current = AppContext.BaseDirectory;
        while (!string.IsNullOrEmpty(current)) {
            string candidate = Path.Combine(current, "helengine-wii");
            if (File.Exists(Path.Combine(candidate, "builder", "helengine.wii.builder.csproj"))) return candidate;
            current = Path.GetDirectoryName(Path.TrimEndingDirectorySeparator(current));
        }
        throw new DirectoryNotFoundException("The Wii fixture could not locate its sibling checkout.");
    }
}
