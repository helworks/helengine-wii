namespace helengine.wii.builder;

/// <summary>
/// Copies one prebuilt loose content tree into the runtime root consumed by the Wii native host.
/// </summary>
public sealed class WiiLooseContentStager {
    /// <summary>
    /// Copies all files beneath one staged content source root into one runtime root while preserving relative paths.
    /// </summary>
    /// <param name="sourceRootPath">Existing content root whose children should be staged for runtime reads.</param>
    /// <param name="runtimeRootPath">Destination runtime root that receives the loose-file layout.</param>
    public void Stage(string sourceRootPath, string runtimeRootPath) {
        if (string.IsNullOrWhiteSpace(sourceRootPath)) {
            throw new ArgumentException("Source root path must be provided.", nameof(sourceRootPath));
        } else if (string.IsNullOrWhiteSpace(runtimeRootPath)) {
            throw new ArgumentException("Runtime root path must be provided.", nameof(runtimeRootPath));
        }
        if (!Directory.Exists(sourceRootPath)) {
            throw new DirectoryNotFoundException($"Source root path does not exist: {sourceRootPath}");
        }

        string fullSourceRootPath = Path.GetFullPath(sourceRootPath);
        string fullRuntimeRootPath = Path.GetFullPath(runtimeRootPath);
        Directory.CreateDirectory(fullRuntimeRootPath);

        string[] directories = Directory.GetDirectories(fullSourceRootPath, "*", SearchOption.AllDirectories);
        for (int index = 0; index < directories.Length; index++) {
            string relativeDirectoryPath = Path.GetRelativePath(fullSourceRootPath, directories[index]);
            string destinationDirectoryPath = Path.Combine(fullRuntimeRootPath, relativeDirectoryPath);
            Directory.CreateDirectory(destinationDirectoryPath);
        }

        string[] files = Directory.GetFiles(fullSourceRootPath, "*", SearchOption.AllDirectories);
        for (int index = 0; index < files.Length; index++) {
            string relativeFilePath = Path.GetRelativePath(fullSourceRootPath, files[index]);
            string destinationFilePath = Path.Combine(fullRuntimeRootPath, relativeFilePath);
            string destinationDirectoryPath = Path.GetDirectoryName(destinationFilePath);
            if (string.IsNullOrWhiteSpace(destinationDirectoryPath)) {
                throw new InvalidOperationException($"Destination directory could not be resolved for '{destinationFilePath}'.");
            }

            Directory.CreateDirectory(destinationDirectoryPath);
            File.Copy(files[index], destinationFilePath, true);
        }
    }
}
