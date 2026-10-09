namespace helengine.wii.builder;

/// <summary>Holds preflighted font externalization and optional legacy atlas bytes until the complete packaged batch can publish.</summary>
internal sealed class WiiFontAtlasCookWorkPlan {
    /// <summary>Gets the contained packaged font destination.</summary>
    public string FontRelativePath { get; init; }
    /// <summary>Gets serialized font bytes referencing the preserved or newly prepared atlas.</summary>
    public byte[] FontPayload { get; init; }
    /// <summary>Gets the contained atlas destination selected by the font metadata.</summary>
    public string AtlasRelativePath { get; init; }
    /// <summary>Gets legacy atlas bytes, or null when an explicit texture work item or manifest artifact supplies the atlas.</summary>
    public byte[] AtlasPayload { get; init; }
}
