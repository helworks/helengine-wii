using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using helengine.baseplatform.Manifest;
using helengine.editor;
using FilesAssetSerializer = helengine.files.AssetSerializer;
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

    /// <summary>
    /// Executes the supplied Wii platform cook work items into the staging root.
    /// </summary>
    /// <param name="workItems">Work items emitted by the editor build graph.</param>
    /// <param name="projectRootPath">Project root used to resolve source asset paths.</param>
    /// <param name="stagingRootPath">Staging root that receives cooked platform-owned outputs.</param>
    public void Execute(PlatformCookWorkItem[] workItems, string projectRootPath, string stagingRootPath) {
        if (workItems == null) {
            throw new ArgumentNullException(nameof(workItems));
        } else if (string.IsNullOrWhiteSpace(projectRootPath)) {
            throw new ArgumentException("Project root path must be provided.", nameof(projectRootPath));
        } else if (string.IsNullOrWhiteSpace(stagingRootPath)) {
            throw new ArgumentException("Staging root path must be provided.", nameof(stagingRootPath));
        }

        for (int index = 0; index < workItems.Length; index++) {
            PlatformCookWorkItem workItem = workItems[index] ?? throw new InvalidOperationException("Platform cook work items must not contain null entries.");
            if (string.Equals(workItem.SourceAssetKind, "texture", StringComparison.OrdinalIgnoreCase)) {
                ExecuteTextureWorkItem(workItem, projectRootPath, stagingRootPath);
            } else if (string.Equals(workItem.SourceAssetKind, "font-atlas-texture", StringComparison.OrdinalIgnoreCase)) {
                ExecuteFontAtlasTextureWorkItem(workItem, projectRootPath, stagingRootPath);
            } else {
                throw new InvalidOperationException($"Wii builder does not support platform cook work item kind '{workItem.SourceAssetKind}'.");
            }
        }
    }

    /// <summary>
    /// Executes one builder-owned texture work item into the staging root.
    /// </summary>
    /// <param name="workItem">Texture work item emitted by the editor build graph.</param>
    /// <param name="projectRootPath">Project root used to resolve the source texture asset.</param>
    /// <param name="stagingRootPath">Staging root that receives the cooked texture asset.</param>
    void ExecuteTextureWorkItem(PlatformCookWorkItem workItem, string projectRootPath, string stagingRootPath) {
        TextureAsset sourceTexture = LoadSourceTexture(workItem, projectRootPath);
        WiiTextureCookSettings settings = WiiTextureCookSettings.Parse(workItem.SerializedPlatformSettings);
        TextureAsset cookedTexture = TextureCooker.CookTexture(sourceTexture, settings);
        WriteTextureAsset(Path.Combine(stagingRootPath, NormalizePath(workItem.OutputRelativePath)), cookedTexture);
    }

    /// <summary>
    /// Executes one builder-owned font-atlas texture work item into the staging root.
    /// </summary>
    /// <param name="workItem">Font-atlas work item emitted by the editor build graph.</param>
    /// <param name="projectRootPath">Project root used to resolve the source font asset.</param>
    /// <param name="stagingRootPath">Staging root that receives the cooked font asset.</param>
    void ExecuteFontAtlasTextureWorkItem(PlatformCookWorkItem workItem, string projectRootPath, string stagingRootPath) {
        string sourcePath = ResolveSourceAssetPath(projectRootPath, workItem.SourceAssetPath);
        FontAsset sourceFont = sourcePath.EndsWith(".hefont", StringComparison.OrdinalIgnoreCase)
            ? ReadFontAsset(sourcePath)
            : ImportSourceFont(workItem, sourcePath);
        if (sourceFont.SourceTextureAsset == null) {
            throw new InvalidOperationException($"Font asset '{workItem.SourceAssetPath}' did not contain a source texture atlas.");
        }

        WiiTextureCookSettings settings = WiiTextureCookSettings.Parse(workItem.SerializedPlatformSettings);
        TextureAsset cookedTexture = TextureCooker.CookTexture(sourceFont.SourceTextureAsset, settings);
        WriteTextureAsset(Path.Combine(stagingRootPath, NormalizePath(workItem.OutputRelativePath)), cookedTexture);
    }

    /// <summary>
    /// Imports one source font file into a raw font asset that still owns its RGBA32 atlas texture.
    /// </summary>
    /// <param name="workItem">Font-atlas work item emitted by the editor build graph.</param>
    /// <param name="sourcePath">Absolute source path to the authored font file.</param>
    /// <returns>Imported font asset ready for Wii atlas cooking.</returns>
    static FontAsset ImportSourceFont(PlatformCookWorkItem workItem, string sourcePath) {
        if (workItem == null) {
            throw new ArgumentNullException(nameof(workItem));
        } else if (string.IsNullOrWhiteSpace(sourcePath)) {
            throw new ArgumentException("Source path must be provided.", nameof(sourcePath));
        } else if (!File.Exists(sourcePath)) {
            throw new FileNotFoundException("Font source file was not found.", sourcePath);
        }

        using FileStream stream = new FileStream(sourcePath, FileMode.Open, FileAccess.Read, FileShare.Read);
        FontAsset sourceFont = new GdiFontImporter().ImportFont(stream);
        if (sourceFont.SourceTextureAsset == null) {
            throw new InvalidOperationException($"Font importer did not produce a source atlas for '{workItem.SourceAssetPath}'.");
        }

        string sourceAssetId = ResolveMetadataValue(workItem, "source-asset-id");
        string fontAtlasAssetId = sourceAssetId + "#atlas";
        sourceFont.SourceTextureAsset.Id = fontAtlasAssetId;
        sourceFont.SourceTextureAsset.RuntimeAssetId = RuntimeAssetIdGenerator.Generate(fontAtlasAssetId);
        return sourceFont;
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
        using Bitmap sourceBitmap = new Bitmap(sourcePath);
        int width = sourceBitmap.Width;
        int height = sourceBitmap.Height;
        if (width < 1 || height < 1 || width > ushort.MaxValue || height > ushort.MaxValue) {
            throw new InvalidOperationException($"Texture source '{workItem.SourceAssetPath}' produced unsupported dimensions '{width}x{height}'.");
        }

        Rectangle bounds = new Rectangle(0, 0, width, height);
        using Bitmap bitmap = sourceBitmap.Clone(bounds, PixelFormat.Format32bppArgb);
        BitmapData bitmapData = bitmap.LockBits(bounds, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        try {
            byte[] colors = ReadBitmapColors(bitmapData, width, height);
            return new TextureAsset {
                Id = ResolveMetadataValue(workItem, "source-asset-id"),
                Width = (ushort)width,
                Height = (ushort)height,
                Colors = colors,
                PaletteColors = Array.Empty<byte>(),
                ColorFormat = TextureAssetColorFormat.Rgba32,
                AlphaPrecision = TextureAssetAlphaPrecision.A8
            };
        } finally {
            bitmap.UnlockBits(bitmapData);
        }
    }

    /// <summary>
    /// Reads the logical RGBA32 colors from one 32-bit ARGB bitmap lock.
    /// </summary>
    /// <param name="bitmapData">Locked bitmap payload.</param>
    /// <param name="width">Bitmap width in pixels.</param>
    /// <param name="height">Bitmap height in pixels.</param>
    /// <returns>Tightly packed RGBA32 bytes.</returns>
    static byte[] ReadBitmapColors(BitmapData bitmapData, int width, int height) {
        if (bitmapData == null) {
            throw new ArgumentNullException(nameof(bitmapData));
        } else if (width < 1) {
            throw new ArgumentOutOfRangeException(nameof(width), "Bitmap width must be positive.");
        } else if (height < 1) {
            throw new ArgumentOutOfRangeException(nameof(height), "Bitmap height must be positive.");
        }

        const int BytesPerPixel = 4;
        int rowByteCount = width * BytesPerPixel;
        byte[] colors = new byte[width * height * BytesPerPixel];
        byte[] sourceRow = new byte[rowByteCount];
        for (int rowIndex = 0; rowIndex < height; rowIndex++) {
            IntPtr rowPointer = IntPtr.Add(bitmapData.Scan0, rowIndex * bitmapData.Stride);
            Marshal.Copy(rowPointer, sourceRow, 0, rowByteCount);

            int destinationRowOffset = rowIndex * rowByteCount;
            for (int columnIndex = 0; columnIndex < width; columnIndex++) {
                int sourceOffset = columnIndex * BytesPerPixel;
                int destinationOffset = destinationRowOffset + sourceOffset;
                colors[destinationOffset + 0] = sourceRow[sourceOffset + 2];
                colors[destinationOffset + 1] = sourceRow[sourceOffset + 1];
                colors[destinationOffset + 2] = sourceRow[sourceOffset + 0];
                colors[destinationOffset + 3] = sourceRow[sourceOffset + 3];
            }
        }

        return colors;
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

        string normalizedSourcePath = NormalizePath(sourceAssetPath);
        if (Path.IsPathRooted(normalizedSourcePath)) {
            return Path.GetFullPath(normalizedSourcePath);
        }

        return Path.GetFullPath(Path.Combine(projectRootPath, "assets", normalizedSourcePath));
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
