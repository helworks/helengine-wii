using System.Buffers.Binary;
using System.Text;

namespace helengine.wii.builder;

/// <summary>
/// Writes the required Wii FST-partition system files for one extracted-disc root before image packaging.
/// </summary>
public sealed class WiiDiscSystemAreaWriter {
    /// <summary>
    /// Size in bytes of one Wii <c>boot.bin</c> file.
    /// </summary>
    public const int BootBinSize = 0x440;

    /// <summary>
    /// Size in bytes of one Wii <c>bi2.bin</c> file.
    /// </summary>
    public const int Bi2BinSize = 0x2000;

    /// <summary>
    /// Disc magic written into the Wii partition boot header.
    /// </summary>
    const uint WiiDiscMagic = 0x5D1C9EA3;

    /// <summary>
    /// Simulated MEM1 size expected by standard retail Wii titles.
    /// </summary>
    const uint DefaultSimulatedMemorySize = 0x01800000;

    /// <summary>
    /// Indicates a normal retail-style boot without debug monitor routing.
    /// </summary>
    const uint NormalBootDebugFlag = 0U;

    /// <summary>
    /// Indicates a single-disc title in <c>bi2.bin</c>.
    /// </summary>
    const uint SingleDiscCount = 1U;

    /// <summary>
    /// Enables long filenames in the extracted-partition system metadata.
    /// </summary>
    const uint LongFileNameSupportEnabled = 1U;

    /// <summary>
    /// Native apploader template builder used to synthesize one disc-specific Wii apploader image.
    /// </summary>
    readonly WiiGeneratedApploaderImageBuilder GeneratedApploaderImageBuilder = new();

    /// <summary>
    /// Byte offset of the DOL offset field stored inside <c>boot.bin</c>.
    /// </summary>
    const int BootBinDolOffsetFieldOffset = 0x420;

    /// <summary>
    /// Byte offset of the FST offset field stored inside <c>boot.bin</c>.
    /// </summary>
    const int BootBinFstOffsetFieldOffset = 0x424;

    /// <summary>
    /// Byte offset of the FST size field stored inside <c>boot.bin</c>.
    /// </summary>
    const int BootBinFstSizeFieldOffset = 0x428;

    /// <summary>
    /// Byte offset of the FST max-size field stored inside <c>boot.bin</c>.
    /// </summary>
    const int BootBinFstMaxSizeFieldOffset = 0x42C;

    /// <summary>
    /// Writes the Wii system-area files into the supplied extracted-disc root.
    /// </summary>
    /// <param name="discRootPath">Destination extracted-disc root.</param>
    /// <param name="nativeExecutablePath">Built Wii executable to stage as <c>sys/main.dol</c>.</param>
    /// <param name="options">Explicit system-area inputs used to stage boot metadata.</param>
    public void Write(
        string discRootPath,
        string nativeExecutablePath,
        WiiDiscSystemAreaOptions options) {
        if (string.IsNullOrWhiteSpace(discRootPath)) {
            throw new ArgumentException("Disc root path is required.", nameof(discRootPath));
        } else if (string.IsNullOrWhiteSpace(nativeExecutablePath)) {
            throw new ArgumentException("Native executable path is required.", nameof(nativeExecutablePath));
        } else if (!File.Exists(nativeExecutablePath)) {
            throw new FileNotFoundException("Native Wii executable is required before disc staging.", nativeExecutablePath);
        } else if (options == null) {
            throw new ArgumentNullException(nameof(options));
        }

        string nativeApploaderTemplatePath = WiiNativeApploaderTemplatePathResolver.Resolve(nativeExecutablePath);
        if (!File.Exists(nativeApploaderTemplatePath)) {
            throw new FileNotFoundException("Native Wii apploader template is required before disc staging.", nativeApploaderTemplatePath);
        }

        string sysRootPath = Path.Combine(discRootPath, "sys");
        Directory.CreateDirectory(sysRootPath);

        string mainDolPath = Path.Combine(sysRootPath, "main.dol");
        string apploaderOutputPath = Path.Combine(sysRootPath, "apploader.img");
        File.Copy(nativeExecutablePath, mainDolPath, true);
        uint dolOffsetBytes = AlignToFourBytes(BootBinSize + Bi2BinSize + new FileInfo(nativeApploaderTemplatePath).Length);
        byte[] apploaderImageBytes = GeneratedApploaderImageBuilder.Build(nativeApploaderTemplatePath, nativeExecutablePath, dolOffsetBytes);
        File.WriteAllBytes(apploaderOutputPath, apploaderImageBytes);
        File.WriteAllBytes(Path.Combine(sysRootPath, "bi2.bin"), BuildBi2Bin(options));
        File.WriteAllBytes(Path.Combine(sysRootPath, "boot.bin"), BuildBootBin(options, discRootPath, mainDolPath, (uint)apploaderImageBytes.Length));
        File.WriteAllText(Path.Combine(discRootPath, "setup.txt"), BuildSetupContents(options));
    }

    /// <summary>
    /// Builds one synthetic Wii <c>boot.bin</c> header suitable for extracted-partition composition.
    /// </summary>
    /// <param name="options">Disc metadata to encode into the header.</param>
    /// <param name="discRootPath">Extracted disc root whose staged payload defines the FST layout.</param>
    /// <param name="mainDolPath">Staged Wii executable path under <c>sys/main.dol</c>.</param>
    /// <param name="apploaderImageSizeBytes">Byte size of the staged Wii apploader image.</param>
    /// <returns>Binary <c>boot.bin</c> payload.</returns>
    static byte[] BuildBootBin(
        WiiDiscSystemAreaOptions options,
        string discRootPath,
        string mainDolPath,
        uint apploaderImageSizeBytes) {
        byte[] buffer = new byte[BootBinSize];
        string normalizedDiscId = NormalizeDiscId(options.DiscId);
        Encoding ascii = Encoding.ASCII;
        ascii.GetBytes(normalizedDiscId, 0, normalizedDiscId.Length, buffer, 0);
        BinaryPrimitives.WriteUInt32BigEndian(buffer.AsSpan(0x18, sizeof(uint)), WiiDiscMagic);

        string normalizedTitle = NormalizeDiscTitle(options.DiscTitle);
        int titleByteCount = ascii.GetByteCount(normalizedTitle);
        ascii.GetBytes(normalizedTitle, 0, normalizedTitle.Length, buffer, 0x20);
        Array.Clear(buffer, 0x20 + titleByteCount, 0x400 - (0x20 + titleByteCount));

        uint dolOffsetBytes = AlignToFourBytes(BootBinSize + Bi2BinSize + apploaderImageSizeBytes);
        uint fstSizeBytes = CalculateFileSystemTableSize(Path.Combine(discRootPath, "files"));
        uint fstOffsetBytes = AlignToFourBytes(dolOffsetBytes + new FileInfo(mainDolPath).Length);
        BinaryPrimitives.WriteUInt32BigEndian(buffer.AsSpan(BootBinDolOffsetFieldOffset, sizeof(uint)), dolOffsetBytes);
        BinaryPrimitives.WriteUInt32BigEndian(buffer.AsSpan(BootBinFstOffsetFieldOffset, sizeof(uint)), fstOffsetBytes);
        BinaryPrimitives.WriteUInt32BigEndian(buffer.AsSpan(BootBinFstSizeFieldOffset, sizeof(uint)), fstSizeBytes);
        BinaryPrimitives.WriteUInt32BigEndian(buffer.AsSpan(BootBinFstMaxSizeFieldOffset, sizeof(uint)), fstSizeBytes);
        return buffer;
    }

    /// <summary>
    /// Calculates the byte size of the Wii FST that will be synthesized for the supplied extracted payload tree.
    /// </summary>
    /// <param name="filesRootPath">Extracted payload root staged under <c>files/</c>.</param>
    /// <returns>Aligned FST byte size.</returns>
    static uint CalculateFileSystemTableSize(string filesRootPath) {
        if (!Directory.Exists(filesRootPath)) {
            throw new DirectoryNotFoundException("The extracted Wii payload root was not found: " + filesRootPath);
        }

        FileSystemInfo[] entries = new DirectoryInfo(filesRootPath)
            .GetFileSystemInfos("*", SearchOption.AllDirectories);
        long stringTableSize = 0;
        for (int index = 0; index < entries.Length; index++) {
            stringTableSize += Encoding.UTF8.GetByteCount(entries[index].Name) + 1;
        }

        long entryTableSize = (entries.Length + 1L) * 12L;
        return AlignToFourBytes(entryTableSize + stringTableSize);
    }

    /// <summary>
    /// Aligns one byte count up to the next four-byte boundary.
    /// </summary>
    /// <param name="value">Raw byte count.</param>
    /// <returns>Four-byte-aligned byte count.</returns>
    static uint AlignToFourBytes(long value) {
        if (value < 0) {
            throw new ArgumentOutOfRangeException(nameof(value), "Alignment input must be nonnegative.");
        }

        long alignedValue = (value + 3L) & ~3L;
        if (alignedValue > uint.MaxValue) {
            throw new InvalidOperationException("Aligned Wii boot header values exceeded the supported 32-bit range.");
        }

        return (uint)alignedValue;
    }

    /// <summary>
    /// Builds one generic Wii <c>bi2.bin</c> payload for FST-partition composition.
    /// </summary>
    /// <returns>Binary <c>bi2.bin</c> payload.</returns>
    static byte[] BuildBi2Bin(WiiDiscSystemAreaOptions options) {
        if (options == null) {
            throw new ArgumentNullException(nameof(options));
        }

        byte[] buffer = new byte[Bi2BinSize];
        BinaryPrimitives.WriteUInt32BigEndian(buffer.AsSpan(0x04, sizeof(uint)), DefaultSimulatedMemorySize);
        BinaryPrimitives.WriteUInt32BigEndian(buffer.AsSpan(0x0C, sizeof(uint)), NormalBootDebugFlag);
        BinaryPrimitives.WriteUInt32BigEndian(buffer.AsSpan(0x18, sizeof(uint)), ResolveCountryCode(options.DiscId));
        BinaryPrimitives.WriteUInt32BigEndian(buffer.AsSpan(0x1C, sizeof(uint)), SingleDiscCount);
        BinaryPrimitives.WriteUInt32BigEndian(buffer.AsSpan(0x20, sizeof(uint)), LongFileNameSupportEnabled);
        return buffer;
    }

    /// <summary>
    /// Builds the Wii <c>setup.txt</c> file consumed by Wiimms ISO Tools while composing a DATA partition from the extracted directory.
    /// </summary>
    /// <param name="options">Disc metadata to encode into the setup file.</param>
    /// <returns>Text payload for <c>setup.txt</c>.</returns>
    static string BuildSetupContents(WiiDiscSystemAreaOptions options) {
        string normalizedDiscId = NormalizeDiscId(options.DiscId);
        string normalizedTitle = NormalizeDiscTitle(options.DiscTitle);
        return string.Join(
            Environment.NewLine,
            [
                "disc-type = Wii",
                "part-id = " + normalizedDiscId,
                "part-name = " + normalizedTitle
            ]) + Environment.NewLine;
    }

    /// <summary>
    /// Normalizes one authored disc identifier into one stable six-character Wii ID6 value.
    /// </summary>
    /// <param name="discId">Authored disc identifier.</param>
    /// <returns>Normalized six-character disc identifier.</returns>
    static string NormalizeDiscId(string discId) {
        StringBuilder builder = new(6);
        for (int index = 0; index < discId.Length && builder.Length < 6; index++) {
            char character = char.ToUpperInvariant(discId[index]);
            if (char.IsAsciiLetterOrDigit(character)) {
                builder.Append(character);
            }
        }

        while (builder.Length < 6) {
            builder.Append('X');
        }

        return builder.ToString();
    }

    /// <summary>
    /// Normalizes one authored disc title into the printable ASCII subset stored in Wii <c>boot.bin</c> and <c>setup.txt</c>.
    /// </summary>
    /// <param name="discTitle">Authored disc title.</param>
    /// <returns>Normalized disc title that fits the Wii header payload.</returns>
    static string NormalizeDiscTitle(string discTitle) {
        StringBuilder builder = new(0x3E0);
        for (int index = 0; index < discTitle.Length && builder.Length < 0x3E0; index++) {
            char character = discTitle[index];
            if (character >= 0x20 && character <= 0x7E) {
                builder.Append(character);
            }
        }

        if (builder.Length == 0) {
            builder.Append("HELENGINE WII");
        }

        return builder.ToString();
    }

    /// <summary>
    /// Resolves the Wii country code written into <c>bi2.bin</c> from the region byte encoded in the supplied ID6 value.
    /// </summary>
    /// <param name="discId">Six-character Wii disc identifier.</param>
    /// <returns>Wii country code stored in <c>bi2.bin</c>.</returns>
    static uint ResolveCountryCode(string discId) {
        string normalizedDiscId = NormalizeDiscId(discId);
        char regionCode = normalizedDiscId[3];
        if (regionCode == 'J' || regionCode == 'T') {
            return 0U;
        } else if (regionCode == 'E') {
            return 1U;
        } else if (regionCode == 'K') {
            return 4U;
        } else {
            return 2U;
        }
    }
}
