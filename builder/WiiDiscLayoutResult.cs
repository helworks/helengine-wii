namespace helengine.wii.builder;

/// <summary>
/// Carries the staged extracted-disc paths produced by the Wii disc layout writer.
/// </summary>
public sealed class WiiDiscLayoutResult {
    /// <summary>
    /// Initializes one extracted-disc layout result.
    /// </summary>
    /// <param name="discRootPath">Path to the staged extracted-disc root.</param>
    /// <param name="discExecutablePath">Path to the staged <c>sys/main.dol</c> executable.</param>
    /// <param name="logicalToPhysicalPaths">Logical cooked paths mapped to extracted-disc physical paths.</param>
    public WiiDiscLayoutResult(
        string discRootPath,
        string discExecutablePath,
        IReadOnlyDictionary<string, string> logicalToPhysicalPaths) {
        if (string.IsNullOrWhiteSpace(discRootPath)) {
            throw new ArgumentException("Disc root path is required.", nameof(discRootPath));
        } else if (string.IsNullOrWhiteSpace(discExecutablePath)) {
            throw new ArgumentException("Disc executable path is required.", nameof(discExecutablePath));
        } else if (logicalToPhysicalPaths == null) {
            throw new ArgumentNullException(nameof(logicalToPhysicalPaths));
        }

        DiscRootPath = discRootPath;
        DiscExecutablePath = discExecutablePath;
        LogicalToPhysicalPaths = logicalToPhysicalPaths;
    }

    /// <summary>
    /// Gets the staged extracted-disc root path.
    /// </summary>
    public string DiscRootPath { get; }

    /// <summary>
    /// Gets the staged Wii executable path under <c>sys/main.dol</c>.
    /// </summary>
    public string DiscExecutablePath { get; }

    /// <summary>
    /// Gets the logical-to-physical mapping produced during disc staging.
    /// </summary>
    public IReadOnlyDictionary<string, string> LogicalToPhysicalPaths { get; }
}
