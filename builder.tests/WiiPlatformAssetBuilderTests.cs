using helengine.baseplatform.Builders;
using helengine.baseplatform.Requests;

namespace helengine.wii.builder.tests;

/// <summary>
/// Verifies the Wii builder exposes the editor-facing platform metadata contract required by the shared headless build graph.
/// </summary>
public sealed class WiiPlatformAssetBuilderTests {
    /// <summary>
    /// Ensures the Wii builder publishes one editor-loadable platform builder descriptor and packaged-disc definition.
    /// </summary>
    [Fact]
    public void Constructor_ExposesDescriptorAndDefinitionForPackagedDiscBuilds() {
        IPlatformAssetBuilder builder = new WiiPlatformAssetBuilder();

        Assert.Equal("helengine.wii.builder", builder.Descriptor.BuilderId);
        Assert.Equal("wii", builder.Descriptor.TargetPlatformId);
        Assert.Contains("wii", builder.Descriptor.SupportedRuntimeBackendIds);
        Assert.Contains("wii-default", builder.Descriptor.SupportedCookProfileFamilies);

        Assert.Equal("wii", builder.Definition.PlatformId);
        Assert.Equal("Nintendo Wii", builder.Definition.DisplayName);
        Assert.Single(builder.Definition.BuildProfiles);
        Assert.Equal("wii-default", builder.Definition.BuildProfiles[0].ProfileId);
        Assert.Single(builder.Definition.GraphicsProfiles);
        Assert.Equal("gx", builder.Definition.GraphicsProfiles[0].ProfileId);
        Assert.Single(builder.Definition.StorageProfiles);
        Assert.Equal("disc-layout", builder.Definition.StorageProfiles[0].ProfileId);
        Assert.Single(builder.Definition.MediaProfiles);
        Assert.Equal("wii-install-tree", builder.Definition.MediaProfiles[0].ProfileId);
        Assert.Contains(
            builder.Definition.AssetCookCapabilities,
            capability => string.Equals(capability.SourceAssetKind, "texture", StringComparison.Ordinal));
    }

    /// <summary>
    /// Ensures the Wii builder cooks the shared standard-shader schema into one platform-owned material payload.
    /// </summary>
    [Fact]
    public void CookMaterial_WithStandardShaderSchema_ProducesCookedPlatformMaterialAsset() {
        IPlatformAssetBuilder builder = new WiiPlatformAssetBuilder();
        PlatformMaterialCookRequest request = new PlatformMaterialCookRequest(
            "wii-material-01",
            "Materials/Menu/logo.hemat",
            "wii",
            "wii-default",
            "gx",
            "standard-shader",
            new Dictionary<string, string> {
                ["texture-id"] = "cooked/textures/logo.htex",
                ["double-sided"] = "true",
                ["vertex-color-mode"] = "ignore",
                ["base-color"] = "#804020FF",
                ["lighting-mode"] = "unlit"
            });

        var result = builder.CookMaterial(request);

        Assert.NotNull(result);
        Assert.NotEmpty(result.CookedMaterialBytes);
        Assert.Empty(result.ReferencedShaderAssetIds);
    }
}
