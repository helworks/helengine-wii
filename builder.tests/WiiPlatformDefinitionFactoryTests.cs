using helengine.baseplatform.Definitions;

namespace helengine.wii.builder.tests;

/// <summary>
/// Verifies the editor-facing Wii platform definition exposes the native codegen defaults required by editor-owned packaged builds.
/// </summary>
public sealed class WiiPlatformDefinitionFactoryTests {
    /// <summary>
    /// Ensures the default Wii codegen profile publishes the generic System.Numerics remap contract needed by physics-enabled scene builds.
    /// </summary>
    [Fact]
    public void Create_PublishesNativeNumericTypeRemapDefaultsForCppCodegen() {
        PlatformDefinition definition = WiiPlatformDefinitionFactory.Create();

        PlatformCodegenProfileDefinition codegenProfile = Assert.Single(definition.CodegenProfiles);
        PlatformSettingDefinition typeRemapsSetting = Assert.Single(codegenProfile.Settings.Where(setting => string.Equals(setting.SettingId, "type-remaps", StringComparison.Ordinal)));
        PlatformSettingDefinition runtimeMetadataSetting = Assert.Single(codegenProfile.Settings.Where(setting => string.Equals(setting.SettingId, "load-native-runtime-metadata", StringComparison.Ordinal)));
        PlatformSettingDefinition generatedMathConventionSetting = Assert.Single(codegenProfile.Settings.Where(setting => string.Equals(setting.SettingId, "generated-math-convention", StringComparison.Ordinal)));
        PlatformSettingDefinition pointerSizeSetting = Assert.Single(codegenProfile.Settings.Where(setting => string.Equals(setting.SettingId, "pointer-size-bytes", StringComparison.Ordinal)));
        PlatformSettingDefinition nativeFileSystemHeaderSetting = Assert.Single(codegenProfile.Settings.Where(setting => string.Equals(setting.SettingId, "native-file-system-header", StringComparison.Ordinal)));
        PlatformSettingDefinition nativeFileSystemTypeSetting = Assert.Single(codegenProfile.Settings.Where(setting => string.Equals(setting.SettingId, "native-file-system-type", StringComparison.Ordinal)));

        Assert.Equal("System.Numerics.Vector2=helengine.float2;System.Numerics.Vector3=helengine.float3;System.Numerics.Vector4=helengine.float4;System.Numerics.Quaternion=helengine.float4", typeRemapsSetting.DefaultValue);
        Assert.Equal("true", runtimeMetadataSetting.DefaultValue);
        Assert.Equal("native-column-vector", generatedMathConventionSetting.DefaultValue);
        Assert.Equal("4", pointerSizeSetting.DefaultValue);
        Assert.Equal("\"platform/wii/WiiDiscFileSystem.hpp\"", nativeFileSystemHeaderSetting.DefaultValue);
        Assert.Equal("helengine::wii::WiiDiscFileSystem", nativeFileSystemTypeSetting.DefaultValue);
        Assert.Contains(PortableInputPreprocessorSymbolCatalog.MatrixAbiGxGameCubeWiiSymbol, definition.RuntimeGenerationContract.PortableInputPreprocessorSymbols);
    }
}
