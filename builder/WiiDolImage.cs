namespace helengine.wii.builder;

/// <summary>
/// Captures the DOL entry point, BSS range, and loadable sections needed to generate one Wii apploader image.
/// </summary>
public sealed class WiiDolImage {
    /// <summary>
    /// Initializes one parsed DOL description.
    /// </summary>
    /// <param name="entryPoint">Entry point address of the DOL.</param>
    /// <param name="bssAddress">Start address of the DOL BSS range.</param>
    /// <param name="bssSize">Size of the DOL BSS range in bytes.</param>
    /// <param name="loadRequests">Loadable DOL sections that must be requested from disc.</param>
    public WiiDolImage(
        uint entryPoint,
        uint bssAddress,
        uint bssSize,
        IReadOnlyList<WiiDolLoadRequest> loadRequests) {
        if (entryPoint == 0U) {
            throw new ArgumentException("Entry point must be nonzero.", nameof(entryPoint));
        } else if (loadRequests == null) {
            throw new ArgumentNullException(nameof(loadRequests));
        } else if (loadRequests.Count == 0) {
            throw new ArgumentException("At least one DOL load request is required.", nameof(loadRequests));
        }

        EntryPoint = entryPoint;
        BssAddress = bssAddress;
        BssSize = bssSize;
        LoadRequests = loadRequests;
    }

    /// <summary>
    /// Gets the entry point address of the DOL.
    /// </summary>
    public uint EntryPoint { get; }

    /// <summary>
    /// Gets the start address of the DOL BSS range.
    /// </summary>
    public uint BssAddress { get; }

    /// <summary>
    /// Gets the size of the DOL BSS range in bytes.
    /// </summary>
    public uint BssSize { get; }

    /// <summary>
    /// Gets the loadable DOL sections that must be requested from disc.
    /// </summary>
    public IReadOnlyList<WiiDolLoadRequest> LoadRequests { get; }
}
