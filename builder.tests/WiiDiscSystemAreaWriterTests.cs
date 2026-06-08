using System.Buffers.Binary;

namespace helengine.wii.builder.tests;

/// <summary>
/// Verifies the synthesized Wii disc system-area files carry valid retail-style metadata.
/// </summary>
public sealed class WiiDiscSystemAreaWriterTests {
    /// <summary>
    /// Byte offset of the patchable apploader config block inside the fake native apploader template.
    /// </summary>
    const int TemplateConfigOffset = 0x40;

    /// <summary>
    /// Ensures the staged <c>boot.bin</c> preserves the supplied ID6 and Wii magic word.
    /// </summary>
    [Fact]
    public void Write_WritesBootBinWithDiscIdAndWiiMagic() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wii-system-area-tests", Guid.NewGuid().ToString("N"));
        string discRootPath = Path.Combine(workingRootPath, "disc");
        string filesRootPath = Path.Combine(discRootPath, "files");
        string nativeExecutablePath = Path.Combine(workingRootPath, "main.dol");

        try {
            Directory.CreateDirectory(filesRootPath);
            WriteFakeDol(nativeExecutablePath, 0x20U);
            WriteFakeApploaderTemplate(WiiNativeApploaderTemplatePathResolver.Resolve(nativeExecutablePath), 0x200);
            File.WriteAllText(Path.Combine(filesRootPath, "placeholder.bin"), "payload");

            WiiDiscSystemAreaWriter writer = new();
            writer.Write(discRootPath, nativeExecutablePath, new WiiDiscSystemAreaOptions("RCIE01", "city"));

            byte[] bootBytes = File.ReadAllBytes(Path.Combine(discRootPath, "sys", "boot.bin"));

            Assert.Equal("RCIE01", System.Text.Encoding.ASCII.GetString(bootBytes, 0, 6));
            Assert.Equal(0x5D1C9EA3U, ReadBigEndianUInt32(bootBytes, 0x18));
            Assert.Equal(0U, ReadBigEndianUInt32(bootBytes, 0x1C));
        }
        finally {
            if (Directory.Exists(workingRootPath)) {
                Directory.Delete(workingRootPath, true);
            }
        }
    }

    /// <summary>
    /// Ensures the staged <c>bi2.bin</c> contains sane nonzero defaults that match the disc region.
    /// </summary>
    [Fact]
    public void Write_WritesBi2BinWithRetailDefaults() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wii-system-area-tests", Guid.NewGuid().ToString("N"));
        string discRootPath = Path.Combine(workingRootPath, "disc");
        string filesRootPath = Path.Combine(discRootPath, "files");
        string nativeExecutablePath = Path.Combine(workingRootPath, "main.dol");

        try {
            Directory.CreateDirectory(filesRootPath);
            WriteFakeDol(nativeExecutablePath, 0x20U);
            WriteFakeApploaderTemplate(WiiNativeApploaderTemplatePathResolver.Resolve(nativeExecutablePath), 0x200);
            File.WriteAllText(Path.Combine(filesRootPath, "placeholder.bin"), "payload");

            WiiDiscSystemAreaWriter writer = new();
            writer.Write(discRootPath, nativeExecutablePath, new WiiDiscSystemAreaOptions("RCIE01", "city"));

            byte[] bi2Bytes = File.ReadAllBytes(Path.Combine(discRootPath, "sys", "bi2.bin"));

            Assert.Equal(0x01800000U, ReadBigEndianUInt32(bi2Bytes, 0x04));
            Assert.Equal(1U, ReadBigEndianUInt32(bi2Bytes, 0x18));
            Assert.Equal(1U, ReadBigEndianUInt32(bi2Bytes, 0x1C));
            Assert.Equal(1U, ReadBigEndianUInt32(bi2Bytes, 0x20));
        }
        finally {
            if (Directory.Exists(workingRootPath)) {
                Directory.Delete(workingRootPath, true);
            }
        }
    }

    /// <summary>
    /// Ensures the staged <c>boot.bin</c> carries nonzero DOL/FST layout fields derived from the extracted payload tree.
    /// </summary>
    [Fact]
    public void Write_WritesBootBinWithDolAndFstLayoutFields() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wii-system-area-tests", Guid.NewGuid().ToString("N"));
        string discRootPath = Path.Combine(workingRootPath, "disc");
        string filesRootPath = Path.Combine(discRootPath, "files");
        string nativeExecutablePath = Path.Combine(workingRootPath, "main.dol");

        try {
            Directory.CreateDirectory(Path.Combine(filesRootPath, "cooked", "subdir"));
            WriteFakeDol(nativeExecutablePath, 0x1138U);
            WriteFakeApploaderTemplate(WiiNativeApploaderTemplatePathResolver.Resolve(nativeExecutablePath), 0x200);
            File.WriteAllText(Path.Combine(filesRootPath, "cooked", "menu.heasset"), "menu");
            File.WriteAllText(Path.Combine(filesRootPath, "cooked", "subdir", "font.hasset"), "font");

            WiiDiscSystemAreaWriter writer = new();
            writer.Write(discRootPath, nativeExecutablePath, new WiiDiscSystemAreaOptions("RCIE01", "city"));

            byte[] bootBytes = File.ReadAllBytes(Path.Combine(discRootPath, "sys", "boot.bin"));

            uint expectedDolOffset = WiiDiscSystemAreaWriter.BootBinSize + WiiDiscSystemAreaWriter.Bi2BinSize + 0x200U;
            uint expectedFstSize = 100U;
            uint expectedFstOffset = expectedDolOffset + 0x1238U;

            Assert.Equal(expectedDolOffset, ReadBigEndianUInt32(bootBytes, 0x420));
            Assert.Equal(expectedFstOffset, ReadBigEndianUInt32(bootBytes, 0x424));
            Assert.Equal(expectedFstSize, ReadBigEndianUInt32(bootBytes, 0x428));
            Assert.Equal(expectedFstSize, ReadBigEndianUInt32(bootBytes, 0x42C));
        }
        finally {
            if (Directory.Exists(workingRootPath)) {
                Directory.Delete(workingRootPath, true);
            }
        }
    }

    /// <summary>
    /// Ensures the writer generates a disc-specific apploader image from the native template and patched DOL metadata.
    /// </summary>
    [Fact]
    public void Write_GeneratesPatchedApploaderImageFromNativeTemplate() {
        string workingRootPath = Path.Combine(Path.GetTempPath(), "wii-system-area-tests", Guid.NewGuid().ToString("N"));
        string discRootPath = Path.Combine(workingRootPath, "disc");
        string filesRootPath = Path.Combine(discRootPath, "files");
        string nativeExecutablePath = Path.Combine(workingRootPath, "main.dol");
        string nativeApploaderTemplatePath = WiiNativeApploaderTemplatePathResolver.Resolve(nativeExecutablePath);

        try {
            Directory.CreateDirectory(filesRootPath);
            File.WriteAllText(Path.Combine(filesRootPath, "placeholder.bin"), "payload");
            WriteFakeDol(nativeExecutablePath, 0x20U);
            WriteFakeApploaderTemplate(nativeApploaderTemplatePath, 0x200);

            WiiDiscSystemAreaWriter writer = new();
            writer.Write(discRootPath, nativeExecutablePath, new WiiDiscSystemAreaOptions("RCIE01", "city"));

            byte[] apploaderBytes = File.ReadAllBytes(Path.Combine(discRootPath, "sys", "apploader.img"));
            Assert.Equal(1U, ReadBigEndianUInt32(apploaderBytes, TemplateConfigOffset + 0x10));
            Assert.Equal(0x80004000U, ReadBigEndianUInt32(apploaderBytes, TemplateConfigOffset + 0x14));
            Assert.Equal(0x80005000U, ReadBigEndianUInt32(apploaderBytes, TemplateConfigOffset + 0x18));
            Assert.Equal(0x00000040U, ReadBigEndianUInt32(apploaderBytes, TemplateConfigOffset + 0x1C));
            Assert.Equal(0x80004000U, ReadBigEndianUInt32(apploaderBytes, TemplateConfigOffset + 0x30));
            Assert.Equal(0x00000020U, ReadBigEndianUInt32(apploaderBytes, TemplateConfigOffset + 0x34));
            Assert.Equal(0x000009D0U, ReadBigEndianUInt32(apploaderBytes, TemplateConfigOffset + 0x38));
        }
        finally {
            if (Directory.Exists(workingRootPath)) {
                Directory.Delete(workingRootPath, true);
            }
        }
    }

    /// <summary>
    /// Writes one minimal fake DOL with a single loadable section and caller-specified payload size.
    /// </summary>
    /// <param name="path">Path of the fake DOL to write.</param>
    /// <param name="payloadSize">Payload byte size of the single loadable DOL section.</param>
    static void WriteFakeDol(string path, uint payloadSize) {
        byte[] bytes = new byte[0x100 + payloadSize];
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(0x00, sizeof(uint)), 0x00000100U);
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(0x48, sizeof(uint)), 0x80004000U);
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(0x90, sizeof(uint)), payloadSize);
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(0xD8, sizeof(uint)), 0x80005000U);
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(0xDC, sizeof(uint)), 0x00000040U);
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(0xE0, sizeof(uint)), 0x80004000U);
        File.WriteAllBytes(path, bytes);
    }

    /// <summary>
    /// Writes one fake native apploader template that exposes the production patch marker at a stable offset.
    /// </summary>
    /// <param name="path">Path of the fake native apploader template to write.</param>
    /// <param name="length">Byte length of the fake native apploader template.</param>
    static void WriteFakeApploaderTemplate(string path, int length) {
        byte[] bytes = new byte[length];
        bytes[TemplateConfigOffset + 0x00] = 0x48;
        bytes[TemplateConfigOffset + 0x01] = 0x45;
        bytes[TemplateConfigOffset + 0x02] = 0x4C;
        bytes[TemplateConfigOffset + 0x03] = 0x41;
        bytes[TemplateConfigOffset + 0x04] = 0x50;
        bytes[TemplateConfigOffset + 0x05] = 0x50;
        bytes[TemplateConfigOffset + 0x06] = 0x4C;
        bytes[TemplateConfigOffset + 0x07] = 0x44;
        bytes[TemplateConfigOffset + 0x08] = 0x30;
        bytes[TemplateConfigOffset + 0x09] = 0x30;
        bytes[TemplateConfigOffset + 0x0A] = 0x31;
        bytes[TemplateConfigOffset + 0x0B] = 0x31;
        bytes[TemplateConfigOffset + 0x0C] = 0xA5;
        bytes[TemplateConfigOffset + 0x0D] = 0x5A;
        bytes[TemplateConfigOffset + 0x0E] = 0xA5;
        bytes[TemplateConfigOffset + 0x0F] = 0x5A;
        File.WriteAllBytes(path, bytes);
    }

    /// <summary>
    /// Reads one big-endian unsigned 32-bit value from the supplied byte array.
    /// </summary>
    /// <param name="bytes">Buffer that contains the requested value.</param>
    /// <param name="offset">Byte offset of the value.</param>
    /// <returns>Decoded unsigned 32-bit value.</returns>
    static uint ReadBigEndianUInt32(byte[] bytes, int offset) {
        return BinaryPrimitives.ReadUInt32BigEndian(bytes.AsSpan(offset, sizeof(uint)));
    }
}
