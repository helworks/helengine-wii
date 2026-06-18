using helengine.baseplatform.Manifest;

namespace helengine.wii.builder.tests;

/// <summary>
/// Verifies the packaged Wii runtime scene manifest emitted for native startup.
/// </summary>
public sealed class WiiRuntimeSceneManifestWriterTests {
    /// <summary>
    /// Ensures the writer emits a startup-scene function and canonical cooked scene catalog paths that the native bootstrap can consume.
    /// </summary>
    [Fact]
    public void Write_EmitsHeaderWithStartupSceneAndCanonicalCookedSceneCatalog() {
        string outputRootPath = Path.Combine(Path.GetTempPath(), "wii-runtime-manifest-tests", Guid.NewGuid().ToString("N"));
        const string StartupSceneId = "Scenes/DemoDiscMainMenu.helen";
        const string StartupSceneCookedRelativePath = "cooked/scenes/DemoDiscMainMenu.hasset";
        PlatformBuildManifest manifest = new(
            1,
            "project",
            "1.0.0",
            "1.0.0",
            "wii",
            "1.0.0",
            StartupSceneId,
            [
                new PlatformBuildScene(
                    StartupSceneId,
                    "Demo Disc Main Menu",
                    "Scenes/DemoDiscMainMenu.helen",
                    [],
                    [new KeyValuePair<string, string>(PlatformBuildSceneMetadataKeys.CookedRelativePath, StartupSceneCookedRelativePath)])
            ],
            Array.Empty<PlatformBuildAsset>(),
            Array.Empty<PlatformBuildArtifact>(),
            Array.Empty<PlatformBuildCodeModule>(),
            Array.Empty<PlatformArtifactPlacement>(),
            new PlatformContainerWritePlan("wii-runtime-layout", Array.Empty<PlatformContainerArtifact>()));

        try {
            WiiRuntimeSceneManifestWriter writer = new();
            writer.Write(outputRootPath, manifest);

            string header = File.ReadAllText(Path.Combine(outputRootPath, "runtime", "wii_runtime_scene_manifest.hpp"));
            Assert.Contains("he_get_runtime_wii_startup_scene_id", header, StringComparison.Ordinal);
            Assert.Contains("he_get_runtime_wii_scene_entries", header, StringComparison.Ordinal);

            string source = File.ReadAllText(Path.Combine(outputRootPath, "runtime", "wii_runtime_scene_manifest.inl"));
            Assert.Contains("\"Scenes/DemoDiscMainMenu.helen\"", source, StringComparison.Ordinal);
            Assert.Contains("\"cooked/scenes/demodiscmainmenu.hasset\"", source, StringComparison.Ordinal);
            Assert.DoesNotContain("\"files/cooked/scenes/demodiscmainmenu.hasset\"", source, StringComparison.Ordinal);
        } finally {
            if (Directory.Exists(outputRootPath)) {
                Directory.Delete(outputRootPath, recursive: true);
            }
        }
    }
}
