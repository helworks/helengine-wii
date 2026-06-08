namespace helengine.wii.builder;

/// <summary>
/// Resolves the staged native Wii apploader template path that accompanies one packaged-mode DOL output.
/// </summary>
public static class WiiNativeApploaderTemplatePathResolver {
    /// <summary>
    /// File name of the generic native apploader template emitted beside the packaged-mode DOL.
    /// </summary>
    public const string FileName = "helengine_wii_apploader_template.bin";

    /// <summary>
    /// Resolves the staged native apploader template path beside the supplied packaged-mode DOL path.
    /// </summary>
    /// <param name="nativeExecutablePath">Packaged-mode DOL path whose sibling directory stores the apploader template.</param>
    /// <returns>Absolute path of the staged native apploader template.</returns>
    public static string Resolve(string nativeExecutablePath) {
        if (string.IsNullOrWhiteSpace(nativeExecutablePath)) {
            throw new ArgumentException("Native executable path is required.", nameof(nativeExecutablePath));
        }

        string nativeExecutableDirectoryPath = Path.GetDirectoryName(nativeExecutablePath)
            ?? throw new InvalidOperationException("Native executable directory path could not be resolved.");
        return Path.Combine(nativeExecutableDirectoryPath, FileName);
    }
}
