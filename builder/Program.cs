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

        if (args.Length == 3 && string.Equals(args[0], "--stage-runtime-content", StringComparison.OrdinalIgnoreCase)) {
            string sourceRootPath = args[1];
            string runtimeRootPath = args[2];
            new WiiLooseContentStager().Stage(sourceRootPath, runtimeRootPath);
            Console.WriteLine(Path.Combine(runtimeRootPath, "cooked"));
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

        if (args.Length == 6 && string.Equals(args[0], "--write-disc-layout", StringComparison.OrdinalIgnoreCase)) {
            string stagingRootPath = args[1];
            string nativeExecutablePath = args[2];
            string discRootPath = args[3];
            string discId = args[4];
            string discTitle = args[5];
            new WiiDiscLayoutWriter().Write(
                stagingRootPath,
                nativeExecutablePath,
                discRootPath,
                new WiiDiscSystemAreaOptions(discId, discTitle));
            Console.WriteLine(discRootPath);
            return 0;
        }

        if (args.Length == 3 && string.Equals(args[0], "--package-image", StringComparison.OrdinalIgnoreCase)) {
            string discRootPath = args[1];
            string outputImagePath = args[2];
            string witExecutablePath = Environment.GetEnvironmentVariable("HELENGINE_WII_WIT_PATH");
            new WiiWiimmsIsoToolsImagePackager(
                new WiiWiimmsIsoToolsOptions(witExecutablePath),
                new WiiProcessRunner()).Package(
                    new WiiDiscLayoutResult(
                        discRootPath,
                        Path.Combine(discRootPath, "sys", "main.dol"),
                        new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)),
                    outputImagePath,
                    CancellationToken.None);
            Console.WriteLine(outputImagePath);
            return 0;
        }

        if (args.Length >= 6
            && string.Equals(args[0], "--stage-runtime-generated-modules", StringComparison.OrdinalIgnoreCase)
            && ((args.Length - 4) % 2) == 0) {
            string generatedCoreRootPath = args[1];
            string codeRootPath = args[2];
            string cookedSceneAssetPath = args[3];
            Dictionary<string, string> runtimeAssemblyPathsByModuleId = ParseRuntimeAssemblyPathsByModuleId(args, 4);
            new WiiRuntimeGeneratedModuleStager().Stage(
                generatedCoreRootPath,
                codeRootPath,
                [cookedSceneAssetPath],
                runtimeAssemblyPathsByModuleId);
            Console.WriteLine(Path.Combine(generatedCoreRootPath, "GeneratedRuntimeComponentDeserializerRegistration.cpp"));
            return 0;
        }

        Console.WriteLine("helengine.wii.builder --describe | --smoke-test | --stage-runtime-content <source-root> <runtime-root> | --write-runtime-scene-manifest <generated-core-root> <startup-scene-id> <scene-id> <cooked-relative-path> | --write-disc-layout <staging-root> <native-executable> <disc-root> <disc-id> <disc-title> | --package-image <disc-root> <output-image> | --stage-runtime-generated-modules <generated-core-root> <code-root> <cooked-scene-asset-path> <module-id> <assembly-path> [<module-id> <assembly-path> ...]");
        return 0;
    }

    /// <summary>
    /// Parses runtime assembly paths keyed by module id from the supplied command-line argument tail.
    /// </summary>
    /// <param name="args">Full command-line argument vector.</param>
    /// <param name="startIndex">Index of the first module-id token in the argument vector.</param>
    /// <returns>Runtime assembly paths keyed by module id.</returns>
    static Dictionary<string, string> ParseRuntimeAssemblyPathsByModuleId(string[] args, int startIndex) {
        if (args == null) {
            throw new ArgumentNullException(nameof(args));
        }
        if (startIndex < 0 || startIndex > args.Length) {
            throw new ArgumentOutOfRangeException(nameof(startIndex));
        }

        Dictionary<string, string> runtimeAssemblyPathsByModuleId = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        for (int index = startIndex; index < args.Length; index += 2) {
            string moduleId = args[index];
            string assemblyPath = args[index + 1];
            if (string.IsNullOrWhiteSpace(moduleId)) {
                throw new InvalidOperationException("Runtime assembly module ids must be provided.");
            }
            if (string.IsNullOrWhiteSpace(assemblyPath)) {
                throw new InvalidOperationException($"Runtime assembly path for module '{moduleId}' must be provided.");
            }

            runtimeAssemblyPathsByModuleId[moduleId] = assemblyPath;
        }

        return runtimeAssemblyPathsByModuleId;
    }
}
