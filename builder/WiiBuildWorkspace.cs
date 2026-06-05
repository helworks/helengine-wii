using helengine.baseplatform.Manifest;
using helengine.baseplatform.Requests;

namespace helengine.wii.builder;

/// <summary>
/// Owns the Wii builder workspace operations that prepare generated-core runtime metadata.
/// </summary>
public static class WiiBuildWorkspace {
    /// <summary>
    /// Writes the Wii runtime scene manifest into the generated-core runtime folder for one resolved build request.
    /// </summary>
    /// <param name="request">Resolved build request that defines the generated core root and runtime scene metadata.</param>
    public static void WriteRuntimeSceneManifest(PlatformBuildRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (request.Manifest == null) {
            throw new ArgumentNullException(nameof(request.Manifest));
        }

        WiiBuilderPaths paths = WiiBuilderPaths.Create(request);
        Directory.CreateDirectory(paths.GeneratedCoreRootPath);
        new WiiRuntimeSceneManifestWriter().Write(paths.GeneratedCoreRootPath, request.Manifest);
    }

    /// <summary>
    /// Writes the Wii runtime scene manifest into the generated-core runtime folder for one explicit path set and manifest pair.
    /// </summary>
    /// <param name="paths">Resolved builder paths that expose the generated-core output root.</param>
    /// <param name="manifest">Resolved runtime scene metadata to embed.</param>
    public static void WriteRuntimeSceneManifest(WiiBuilderPaths paths, PlatformBuildManifest manifest) {
        if (paths == null) {
            throw new ArgumentNullException(nameof(paths));
        } else if (manifest == null) {
            throw new ArgumentNullException(nameof(manifest));
        }

        Directory.CreateDirectory(paths.GeneratedCoreRootPath);
        new WiiRuntimeSceneManifestWriter().Write(paths.GeneratedCoreRootPath, manifest);
    }
}
