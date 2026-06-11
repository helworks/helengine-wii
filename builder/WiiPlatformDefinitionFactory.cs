using System.Text.Json;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;
using helengine.editor;

namespace helengine.wii.builder;

/// <summary>
/// Creates the typed Wii builder metadata consumed by the editor.
/// </summary>
public static class WiiPlatformDefinitionFactory {
    /// <summary>
    /// Generic native numeric type remaps required by C++ platforms that cannot emit System.Numerics runtime types directly.
    /// </summary>
    const string NativeNumericTypeRemaps = "System.Numerics.Vector2=helengine.float2;System.Numerics.Vector3=helengine.float3;System.Numerics.Vector4=helengine.float4;System.Numerics.Quaternion=helengine.float4";

    /// <summary>
    /// Generic generated-math-convention value that instructs the shared C++ generator to emit native column-vector math helpers.
    /// </summary>
    const string NativeColumnVectorMathConvention = "native-column-vector";

    /// <summary>
    /// Generic pointer-size contract forwarded to the shared C++ generator for Wii-native output.
    /// </summary>
    const string WiiPointerSizeInBytes = "4";

    /// <summary>
    /// Generic native file-system header contract forwarded to the shared C++ generator so packaged-disc file access routes through the Wii disc bridge without generated-source rewrites.
    /// </summary>
    const string WiiNativeFileSystemHeader = "\"platform/wii/WiiDiscFileSystem.hpp\"";

    /// <summary>
    /// Generic native file-system type contract forwarded to the shared C++ generator so packaged-disc file access routes through the Wii disc bridge without generated-source rewrites.
    /// </summary>
    const string WiiNativeFileSystemType = "helengine::wii::WiiDiscFileSystem";

    /// <summary>
    /// Creates the serialized default Wii texture settings contract used when assets do not provide an explicit Wii override.
    /// </summary>
    /// <returns>Serialized default Wii texture settings.</returns>
    static string CreateDefaultSerializedTextureCookSettings() {
        return SerializeTextureCookSettings(new TextureAssetProcessorSettings {
            MaxResolution = 0,
            ColorFormatId = TextureAssetColorFormat.GxRgb5A3.ToString(),
            AlphaPrecision = TextureAssetAlphaPrecision.A8
        });
    }

    /// <summary>
    /// Creates the serialized default Wii font-atlas texture settings contract used when fonts do not provide an explicit Wii override.
    /// </summary>
    /// <returns>Serialized default Wii font-atlas texture settings.</returns>
    static string CreateDefaultSerializedFontAtlasTextureCookSettings() {
        return SerializeTextureCookSettings(new TextureAssetProcessorSettings {
            MaxResolution = 0,
            ColorFormatId = TextureAssetColorFormat.GxRgb5A3.ToString(),
            AlphaPrecision = TextureAssetAlphaPrecision.A8
        });
    }

    /// <summary>
    /// Serializes the generic texture cook settings contract published to the editor build graph.
    /// </summary>
    /// <param name="processorSettings">Resolved texture processor settings to serialize.</param>
    /// <returns>Serialized generic texture cook settings payload.</returns>
    static string SerializeTextureCookSettings(TextureAssetProcessorSettings processorSettings) {
        if (processorSettings == null) {
            throw new ArgumentNullException(nameof(processorSettings));
        }

        return JsonSerializer.Serialize(new Dictionary<string, object> {
            ["maxResolution"] = processorSettings.MaxResolution,
            ["colorFormat"] = processorSettings.ColorFormatId,
            ["alphaPrecision"] = processorSettings.AlphaPrecision.ToString()
        });
    }

    /// <summary>
    /// Creates the generic texture format capability metadata supported by the Wii texture cooker.
    /// </summary>
    /// <returns>Texture capability metadata for Wii builder-owned texture cook contracts.</returns>
    static PlatformTextureFormatCapabilityDefinition CreateTextureFormatCapabilities() {
        return new PlatformTextureFormatCapabilityDefinition(
            [
                TextureAssetColorFormat.GxRgb5A3.ToString()
            ],
            [
                TextureAssetAlphaPrecision.A8
            ],
            [
                new PlatformTextureFormatCombinationDefinition(TextureAssetColorFormat.GxRgb5A3.ToString(), TextureAssetAlphaPrecision.A8)
            ]);
    }

    /// <summary>
    /// Creates the current Wii platform definition.
    /// </summary>
    /// <returns>The Wii platform definition.</returns>
    public static PlatformDefinition Create() {
        return new PlatformDefinition(
            "wii",
            "Nintendo Wii",
            [
                new PlatformBuildProfileDefinition(
                    "wii-default",
                    "Wii Default",
                    "Standard Wii packaged-disc player build",
                    "gx",
                    "default",
                    [
                        new PlatformSettingDefinition(
                            "texture-scale-percent",
                            "Texture Scale Percent",
                            PlatformSettingKind.Text,
                            "100",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "shader-variant-pruning",
                            "Shader Variant Pruning",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            [])
                    ])
            ],
            [
                new PlatformGraphicsProfileDefinition(
                    "gx",
                    "GX",
                    "Current Wii rendering backend",
                    [
                        new PlatformSettingDefinition(
                            "default-width",
                            "Default Width",
                            PlatformSettingKind.Text,
                            "640",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "default-height",
                            "Default Height",
                            PlatformSettingKind.Text,
                            "480",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "vsync-enabled",
                            "VSync Enabled",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "fullscreen-enabled",
                            "Fullscreen Enabled",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            [])
                    ])
            ],
            [
                new PlatformAssetRequirementDefinition(
                    "scene",
                    "Scene",
                    true,
                    ["helen"]),
                new PlatformAssetRequirementDefinition(
                    "texture",
                    "Texture",
                    true,
                    ["png", "tga", "jpg"]),
                new PlatformAssetRequirementDefinition(
                    "font",
                    "Font",
                    false,
                    ["font.asset", "ttf", "otf"])
            ],
            [
                new PlatformMaterialSchemaDefinition(
                    WiiMaterialSchemaIds.StandardTexturedSchemaId,
                    "Wii Standard Textured",
                    ["gx"],
                    [
                        new PlatformMaterialFieldDefinition(
                            WiiMaterialSchemaIds.TextureRelativePathFieldId,
                            "Texture",
                            PlatformMaterialFieldKind.Text,
                            string.Empty,
                            false,
                            []),
                        new PlatformMaterialFieldDefinition(
                            WiiMaterialSchemaIds.DoubleSidedFieldId,
                            "Double Sided",
                            PlatformMaterialFieldKind.Boolean,
                            "false",
                            true,
                            []),
                        new PlatformMaterialFieldDefinition(
                            WiiMaterialSchemaIds.VertexColorModeFieldId,
                            "Vertex Color",
                            PlatformMaterialFieldKind.Choice,
                            "multiply",
                            true,
                            ["multiply", "ignore"]),
                        new PlatformMaterialFieldDefinition(
                            WiiMaterialSchemaIds.BaseColorFieldId,
                            "Base Color",
                            PlatformMaterialFieldKind.Color,
                            "#FFFFFFFF",
                            true,
                            []),
                        new PlatformMaterialFieldDefinition(
                            WiiMaterialSchemaIds.LightingModeFieldId,
                            "Lighting",
                            PlatformMaterialFieldKind.Choice,
                            "lit",
                            true,
                            ["lit", "unlit"])
                    ])
            ],
            [
                new PlatformComponentSupportRule(
                    "helengine.meshcomponent",
                    PlatformComponentSupportKind.Transform,
                    "Mesh components are normalized during packaging.",
                    string.Empty),
                new PlatformComponentSupportRule(
                    "helengine.cameracomponent",
                    PlatformComponentSupportKind.Transform,
                    "Camera components are normalized during packaging.",
                    string.Empty),
                new PlatformComponentSupportRule(
                    "helengine.fpscomponent",
                    PlatformComponentSupportKind.PassThrough,
                    "FPS overlay payload is canonical across platforms.",
                    string.Empty)
            ],
            [
                new PlatformCodegenProfileDefinition(
                    "default",
                    "Default",
                    "Wii C# to C++ codegen profile",
                    PlatformCodegenLanguage.Cpp,
                    PlatformSerializationEndianness.BigEndian,
                    [
                        new PlatformSettingDefinition(
                            "write-conversion-report",
                            "Write Conversion Report",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "include-project-defined-preprocessor-symbols",
                            "Include Project Symbols",
                            PlatformSettingKind.Boolean,
                            "false",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "load-native-runtime-metadata",
                            "Load Native Runtime Metadata",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "generated-math-convention",
                            "Generated Math Convention",
                            PlatformSettingKind.Text,
                            NativeColumnVectorMathConvention,
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "pointer-size-bytes",
                            "Pointer Size (Bytes)",
                            PlatformSettingKind.Text,
                            WiiPointerSizeInBytes,
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "type-remaps",
                            "Type Remaps",
                            PlatformSettingKind.Text,
                            NativeNumericTypeRemaps,
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "native-file-system-header",
                            "Native File System Header",
                            PlatformSettingKind.Text,
                            WiiNativeFileSystemHeader,
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "native-file-system-type",
                            "Native File System Type",
                            PlatformSettingKind.Text,
                            WiiNativeFileSystemType,
                            true,
                            [])
                    ])
            ],
            [
                new PlatformStorageProfileDefinition(
                    "disc-layout",
                    "Disc Layout",
                    PlatformStorageProfileKind.DiscLayout,
                    "wii-disc-layout",
                    allowContainerSegmentation: true)
            ],
            [
                new PlatformMediaProfileDefinition(
                    "wii-install-tree",
                    "Wii Install Tree",
                    PlatformMediaLayoutKind.InstallTree,
                    allowPhysicalDuplication: true,
                    preferLocalityOverDeduplication: true)
            ],
            new RuntimeGenerationContract(
                RuntimeMaterialResolutionMode.CookedPlatformOwned,
                true,
                PackagedPathPolicy.ContentRelativeOnly),
            assetCookCapabilities: [
                new PlatformAssetCookCapabilityDefinition(
                    "texture",
                    "runtime-texture",
                    PlatformAssetCookOwnershipKind.BuilderOwned,
                    "wii-texture",
                    CreateDefaultSerializedTextureCookSettings(),
                    CreateTextureFormatCapabilities(),
                    "htex"),
                new PlatformAssetCookCapabilityDefinition(
                    "font-atlas-texture",
                    "runtime-font-atlas-texture",
                    PlatformAssetCookOwnershipKind.BuilderOwned,
                    "wii-texture",
                    CreateDefaultSerializedFontAtlasTextureCookSettings(),
                    CreateTextureFormatCapabilities(),
                    "htex")
            ]);
    }
}
