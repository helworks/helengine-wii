using System.Text.Json;
using helengine.editor;

namespace helengine.wii.builder;

/// <summary>Retains persisted Wii native format, alpha and palette strategy settings without changing authored defaults.</summary>
public sealed class WiiTextureCookSettings {
    /// <summary>Initializes settings and rejects unsupported format/alpha/indexing combinations before decoding a source.</summary>
    public WiiTextureCookSettings(int maxResolution, string colorFormatId, TextureAssetAlphaPrecision alphaPrecision, string indexingMethodId = "") {
        MaxResolution = maxResolution;
        ColorFormatId = colorFormatId;
        AlphaPrecision = alphaPrecision;
        IndexingMethodId = indexingMethodId ?? string.Empty;
        Validate();
    }

    /// <summary>Gets the maximum output dimension, with zero retaining uncapped authored dimensions.</summary>
    public int MaxResolution { get; }
    /// <summary>Gets the legacy or native format id as authored in project settings.</summary>
    public string ColorFormatId { get; }
    /// <summary>Gets the requested alpha policy supported by the selected GX texture or TLUT format.</summary>
    public TextureAssetAlphaPrecision AlphaPrecision { get; }
    /// <summary>Gets the explicitly selected shared quantizer, or the established default when unspecified.</summary>
    public string IndexingMethodId { get; }

    /// <summary>Rejects unknown formats, unsupported alpha precisions and inappropriate palette strategies.</summary>
    public void Validate() {
        if (MaxResolution < 0) throw new ArgumentOutOfRangeException(nameof(MaxResolution), "Maximum resolution must be zero or greater.");
        if (ColorFormatId == "GxRgb5A3") {
            if (AlphaPrecision != TextureAssetAlphaPrecision.Opaque && AlphaPrecision != TextureAssetAlphaPrecision.Binary &&
                AlphaPrecision != TextureAssetAlphaPrecision.A4 && AlphaPrecision != TextureAssetAlphaPrecision.A8) throw new InvalidOperationException("Unsupported legacy Wii alpha precision.");
            if (IndexingMethodId.Length != 0) throw new InvalidOperationException("Legacy RGB5A3 does not use a palette indexing method.");
            return;
        }
        if (!GxNativeTextureFormatCatalog.TryGetFormat(ColorFormatId, out GxNativeTextureFormatDefinition format)) throw new InvalidOperationException($"Unsupported Wii texture format '{ColorFormatId}'.");
        if (!format.SupportsAlpha(AlphaPrecision)) throw new InvalidOperationException($"Wii format '{ColorFormatId}' does not support '{AlphaPrecision}' alpha.");
        if (format.IsIndexed) {
            if (IndexingMethodId.Length != 0 && IndexingMethodId != TextureAssetIndexingMethod.QuantizedIndexed.ToString()) throw new InvalidOperationException($"Unsupported Wii indexing method '{IndexingMethodId}'.");
        } else if (IndexingMethodId.Length != 0) {
            throw new InvalidOperationException("Wii indexing method applies only to indexed formats.");
        }
    }

    /// <summary>Parses the existing settings shape and optional palette strategy, rejecting malformed or unknown fields.</summary>
    public static WiiTextureCookSettings Parse(string serializedPlatformSettings) {
        if (string.IsNullOrWhiteSpace(serializedPlatformSettings)) throw new ArgumentException("Serialized platform settings must be provided.", nameof(serializedPlatformSettings));
        using JsonDocument document = JsonDocument.Parse(serializedPlatformSettings);
        JsonElement root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Object) throw new InvalidOperationException("Wii texture settings must be an object.");
        HashSet<string> keys = new(StringComparer.Ordinal);
        foreach (JsonProperty property in root.EnumerateObject()) {
            if (!keys.Add(property.Name)) throw new InvalidOperationException($"Duplicate Wii texture setting '{property.Name}'.");
            if (property.Name != "maxResolution" && property.Name != "colorFormat" && property.Name != "alphaPrecision" && property.Name != "indexingMethod") throw new InvalidOperationException($"Unknown Wii texture setting '{property.Name}'.");
        }
        if (!root.TryGetProperty("maxResolution", out JsonElement maximum) || !root.TryGetProperty("colorFormat", out JsonElement format) || !root.TryGetProperty("alphaPrecision", out JsonElement alpha)) throw new InvalidOperationException("Wii texture settings require maxResolution, colorFormat and alphaPrecision.");
        string alphaName = alpha.GetString();
        if (!Enum.TryParse(alphaName, false, out TextureAssetAlphaPrecision precision) || precision.ToString() != alphaName) throw new InvalidOperationException($"Unsupported Wii texture alpha precision '{alphaName}'.");
        return new WiiTextureCookSettings(maximum.GetInt32(), format.GetString(), precision,
            root.TryGetProperty("indexingMethod", out JsonElement indexing) ? indexing.GetString() : string.Empty);
    }

    /// <summary>Writes native ids and selected quantizer without adding a new field to the established legacy default JSON.</summary>
    public string Serialize() {
        Validate();
        Dictionary<string, object> values = new() {
            ["maxResolution"] = MaxResolution, ["colorFormat"] = ColorFormatId, ["alphaPrecision"] = AlphaPrecision.ToString()
        };
        if (IndexingMethodId.Length != 0) values.Add("indexingMethod", IndexingMethodId);
        return JsonSerializer.Serialize(values);
    }
}
