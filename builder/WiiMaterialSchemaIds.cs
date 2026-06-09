namespace helengine.wii.builder;

/// <summary>
/// Stores the schema and field identifiers used by Wii fixed-pipeline material cooking.
/// </summary>
public static class WiiMaterialSchemaIds {
    /// <summary>
    /// Standard textured Wii schema id.
    /// </summary>
    public const string StandardTexturedSchemaId = "wii-standard-textured";

    /// <summary>
    /// Shared generic standard-shader schema id accepted for backward compatibility.
    /// </summary>
    public const string StandardShaderSchemaId = "standard-shader";

    /// <summary>
    /// Texture-relative-path field id.
    /// </summary>
    public const string TextureRelativePathFieldId = "texture-relative-path";

    /// <summary>
    /// Shared generic texture-id field id accepted for backward compatibility.
    /// </summary>
    public const string TextureIdFieldId = "texture-id";

    /// <summary>
    /// Double-sided field id.
    /// </summary>
    public const string DoubleSidedFieldId = "double-sided";

    /// <summary>
    /// Vertex-color-mode field id.
    /// </summary>
    public const string VertexColorModeFieldId = "vertex-color-mode";

    /// <summary>
    /// Base-color field id.
    /// </summary>
    public const string BaseColorFieldId = "base-color";

    /// <summary>
    /// Lighting-mode field id.
    /// </summary>
    public const string LightingModeFieldId = "lighting-mode";
}
