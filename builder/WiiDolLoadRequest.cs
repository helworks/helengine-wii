namespace helengine.wii.builder;

/// <summary>
/// Describes one loadable DOL section that the generated Wii apploader must request from disc.
/// </summary>
public sealed class WiiDolLoadRequest {
    /// <summary>
    /// Initializes one DOL load request.
    /// </summary>
    /// <param name="fileOffset">Byte offset of the section inside the DOL file.</param>
    /// <param name="memoryAddress">Destination memory address of the section.</param>
    /// <param name="length">Length of the section in bytes.</param>
    public WiiDolLoadRequest(uint fileOffset, uint memoryAddress, uint length) {
        if (memoryAddress == 0U) {
            throw new ArgumentException("Memory address must be nonzero.", nameof(memoryAddress));
        } else if (length == 0U) {
            throw new ArgumentException("Length must be nonzero.", nameof(length));
        }

        FileOffset = fileOffset;
        MemoryAddress = memoryAddress;
        Length = length;
    }

    /// <summary>
    /// Gets the byte offset of the section inside the DOL file.
    /// </summary>
    public uint FileOffset { get; }

    /// <summary>
    /// Gets the destination memory address of the section.
    /// </summary>
    public uint MemoryAddress { get; }

    /// <summary>
    /// Gets the length of the section in bytes.
    /// </summary>
    public uint Length { get; }
}
