namespace helengine.wii.builder;

/// <summary>
/// Resolves logical cooked relative paths into extracted-disc paths rooted under the Wii <c>files/</c> tree.
/// </summary>
public static class WiiDiscPathResolver {
    /// <summary>
    /// Resolves one logical cooked path into a deterministic extracted-disc relative path.
    /// </summary>
    /// <param name="logicalRelativePath">Logical cooked relative path from the build manifest.</param>
    /// <returns>Normalized extracted-disc relative path rooted under <c>files/</c>.</returns>
    public static string ResolvePhysicalRelativePath(string logicalRelativePath) {
        if (string.IsNullOrWhiteSpace(logicalRelativePath)) {
            throw new ArgumentException("Logical relative path must be provided.", nameof(logicalRelativePath));
        }

        string normalizedPath = logicalRelativePath.Replace('\\', '/').TrimStart('/');
        if (normalizedPath.StartsWith("files/", StringComparison.OrdinalIgnoreCase)) {
            return normalizedPath;
        }

        return "files/" + normalizedPath;
    }
}
