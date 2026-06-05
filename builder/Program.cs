using helengine.baseplatform.Manifest;

namespace helengine.wii.builder;

/// <summary>
/// Provides a small command-line entrypoint for the Wii builder assembly.
/// </summary>
public static class Program {
    /// <summary>
    /// Runs the builder smoke mode or prints the builder identity.
    /// </summary>
    /// <param name="args">Command-line arguments.</param>
    /// <returns>Zero on success.</returns>
    public static int Main(string[] args) {
        if (args.Length > 0 && string.Equals(args[0], "--describe", StringComparison.OrdinalIgnoreCase)) {
            Console.WriteLine("helengine.wii.builder");
            Console.WriteLine("wii");
            Console.WriteLine("Nintendo Wii");
            return 0;
        }

        if (args.Length > 0 && string.Equals(args[0], "--smoke-test", StringComparison.OrdinalIgnoreCase)) {
            Console.WriteLine("wii.builder smoke test entrypoint (runtime-scene-manifest foundation)");
            return 0;
        }

        if (args.Length == 5 && string.Equals(args[0], "--write-runtime-scene-manifest", StringComparison.OrdinalIgnoreCase)) {
            string generatedCoreRootPath = args[1];
            string startupSceneId = args[2];
            string sceneId = args[3];
            string cookedRelativePath = args[4];
            PlatformBuildScene scene = new(
                sceneId,
                sceneId,
                cookedRelativePath,
                [new PlatformBuildPayloadReference(sceneId, cookedRelativePath)],
                [new KeyValuePair<string, string>(PlatformBuildSceneMetadataKeys.CookedRelativePath, cookedRelativePath)]);
            PlatformBuildManifest manifest = new(
                1,
                "helengine-wii",
                "1.0.0",
                "1.0.0",
                "wii",
                "1.0.0",
                startupSceneId,
                [scene],
                Array.Empty<PlatformBuildAsset>(),
                Array.Empty<PlatformBuildArtifact>(),
                Array.Empty<PlatformBuildCodeModule>(),
                Array.Empty<PlatformArtifactPlacement>(),
                new PlatformContainerWritePlan(string.Empty, Array.Empty<PlatformContainerArtifact>()));
            new WiiRuntimeSceneManifestWriter().Write(generatedCoreRootPath, manifest);
            Console.WriteLine(Path.Combine(generatedCoreRootPath, "runtime", "wii_runtime_scene_manifest.hpp"));
            return 0;
        }

        Console.WriteLine("helengine.wii.builder --describe | --smoke-test | --write-runtime-scene-manifest <generated-core-root> <startup-scene-id> <scene-id> <cooked-relative-path>");
        return 0;
    }
}
