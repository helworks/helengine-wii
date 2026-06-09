using System.Text.Json;

namespace helengine.wii.builder;

/// <summary>
/// Represents the Wii-native texture cook settings emitted by the shared editor build graph.
/// </summary>
public sealed class WiiTextureCookSettings {
    /// <summary>
    /// Initializes one Wii texture cook settings record.
    /// </summary>
    /// <param name="maxResolution">Maximum allowed output width or height, or zero when uncapped.</param>
    /// <param name="colorFormatId">Final serialized texture payload format identifier.</param>
    /// <param name="alphaPrecision">Final serialized alpha precision.</param>
    public WiiTextureCookSettings(int maxResolution, string colorFormatId, TextureAssetAlphaPrecision alphaPrecision) {
        if (maxResolution < 0) {
            throw new ArgumentOutOfRangeException(nameof(maxResolution), "Maximum resolution must be zero or greater.");
        } else if (string.IsNullOrWhiteSpace(colorFormatId)) {
            throw new ArgumentException("Color format id must be provided.", nameof(colorFormatId));
        }

        MaxResolution = maxResolution;
        ColorFormatId = colorFormatId;
        AlphaPrecision = alphaPrecision;
    }

    /// <summary>
    /// Parses one serialized editor settings payload into Wii texture cook settings.
    /// </summary>
    /// <param name="serializedPlatformSettings">Serialized settings payload emitted by the editor build graph.</param>
    /// <returns>Parsed Wii texture cook settings.</returns>
    public static WiiTextureCookSettings Parse(string serializedPlatformSettings) {
        if (string.IsNullOrWhiteSpace(serializedPlatformSettings)) {
            throw new ArgumentException("Serialized platform settings must be provided.", nameof(serializedPlatformSettings));
        }

        using JsonDocument document = JsonDocument.Parse(serializedPlatformSettings);
        JsonElement root = document.RootElement;

        int maxResolution = root.TryGetProperty("maxResolution", out JsonElement maxResolutionElement)
            ? maxResolutionElement.GetInt32()
            : 0;
        string colorFormatName = root.TryGetProperty("colorFormat", out JsonElement colorFormatElement)
            ? colorFormatElement.GetString() ?? string.Empty
            : string.Empty;
        string alphaPrecisionName = root.TryGetProperty("alphaPrecision", out JsonElement alphaPrecisionElement)
            ? alphaPrecisionElement.GetString() ?? string.Empty
            : string.Empty;

        if (!Enum.TryParse(alphaPrecisionName, ignoreCase: true, out TextureAssetAlphaPrecision alphaPrecision)) {
            throw new InvalidOperationException($"Unsupported Wii texture alpha precision '{alphaPrecisionName}'.");
        }

        return new WiiTextureCookSettings(maxResolution, colorFormatName, alphaPrecision);
    }

    /// <summary>
    /// Gets the maximum allowed output width or height, or zero when uncapped.
    /// </summary>
    public int MaxResolution { get; }

    /// <summary>
    /// Gets the final serialized texture payload format identifier.
    /// </summary>
    public string ColorFormatId { get; }

    /// <summary>
    /// Gets the final serialized alpha precision.
    /// </summary>
    public TextureAssetAlphaPrecision AlphaPrecision { get; }
}
