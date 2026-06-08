namespace helengine.wii.builder;

/// <summary>
/// Writes the extracted Wii disc root consumed by the first image-packaging path.
/// </summary>
public sealed class WiiDiscLayoutWriter {
    /// <summary>
    /// System-area writer used to stage the required Wii packaging metadata and executable files.
    /// </summary>
    readonly WiiDiscSystemAreaWriter SystemAreaWriter = new();

    /// <summary>
    /// Writes the Wii system area plus the <c>files/</c> content tree for one packaged Wii build.
    /// </summary>
    /// <param name="stagingRootPath">Source staging root that contains cooked payloads.</param>
    /// <param name="nativeExecutablePath">Built Wii executable to copy into the disc root.</param>
    /// <param name="discRootPath">Destination extracted-disc root.</param>
    /// <param name="systemAreaOptions">Explicit system-area inputs used to stage Wii disc metadata and bootstrap files.</param>
    /// <returns>Staged extracted-disc layout metadata.</returns>
    public WiiDiscLayoutResult Write(
        string stagingRootPath,
        string nativeExecutablePath,
        string discRootPath,
        WiiDiscSystemAreaOptions systemAreaOptions) {
        if (!Directory.Exists(stagingRootPath)) {
            throw new DirectoryNotFoundException($"Staging root '{stagingRootPath}' was not found.");
        } else if (string.IsNullOrWhiteSpace(nativeExecutablePath)) {
            throw new ArgumentException("Native executable path is required.", nameof(nativeExecutablePath));
        } else if (!File.Exists(nativeExecutablePath)) {
            throw new FileNotFoundException("Built Wii executable was not found.", nativeExecutablePath);
        } else if (string.IsNullOrWhiteSpace(discRootPath)) {
            throw new ArgumentException("Disc root path is required.", nameof(discRootPath));
        } else if (systemAreaOptions == null) {
            throw new ArgumentNullException(nameof(systemAreaOptions));
        }

        if (Directory.Exists(discRootPath)) {
            Directory.Delete(discRootPath, recursive: true);
        }

        Dictionary<string, string> logicalToPhysicalPaths = new(StringComparer.OrdinalIgnoreCase);
        string[] filePaths = Directory.GetFiles(stagingRootPath, "*", SearchOption.AllDirectories);
        for (int index = 0; index < filePaths.Length; index++) {
            string sourcePath = filePaths[index];
            string logicalRelativePath = Path.GetRelativePath(stagingRootPath, sourcePath).Replace('\\', '/');
            string physicalRelativePath = WiiDiscPathResolver.ResolvePhysicalRelativePath(logicalRelativePath);
            string destinationPath = Path.Combine(discRootPath, physicalRelativePath.Replace('/', Path.DirectorySeparatorChar));
            string destinationDirectoryPath = Path.GetDirectoryName(destinationPath)
                ?? throw new InvalidOperationException($"Could not resolve the destination directory for '{destinationPath}'.");
            Directory.CreateDirectory(destinationDirectoryPath);
            File.Copy(sourcePath, destinationPath, true);
            logicalToPhysicalPaths[logicalRelativePath] = physicalRelativePath;
        }

        SystemAreaWriter.Write(discRootPath, nativeExecutablePath, systemAreaOptions);
        string discExecutablePath = Path.Combine(discRootPath, "sys", "main.dol");

        return new WiiDiscLayoutResult(discRootPath, discExecutablePath, logicalToPhysicalPaths);
    }
}
