namespace helengine.wii.builder;

/// <summary>
/// Translates WSL-mounted Windows paths into native Windows paths for external tool invocations.
/// </summary>
public static class WiiWslWindowsPathTranslator {
    /// <summary>
    /// Attempts to convert one WSL-mounted Windows path such as <c>/mnt/c/dev/file.bin</c> into a native Windows path.
    /// </summary>
    /// <param name="path">Candidate WSL-mounted Windows path.</param>
    /// <returns>Converted Windows path.</returns>
    public static string ConvertToWindowsPath(string path) {
        if (string.IsNullOrWhiteSpace(path)) {
            throw new ArgumentException("Path is required.", nameof(path));
        }

        string normalizedPath = path.Replace('\\', '/');
        if (!normalizedPath.StartsWith("/mnt/", StringComparison.OrdinalIgnoreCase) || normalizedPath.Length < 7 || normalizedPath[6] != '/') {
            throw new InvalidOperationException($"Path '{path}' is not a WSL-mounted Windows path under /mnt/<drive>/.");
        }

        char driveLetter = char.ToUpperInvariant(normalizedPath[5]);
        string relativePath = normalizedPath.Substring(7).Replace('/', '\\');
        return driveLetter + ":\\" + relativePath;
    }
}
