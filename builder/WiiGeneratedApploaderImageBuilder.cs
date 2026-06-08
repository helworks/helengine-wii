using System.Buffers.Binary;

namespace helengine.wii.builder;

/// <summary>
/// Patches the generic native Wii apploader template with disc-specific DOL load requests.
/// </summary>
public sealed class WiiGeneratedApploaderImageBuilder {
    /// <summary>
    /// Number of load requests reserved by the native apploader template.
    /// </summary>
    public const int RequestCapacity = 32;

    /// <summary>
    /// Byte offset of the request-count field inside the patchable apploader config block.
    /// </summary>
    const int RequestCountFieldOffset = 0x10;

    /// <summary>
    /// Byte offset of the DOL entry-point field inside the patchable apploader config block.
    /// </summary>
    const int EntryPointFieldOffset = 0x14;

    /// <summary>
    /// Byte offset of the BSS-address field inside the patchable apploader config block.
    /// </summary>
    const int BssAddressFieldOffset = 0x18;

    /// <summary>
    /// Byte offset of the BSS-size field inside the patchable apploader config block.
    /// </summary>
    const int BssSizeFieldOffset = 0x1C;

    /// <summary>
    /// Byte offset of the mutable current-request index inside the patchable apploader config block.
    /// </summary>
    const int CurrentRequestIndexFieldOffset = 0x20;

    /// <summary>
    /// Byte offset of the first load request inside the patchable apploader config block.
    /// </summary>
    const int RequestsFieldOffset = 0x30;

    /// <summary>
    /// Byte size of one patchable load-request record.
    /// </summary>
    const int RequestRecordSize = 0x0C;

    /// <summary>
    /// Patch marker that identifies the config block inside the native apploader template binary.
    /// </summary>
    static readonly byte[] ConfigMagicBytes = [
        0x48, 0x45, 0x4C, 0x41,
        0x50, 0x50, 0x4C, 0x44,
        0x30, 0x30, 0x31, 0x31,
        0xA5, 0x5A, 0xA5, 0x5A
    ];

    /// <summary>
    /// DOL reader used to translate one packaged-mode DOL into apploader load requests.
    /// </summary>
    readonly WiiDolImageReader DolImageReader = new();

    /// <summary>
    /// Builds one disc-specific apploader image from the generic native template and the packaged-mode DOL.
    /// </summary>
    /// <param name="apploaderTemplatePath">Generic native apploader template emitted beside the packaged-mode DOL.</param>
    /// <param name="nativeExecutablePath">Packaged-mode DOL path to load.</param>
    /// <param name="dolOffsetBytes">Disc byte offset where <c>sys/main.dol</c> will be staged.</param>
    /// <returns>Patched apploader image bytes ready to stage as <c>sys/apploader.img</c>.</returns>
    public byte[] Build(string apploaderTemplatePath, string nativeExecutablePath, uint dolOffsetBytes) {
        if (string.IsNullOrWhiteSpace(apploaderTemplatePath)) {
            throw new ArgumentException("Apploader template path is required.", nameof(apploaderTemplatePath));
        } else if (!File.Exists(apploaderTemplatePath)) {
            throw new FileNotFoundException("Native Wii apploader template was not found.", apploaderTemplatePath);
        } else if (string.IsNullOrWhiteSpace(nativeExecutablePath)) {
            throw new ArgumentException("Native executable path is required.", nameof(nativeExecutablePath));
        }

        byte[] apploaderBytes = File.ReadAllBytes(apploaderTemplatePath);
        int configOffset = FindConfigOffset(apploaderBytes);
        WiiDolImage dolImage = DolImageReader.Read(nativeExecutablePath);
        if (dolImage.LoadRequests.Count > RequestCapacity) {
            throw new InvalidOperationException($"The packaged Wii DOL exposes {dolImage.LoadRequests.Count} loadable sections, which exceeds the apploader template capacity of {RequestCapacity}.");
        }

        BinaryPrimitives.WriteUInt32BigEndian(apploaderBytes.AsSpan(configOffset + RequestCountFieldOffset, sizeof(uint)), (uint)dolImage.LoadRequests.Count);
        BinaryPrimitives.WriteUInt32BigEndian(apploaderBytes.AsSpan(configOffset + EntryPointFieldOffset, sizeof(uint)), dolImage.EntryPoint);
        BinaryPrimitives.WriteUInt32BigEndian(apploaderBytes.AsSpan(configOffset + BssAddressFieldOffset, sizeof(uint)), dolImage.BssAddress);
        BinaryPrimitives.WriteUInt32BigEndian(apploaderBytes.AsSpan(configOffset + BssSizeFieldOffset, sizeof(uint)), dolImage.BssSize);
        BinaryPrimitives.WriteUInt32BigEndian(apploaderBytes.AsSpan(configOffset + CurrentRequestIndexFieldOffset, sizeof(uint)), 0U);

        int requestRegionSize = RequestCapacity * RequestRecordSize;
        apploaderBytes.AsSpan(configOffset + RequestsFieldOffset, requestRegionSize).Clear();
        for (int index = 0; index < dolImage.LoadRequests.Count; index++) {
            PatchLoadRequest(apploaderBytes, configOffset, dolImage.LoadRequests[index], dolOffsetBytes, index);
        }

        return apploaderBytes;
    }

    /// <summary>
    /// Patches one load request entry inside the apploader config block.
    /// </summary>
    /// <param name="apploaderBytes">Patchable apploader image bytes.</param>
    /// <param name="configOffset">Byte offset of the config block inside the apploader image.</param>
    /// <param name="loadRequest">Parsed DOL section request.</param>
    /// <param name="dolOffsetBytes">Disc byte offset of <c>sys/main.dol</c>.</param>
    /// <param name="index">Zero-based request index to patch.</param>
    static void PatchLoadRequest(byte[] apploaderBytes, int configOffset, WiiDolLoadRequest loadRequest, uint dolOffsetBytes, int index) {
        if (apploaderBytes == null) {
            throw new ArgumentNullException(nameof(apploaderBytes));
        } else if (loadRequest == null) {
            throw new ArgumentNullException(nameof(loadRequest));
        }

        ulong discOffsetBytes = (ulong)dolOffsetBytes + loadRequest.FileOffset;
        if ((discOffsetBytes & 0x3UL) != 0UL) {
            throw new InvalidOperationException("Generated Wii apploader requests require four-byte-aligned DOL section offsets.");
        }

        uint discOffsetWords = checked((uint)(discOffsetBytes >> 2));
        int requestOffset = configOffset + RequestsFieldOffset + (index * RequestRecordSize);
        BinaryPrimitives.WriteUInt32BigEndian(apploaderBytes.AsSpan(requestOffset + 0x00, sizeof(uint)), loadRequest.MemoryAddress);
        BinaryPrimitives.WriteUInt32BigEndian(apploaderBytes.AsSpan(requestOffset + 0x04, sizeof(uint)), loadRequest.Length);
        BinaryPrimitives.WriteUInt32BigEndian(apploaderBytes.AsSpan(requestOffset + 0x08, sizeof(uint)), discOffsetWords);
    }

    /// <summary>
    /// Locates the patchable config block inside the generic native apploader template.
    /// </summary>
    /// <param name="apploaderBytes">Generic native apploader template bytes.</param>
    /// <returns>Byte offset of the config block.</returns>
    static int FindConfigOffset(byte[] apploaderBytes) {
        if (apploaderBytes == null) {
            throw new ArgumentNullException(nameof(apploaderBytes));
        }

        for (int index = 0; index <= apploaderBytes.Length - ConfigMagicBytes.Length; index++) {
            if (apploaderBytes.AsSpan(index, ConfigMagicBytes.Length).SequenceEqual(ConfigMagicBytes)) {
                return index;
            }
        }

        throw new InvalidOperationException("The native Wii apploader template does not contain the expected patch marker.");
    }
}
