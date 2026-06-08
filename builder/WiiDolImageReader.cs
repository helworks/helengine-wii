using System.Buffers.Binary;

namespace helengine.wii.builder;

/// <summary>
/// Parses the loadable section table and entry metadata from one Wii DOL file.
/// </summary>
public sealed class WiiDolImageReader {
    /// <summary>
    /// Number of text-section table entries stored in one DOL header.
    /// </summary>
    const int TextSectionCount = 7;

    /// <summary>
    /// Number of data-section table entries stored in one DOL header.
    /// </summary>
    const int DataSectionCount = 11;

    /// <summary>
    /// Byte size of one DOL header.
    /// </summary>
    const int DolHeaderSize = 0x100;

    /// <summary>
    /// Reads one DOL file and returns the loadable sections required by the generated apploader.
    /// </summary>
    /// <param name="path">Absolute DOL path to parse.</param>
    /// <returns>Parsed DOL image metadata.</returns>
    public WiiDolImage Read(string path) {
        if (string.IsNullOrWhiteSpace(path)) {
            throw new ArgumentException("DOL path is required.", nameof(path));
        } else if (!File.Exists(path)) {
            throw new FileNotFoundException("DOL file was not found.", path);
        }

        byte[] header = File.ReadAllBytes(path);
        if (header.Length < DolHeaderSize) {
            throw new InvalidOperationException("The supplied Wii DOL is too small to contain a valid DOL header.");
        }

        List<WiiDolLoadRequest> loadRequests = new(TextSectionCount + DataSectionCount);
        AppendLoadRequests(loadRequests, header, 0x00, 0x48, 0x90, TextSectionCount);
        AppendLoadRequests(loadRequests, header, 0x1C, 0x64, 0xAC, DataSectionCount);

        uint bssAddress = ReadBigEndianUInt32(header, 0xD8);
        uint bssSize = ReadBigEndianUInt32(header, 0xDC);
        uint entryPoint = ReadBigEndianUInt32(header, 0xE0);
        return new WiiDolImage(entryPoint, bssAddress, bssSize, loadRequests);
    }

    /// <summary>
    /// Appends all nonempty DOL sections from one header table.
    /// </summary>
    /// <param name="loadRequests">Destination request list.</param>
    /// <param name="header">DOL header bytes.</param>
    /// <param name="offsetTableOffset">Byte offset of the section file-offset table.</param>
    /// <param name="addressTableOffset">Byte offset of the section memory-address table.</param>
    /// <param name="lengthTableOffset">Byte offset of the section length table.</param>
    /// <param name="count">Number of table entries to scan.</param>
    static void AppendLoadRequests(
        List<WiiDolLoadRequest> loadRequests,
        byte[] header,
        int offsetTableOffset,
        int addressTableOffset,
        int lengthTableOffset,
        int count) {
        if (loadRequests == null) {
            throw new ArgumentNullException(nameof(loadRequests));
        } else if (header == null) {
            throw new ArgumentNullException(nameof(header));
        }

        for (int index = 0; index < count; index++) {
            uint fileOffset = ReadBigEndianUInt32(header, offsetTableOffset + (index * sizeof(uint)));
            uint memoryAddress = ReadBigEndianUInt32(header, addressTableOffset + (index * sizeof(uint)));
            uint length = ReadBigEndianUInt32(header, lengthTableOffset + (index * sizeof(uint)));
            if (memoryAddress != 0U && length != 0U) {
                loadRequests.Add(new WiiDolLoadRequest(fileOffset, memoryAddress, length));
            }
        }
    }

    /// <summary>
    /// Reads one big-endian unsigned 32-bit value from the supplied header bytes.
    /// </summary>
    /// <param name="header">Header buffer to read.</param>
    /// <param name="offset">Byte offset of the requested value.</param>
    /// <returns>Decoded unsigned 32-bit value.</returns>
    static uint ReadBigEndianUInt32(byte[] header, int offset) {
        return BinaryPrimitives.ReadUInt32BigEndian(header.AsSpan(offset, sizeof(uint)));
    }
}
