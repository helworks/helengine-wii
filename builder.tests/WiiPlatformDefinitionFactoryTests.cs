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

        Assert.Equal("System.Numerics.Vector2=helengine.float2;System.Numerics.Vector3=helengine.float3;System.Numerics.Vector4=helengine.float4;System.Numerics.Quaternion=helengine.float4", typeRemapsSetting.DefaultValue);
        Assert.Equal("true", runtimeMetadataSetting.DefaultValue);
    }
}
