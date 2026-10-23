using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using helengine.baseplatform.Manifest;
using helengine.editor;
using FilesAssetSerializer = helengine.files.AssetSerializer;
using FilesEngineBinaryHeaderSerializer = helengine.files.EngineBinaryHeaderSerializer;
using FilesFontAssetBinarySerializer = helengine.files.FontAssetBinarySerializer;

namespace helengine.wii.builder;

/// <summary>
/// Executes generic platform-owned cook work items for the Wii builder.
/// </summary>
public sealed class WiiPlatformCookWorkItemExecutor {
    /// <summary>
    /// Shared Wii texture cooker used by both raw texture and font-atlas execution paths.
    /// </summary>
    readonly WiiTextureCooker TextureCooker;

    /// <summary>
    /// Initializes one Wii platform cook work-item executor.
    /// </summary>
    public WiiPlatformCookWorkItemExecutor() {
        TextureCooker = new WiiTextureCooker();
    }

    /// <summary>Prepares the entire texture batch before publishing any output in the staging root.</summary>
    public void Execute(PlatformCookWorkItem[] workItems, string projectRootPath, string stagingRootPath, NumericPrecisionPolicy numericPolicy = null) {
        IReadOnlyList<WiiTextureCookWorkPlan> plans = Prepare(workItems, [], projectRootPath, stagingRootPath, CancellationToken.None, numericPolicy ?? NumericPrecisionPolicy.Default);
        Publish(plans, CancellationToken.None);
    }

    /// <summary>Validates all contracts and disc paths, then decodes and cooks every source without changing staged outputs.</summary>
    internal IReadOnlyList<WiiTextureCookWorkPlan> Prepare(
        PlatformCookWorkItem[] workItems, PlatformBuildArtifact[] artifacts, string projectRootPath, string stagingRootPath, CancellationToken cancellationToken, NumericPrecisionPolicy numericPolicy) {
        ArgumentNullException.ThrowIfNull(workItems);
        ArgumentNullException.ThrowIfNull(artifacts);
        RealBinaryCodec.ValidatePolicy(numericPolicy);
        if (string.IsNullOrWhiteSpace(projectRootPath)) throw new ArgumentException("Project root is required.", nameof(projectRootPath));
        if (string.IsNullOrWhiteSpace(stagingRootPath)) throw new ArgumentException("Staging root is required.", nameof(stagingRootPath));
        cancellationToken.ThrowIfCancellationRequested();
        WiiTextureCookPath.ValidateRoot(stagingRootPath);
        HashSet<string> reserved = new(StringComparer.OrdinalIgnoreCase);
        foreach (PlatformBuildArtifact artifact in artifacts) {
            WiiTextureCookPath.Reserve(reserved, artifact.RelativePath);
            WiiTextureCookPath.Resolve(stagingRootPath, artifact.RelativePath);
        }
        HashSet<string> sources = new(StringComparer.OrdinalIgnoreCase);
        foreach (PlatformCookWorkItem item in workItems) {
            if (item == null) throw new InvalidOperationException("Wii cook work items cannot be null.");
            sources.Add(ResolveSourceAssetPath(projectRootPath, item.SourceAssetPath));
        }
        List<WiiTextureCookWorkPlan> plans = new(workItems.Length);
        foreach (PlatformCookWorkItem item in workItems) {
            cancellationToken.ThrowIfCancellationRequested();
            if (!string.Equals(item.TargetPlatformId, "wii", StringComparison.OrdinalIgnoreCase) ||
                (!string.Equals(item.TargetArtifactKind, "runtime-texture", StringComparison.OrdinalIgnoreCase) &&
                    !(string.Equals(item.SourceAssetKind, "font-atlas-texture", StringComparison.OrdinalIgnoreCase) && string.Equals(item.TargetArtifactKind, "runtime-font-atlas-texture", StringComparison.OrdinalIgnoreCase))) ||
                (!string.Equals(item.SourceAssetKind, "texture", StringComparison.OrdinalIgnoreCase) && !string.Equals(item.SourceAssetKind, "font-atlas-texture", StringComparison.OrdinalIgnoreCase))) {
                throw new InvalidOperationException($"Unsupported Wii cook contract '{item.WorkItemId}'.");
            }
            WiiTextureCookSettings settings = WiiTextureCookSettings.Parse(item.SerializedPlatformSettings);
            string relative = WiiTextureCookPath.Normalize(item.OutputRelativePath);
            WiiTextureCookPath.Reserve(reserved, relative);
            string destination = WiiTextureCookPath.Resolve(stagingRootPath, relative);
            string source = ResolveSourceAssetPath(projectRootPath, item.SourceAssetPath);
            if (source.StartsWith(Path.TrimEndingDirectorySeparator(Path.GetFullPath(stagingRootPath)) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("Wii source cannot reside in the staging tree being replaced.");
            if (sources.Contains(destination)) throw new InvalidOperationException("Wii cook output would replace a source.");
            if (!File.Exists(source)) throw new FileNotFoundException("Wii cook source was not found.", source);
            plans.Add(new WiiTextureCookWorkPlan { WorkItem = item, Settings = settings, SourcePath = source, DestinationPath = destination, StagingRootPath = stagingRootPath });
        }
        foreach (WiiTextureCookWorkPlan plan in plans) {
            cancellationToken.ThrowIfCancellationRequested();
            using TextureAsset source = LoadSourceTexture(plan.WorkItem, projectRootPath);
            using TextureAsset cooked = TextureCooker.CookTexture(source, plan.Settings);
            cooked.Id = plan.WorkItem.OutputLogicalArtifactId;
            cooked.RuntimeAssetId = RuntimeAssetIdGenerator.Generate(cooked.Id);
            plan.Payload = FilesAssetSerializer.SerializeToBytes(cooked, numericPolicy);
        }
        cancellationToken.ThrowIfCancellationRequested();
        return plans;
    }

    /// <summary>Rechecks contained destinations and writes only a fully prepared texture batch.</summary>
    internal void Publish(IReadOnlyList<WiiTextureCookWorkPlan> plans, CancellationToken cancellationToken) {
        cancellationToken.ThrowIfCancellationRequested();
        foreach (WiiTextureCookWorkPlan plan in plans) WiiTextureCookPath.Resolve(plan.StagingRootPath, plan.WorkItem.OutputRelativePath);
        foreach (WiiTextureCookWorkPlan plan in plans) {
            Directory.CreateDirectory(Path.GetDirectoryName(plan.DestinationPath));
            File.WriteAllBytes(plan.DestinationPath, plan.Payload);
        }
    }

    /// <summary>
    /// Loads one source texture from the authored project asset path emitted by the editor build graph.
    /// </summary>
    /// <param name="workItem">Texture work item emitted by the editor build graph.</param>
    /// <param name="projectRootPath">Project root used to resolve the source texture asset path.</param>
    /// <returns>Imported RGBA32 texture asset ready for Wii-native cooking.</returns>
    static TextureAsset LoadSourceTexture(PlatformCookWorkItem workItem, string projectRootPath) {
        if (workItem == null) {
            throw new ArgumentNullException(nameof(workItem));
        } else if (string.IsNullOrWhiteSpace(projectRootPath)) {
            throw new ArgumentException("Project root path must be provided.", nameof(projectRootPath));
        }

        string sourcePath = ResolveSourceAssetPath(projectRootPath, workItem.SourceAssetPath);
        if (string.Equals(workItem.SourceAssetKind, "font-atlas-texture", StringComparison.OrdinalIgnoreCase) && Path.GetExtension(sourcePath).Equals(".hefont", StringComparison.OrdinalIgnoreCase)) {
            using FileStream fontStream = File.OpenRead(sourcePath);
            FontAsset font = FilesFontAssetBinarySerializer.Deserialize(fontStream);
            if (font.SourceTextureAsset == null) throw new InvalidOperationException("Wii font cook source has no embedded atlas.");
            return font.SourceTextureAsset;
        }
        if (TryReadTextureAsset(sourcePath, out TextureAsset textureAsset)) {
            return textureAsset;
        }

        string imagePath = ResolveImageSourcePath(sourcePath);
        string extension = Path.GetExtension(imagePath);
        TextureImporterRegistration registration = PortableImporterFactory.CreateDefault()
            .OfType<TextureImporterRegistration>()
            .FirstOrDefault(importer => importer.Extensions.Contains(extension, StringComparer.OrdinalIgnoreCase))
            ?? throw new InvalidOperationException($"No portable texture importer supports source extension '{extension}'.");
        using FileStream imageStream = File.OpenRead(imagePath);
        using TextureAsset imported = registration.Importer.ImportTexture(imageStream)
            ?? throw new InvalidOperationException($"Texture importer '{registration.ImporterId}' did not return a texture asset.");
        if (imported.Width < 1 || imported.Height < 1) {
            throw new InvalidOperationException($"Texture source '{workItem.SourceAssetPath}' must have positive dimensions.");
        }
        return new TextureAsset {
            Id = ResolveMetadataValue(workItem, "source-asset-id"),
            Width = imported.Width,
            Height = imported.Height,
            Colors = TextureAssetPixelCodec.DecodeToRgba32(imported),
            PaletteColors = Array.Empty<byte>(),
            ColorFormat = TextureAssetColorFormat.Rgba32,
            AlphaPrecision = TextureAssetAlphaPrecision.A8
        };
    }
    /// <summary>
    /// Attempts to read one serialized texture asset from an authored HELE file path.
    /// </summary>
    /// <param name="sourcePath">Absolute source path to inspect.</param>
    /// <param name="textureAsset">Deserialized texture asset when the file stores one.</param>
    /// <returns>True when the source path stored a texture asset; otherwise false.</returns>
    static bool TryReadTextureAsset(string sourcePath, out TextureAsset textureAsset) {
        textureAsset = null;
        if (string.IsNullOrWhiteSpace(sourcePath)) {
            throw new ArgumentException("Source path must be provided.", nameof(sourcePath));
        }

        string extension = Path.GetExtension(sourcePath);
        if (!string.Equals(extension, ".hasset", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(extension, ".heasset", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(extension, ".hetex", StringComparison.OrdinalIgnoreCase)) {
            return false;
        }

        using FileStream stream = new FileStream(sourcePath, FileMode.Open, FileAccess.Read, FileShare.Read);
        EngineBinaryHeader header;
        try {
            header = FilesEngineBinaryHeaderSerializer.Read(stream);
        } catch (InvalidOperationException) {
            return false;
        }

        if (header.RecordKind != (ushort)EditorBinaryRecordKind.Asset) {
            return false;
        }

        stream.Position = 0;
        textureAsset = FilesAssetSerializer.Deserialize(stream) as TextureAsset;
        return textureAsset != null;
    }

    /// <summary>
    /// Resolves the bitmap source path that should be imported when one work-item source path is not a serialized texture asset.
    /// </summary>
    /// <param name="sourcePath">Absolute authored source path emitted by the editor work item.</param>
    /// <returns>Absolute image source path supported by the portable importer registry.</returns>
    static string ResolveImageSourcePath(string sourcePath) {
        if (string.IsNullOrWhiteSpace(sourcePath)) {
            throw new ArgumentException("Source path must be provided.", nameof(sourcePath));
        }

        if (TryResolveImportSettingsSiblingSourcePath(sourcePath, out string siblingSourcePath)) {
            return siblingSourcePath;
        }

        return sourcePath;
    }

    /// <summary>
    /// Attempts to resolve the sibling authored image path for one import-settings sidecar.
    /// </summary>
    /// <param name="sourcePath">Absolute work-item source path that may point at one `.hasset` import-settings sidecar.</param>
    /// <param name="siblingSourcePath">Resolved authored image path when one matching sibling exists.</param>
    /// <returns>True when one sibling source image was found beside the import-settings file; otherwise false.</returns>
    static bool TryResolveImportSettingsSiblingSourcePath(string sourcePath, out string siblingSourcePath) {
        siblingSourcePath = string.Empty;
        if (string.IsNullOrWhiteSpace(sourcePath)) {
            throw new ArgumentException("Source path must be provided.", nameof(sourcePath));
        }

        if (!string.Equals(Path.GetExtension(sourcePath), ".hasset", StringComparison.OrdinalIgnoreCase)) {
            return false;
        }

        string directoryPath = Path.GetDirectoryName(sourcePath) ?? string.Empty;
        if (string.IsNullOrWhiteSpace(directoryPath) || !Directory.Exists(directoryPath)) {
            return false;
        }

        string fileNameWithoutExtension = Path.GetFileNameWithoutExtension(sourcePath);
        string[] candidateExtensions = [".png", ".tga", ".jpg", ".jpeg", ".bmp"];
        for (int index = 0; index < candidateExtensions.Length; index++) {
            string candidatePath = Path.Combine(directoryPath, fileNameWithoutExtension + candidateExtensions[index]);
            if (!File.Exists(candidatePath)) {
                continue;
            }

            siblingSourcePath = candidatePath;
            return true;
        }

        return false;
    }

    /// <summary>
    /// Reads one serialized font asset from the supplied source path without requiring a runtime renderer.
    /// </summary>
    /// <param name="sourcePath">Absolute path to the serialized font asset.</param>
    /// <returns>Deserialized font asset including any embedded source atlas texture.</returns>
    static FontAsset ReadFontAsset(string sourcePath) {
        if (string.IsNullOrWhiteSpace(sourcePath)) {
            throw new ArgumentException("Source path must be provided.", nameof(sourcePath));
        } else if (!File.Exists(sourcePath)) {
            throw new FileNotFoundException("Font source file was not found.", sourcePath);
        }

        using FileStream stream = new FileStream(sourcePath, FileMode.Open, FileAccess.Read, FileShare.Read);
        return FilesFontAssetBinarySerializer.Deserialize(stream);
    }

    /// <summary>
    /// Validates that the supplied binary header matches the packaged font asset format.
    /// </summary>
    /// <param name="header">Binary header already read from the source stream.</param>
    static void ValidateFontHeader(EngineBinaryHeader header) {
        if (header == null) {
            throw new ArgumentNullException(nameof(header));
        } else if (header.FormatId != FilesFontAssetBinarySerializer.FormatId) {
            throw new InvalidOperationException($"Unsupported font binary format id '{header.FormatId}'.");
        } else if (header.RecordKind != (ushort)FilesFontAssetBinarySerializer.RecordKind) {
            throw new InvalidOperationException($"Unexpected font record kind '{header.RecordKind}'.");
        } else if (header.Version < 1 || header.Version > FilesFontAssetBinarySerializer.CurrentVersion) {
            throw new InvalidOperationException($"Unsupported font binary version '{header.Version}'.");
        }
    }

    /// <summary>
    /// Reads one serialized font source texture payload from the packaged font stream.
    /// </summary>
    /// <param name="reader">Binary reader positioned at the font source texture payload.</param>
    /// <param name="version">Serialized font payload version.</param>
    /// <returns>Deserialized source texture asset.</returns>
    static TextureAsset ReadFontSourceTexture(EngineBinaryReader reader, byte version) {
        if (reader == null) {
            throw new ArgumentNullException(nameof(reader));
        }

        TextureAsset sourceTexture = new TextureAsset();
        sourceTexture.RuntimeAssetId = version >= 2 ? (ulong)reader.ReadInt64() : 0ul;
        sourceTexture.Width = reader.ReadUInt16();
        sourceTexture.Height = reader.ReadUInt16();
        sourceTexture.ColorFormat = version >= 3
            ? ReadTextureAssetColorFormat(reader)
            : TextureAssetColorFormat.Rgba32;
        sourceTexture.AlphaPrecision = version >= 4
            ? ReadTextureAssetAlphaPrecision(reader)
            : GetDefaultTextureAssetAlphaPrecision(sourceTexture.ColorFormat);
        sourceTexture.PaletteColors = version >= 4 ? reader.ReadByteArray() : Array.Empty<byte>();
        sourceTexture.Colors = reader.ReadByteArray();
        return sourceTexture;
    }

    /// <summary>
    /// Reads one serialized font character map from the supplied packaged font stream.
    /// </summary>
    /// <param name="reader">Binary reader positioned at the font character payload.</param>
    /// <returns>Deserialized character map.</returns>
    static Dictionary<char, FontChar> ReadFontCharacters(EngineBinaryReader reader) {
        if (reader == null) {
            throw new ArgumentNullException(nameof(reader));
        }

        int characterCount = reader.ReadInt32();
        Dictionary<char, FontChar> characters = new Dictionary<char, FontChar>(characterCount);
        for (int index = 0; index < characterCount; index++) {
            char character = (char)reader.ReadUInt16();
            FontChar fontChar = new FontChar(
                reader.ReadFloat4(),
                reader.ReadSingle(),
                reader.ReadSingle(),
                reader.ReadSingle(),
                reader.ReadSingle());
            characters.Add(character, fontChar);
        }

        return characters;
    }

    /// <summary>
    /// Reads one serialized texture color format from the supplied packaged font stream.
    /// </summary>
    /// <param name="reader">Binary reader positioned at the serialized texture color-format byte.</param>
    /// <returns>Decoded texture color format.</returns>
    static TextureAssetColorFormat ReadTextureAssetColorFormat(EngineBinaryReader reader) {
        if (reader == null) {
            throw new ArgumentNullException(nameof(reader));
        }

        byte serializedValue = reader.ReadByte();
        if (serializedValue == (byte)TextureAssetColorFormat.Rgba32) {
            return TextureAssetColorFormat.Rgba32;
        } else if (serializedValue == (byte)TextureAssetColorFormat.Rgba4444) {
            return TextureAssetColorFormat.Rgba4444;
        } else if (serializedValue == (byte)TextureAssetColorFormat.Indexed4) {
            return TextureAssetColorFormat.Indexed4;
        } else if (serializedValue == (byte)TextureAssetColorFormat.Indexed8) {
            return TextureAssetColorFormat.Indexed8;
        } else if (serializedValue == (byte)TextureAssetColorFormat.GxRgb5A3) {
            return TextureAssetColorFormat.GxRgb5A3;
        }

        throw new InvalidOperationException($"Unsupported texture color format '{serializedValue}'.");
    }

    /// <summary>
    /// Reads one serialized texture alpha precision from the supplied packaged font stream.
    /// </summary>
    /// <param name="reader">Binary reader positioned at the serialized texture alpha-precision byte.</param>
    /// <returns>Decoded texture alpha precision.</returns>
    static TextureAssetAlphaPrecision ReadTextureAssetAlphaPrecision(EngineBinaryReader reader) {
        if (reader == null) {
            throw new ArgumentNullException(nameof(reader));
        }

        byte serializedValue = reader.ReadByte();
        if (serializedValue == (byte)TextureAssetAlphaPrecision.Opaque) {
            return TextureAssetAlphaPrecision.Opaque;
        } else if (serializedValue == (byte)TextureAssetAlphaPrecision.Binary) {
            return TextureAssetAlphaPrecision.Binary;
        } else if (serializedValue == (byte)TextureAssetAlphaPrecision.A4) {
            return TextureAssetAlphaPrecision.A4;
        } else if (serializedValue == (byte)TextureAssetAlphaPrecision.A8) {
            return TextureAssetAlphaPrecision.A8;
        }

        throw new InvalidOperationException($"Unsupported texture alpha precision '{serializedValue}'.");
    }

    /// <summary>
    /// Resolves the default alpha precision for legacy font atlas payloads that predate explicit metadata.
    /// </summary>
    /// <param name="colorFormat">Serialized texture color format used by the legacy atlas payload.</param>
    /// <returns>Best-effort alpha precision for the legacy payload.</returns>
    static TextureAssetAlphaPrecision GetDefaultTextureAssetAlphaPrecision(TextureAssetColorFormat colorFormat) {
        if (colorFormat == TextureAssetColorFormat.Rgba4444) {
            return TextureAssetAlphaPrecision.A4;
        }

        return TextureAssetAlphaPrecision.A8;
    }

    /// <summary>
    /// Writes one cooked texture asset to the supplied staging path.
    /// </summary>
    /// <param name="destinationPath">Absolute staging path that receives the cooked texture asset.</param>
    /// <param name="textureAsset">Cooked texture asset to serialize.</param>
    static void WriteTextureAsset(string destinationPath, TextureAsset textureAsset) {
        if (string.IsNullOrWhiteSpace(destinationPath)) {
            throw new ArgumentException("Destination path must be provided.", nameof(destinationPath));
        } else if (textureAsset == null) {
            throw new ArgumentNullException(nameof(textureAsset));
        }

        string directoryPath = Path.GetDirectoryName(destinationPath) ?? throw new InvalidOperationException("Destination directory path could not be resolved.");
        Directory.CreateDirectory(directoryPath);
        using FileStream stream = new FileStream(destinationPath, FileMode.Create, FileAccess.Write, FileShare.None);
        FilesAssetSerializer.Serialize(stream, textureAsset);
    }

    /// <summary>
    /// Writes one cooked font asset to the supplied staging path.
    /// </summary>
    /// <param name="destinationPath">Absolute staging path that receives the cooked font asset.</param>
    /// <param name="fontAsset">Cooked font asset to serialize.</param>
    static void WriteFontAsset(string destinationPath, FontAsset fontAsset) {
        if (string.IsNullOrWhiteSpace(destinationPath)) {
            throw new ArgumentException("Destination path must be provided.", nameof(destinationPath));
        } else if (fontAsset == null) {
            throw new ArgumentNullException(nameof(fontAsset));
        }

        string directoryPath = Path.GetDirectoryName(destinationPath) ?? throw new InvalidOperationException("Destination directory path could not be resolved.");
        Directory.CreateDirectory(directoryPath);
        using FileStream stream = new FileStream(destinationPath, FileMode.Create, FileAccess.Write, FileShare.None);
        FilesFontAssetBinarySerializer.Serialize(stream, fontAsset);
    }

    /// <summary>
    /// Resolves one work-item metadata value by key.
    /// </summary>
    /// <param name="workItem">Work item whose metadata should be searched.</param>
    /// <param name="key">Metadata key to resolve.</param>
    /// <returns>Matching metadata value when present; otherwise an empty string.</returns>
    static string ResolveMetadataValue(PlatformCookWorkItem workItem, string key) {
        if (workItem == null) {
            throw new ArgumentNullException(nameof(workItem));
        } else if (string.IsNullOrWhiteSpace(key)) {
            throw new ArgumentException("Metadata key must be provided.", nameof(key));
        }

        PlatformCookWorkItemMetadata[] metadata = workItem.Metadata ?? Array.Empty<PlatformCookWorkItemMetadata>();
        for (int index = 0; index < metadata.Length; index++) {
            PlatformCookWorkItemMetadata entry = metadata[index];
            if (entry != null && string.Equals(entry.Key, key, StringComparison.OrdinalIgnoreCase)) {
                return entry.Value ?? string.Empty;
            }
        }

        return string.Empty;
    }

    /// <summary>
    /// Resolves one source asset path beneath the project assets root.
    /// </summary>
    /// <param name="projectRootPath">Project root that owns the source asset.</param>
    /// <param name="sourceAssetPath">Relative or absolute source asset path emitted by the editor build graph.</param>
    /// <returns>Absolute source asset path.</returns>
    static string ResolveSourceAssetPath(string projectRootPath, string sourceAssetPath) {
        if (string.IsNullOrWhiteSpace(projectRootPath)) {
            throw new ArgumentException("Project root path must be provided.", nameof(projectRootPath));
        } else if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source asset path must be provided.", nameof(sourceAssetPath));
        }

        if (Path.IsPathRooted(sourceAssetPath)) {
            return Path.GetFullPath(sourceAssetPath);
        }
        return WiiTextureCookPath.Resolve(Path.Combine(projectRootPath, "assets"), sourceAssetPath);
    }

    /// <summary>
    /// Normalizes one relative path for the current host filesystem.
    /// </summary>
    /// <param name="relativePath">Relative path emitted by the editor build graph.</param>
    /// <returns>Host-normalized relative path.</returns>
    static string NormalizePath(string relativePath) {
        if (string.IsNullOrWhiteSpace(relativePath)) {
            throw new ArgumentException("Relative path must be provided.", nameof(relativePath));
        }

        return relativePath.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);
    }
}
