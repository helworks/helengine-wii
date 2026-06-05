using System.Text;
using helengine.baseplatform.Manifest;

namespace helengine.wii.builder;

/// <summary>
/// Writes the Wii startup-scene and scene-catalog manifest consumed by the native Wii scene bootstrap.
/// </summary>
public sealed class WiiRuntimeSceneManifestWriter {
    /// <summary>
    /// Writes the header-only startup-scene and scene-catalog manifest into the generated-core runtime folder.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated core root that receives the runtime manifest files.</param>
    /// <param name="manifest">Resolved build manifest that defines the startup scene and cooked scene metadata.</param>
    public void Write(string generatedCoreRootPath, PlatformBuildManifest manifest) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        } else if (manifest == null) {
            throw new ArgumentNullException(nameof(manifest));
        }

        string runtimeRootPath = Path.Combine(generatedCoreRootPath, "runtime");
        Directory.CreateDirectory(runtimeRootPath);

        File.WriteAllText(Path.Combine(runtimeRootPath, "wii_runtime_scene_manifest.hpp"), BuildHeaderContents());
        File.WriteAllText(Path.Combine(runtimeRootPath, "wii_runtime_scene_manifest.inl"), BuildInlineContents(manifest));
    }

    /// <summary>
    /// Builds the header consumed by the native Wii scene bootstrap.
    /// </summary>
    /// <returns>Header source for the runtime scene manifest.</returns>
    static string BuildHeaderContents() {
        StringBuilder builder = new();
        builder.AppendLine("#pragma once");
        builder.AppendLine();
        builder.AppendLine("#include <cstddef>");
        builder.AppendLine();
        builder.AppendLine("struct HEWiiRuntimeSceneEntry {");
        builder.AppendLine("    const char* SceneId;");
        builder.AppendLine("    const char* CookedRelativePath;");
        builder.AppendLine("};");
        builder.AppendLine();
        builder.AppendLine("const char* he_get_runtime_wii_startup_scene_id();");
        builder.AppendLine("const HEWiiRuntimeSceneEntry* he_get_runtime_wii_scene_entries(std::size_t* count);");
        builder.AppendLine();
        builder.AppendLine("#include \"wii_runtime_scene_manifest.inl\"");
        return builder.ToString();
    }

    /// <summary>
    /// Builds the inline implementation that contains the startup-scene and scene-catalog data.
    /// </summary>
    /// <param name="manifest">Resolved build manifest whose scene metadata is being embedded.</param>
    /// <returns>Inline implementation source for the runtime scene manifest.</returns>
    static string BuildInlineContents(PlatformBuildManifest manifest) {
        string startupSceneId = ResolveStartupSceneId(manifest);
        StringBuilder builder = new();
        builder.AppendLine("static const char kRuntimeWiiStartupSceneId[] = \"" + EscapeCpp(startupSceneId) + "\";");
        builder.AppendLine("static const HEWiiRuntimeSceneEntry kRuntimeWiiSceneEntries[] = {");
        for (int index = 0; index < manifest.Scenes.Length; index++) {
            PlatformBuildScene scene = manifest.Scenes[index];
            builder.AppendLine(
                "    { \"" + EscapeCpp(scene.SceneId) + "\", \"" + EscapeCpp(ResolveCookedRelativePath(scene)) + "\" },");
        }

        builder.AppendLine("};");
        builder.AppendLine("static const std::size_t kRuntimeWiiSceneEntryCount = sizeof(kRuntimeWiiSceneEntries) / sizeof(kRuntimeWiiSceneEntries[0]);");
        builder.AppendLine();
        builder.AppendLine("inline const char* he_get_runtime_wii_startup_scene_id() {");
        builder.AppendLine("    return kRuntimeWiiStartupSceneId;");
        builder.AppendLine("}");
        builder.AppendLine();
        builder.AppendLine("inline const HEWiiRuntimeSceneEntry* he_get_runtime_wii_scene_entries(std::size_t* count) {");
        builder.AppendLine("    if (count != nullptr) {");
        builder.AppendLine("        *count = kRuntimeWiiSceneEntryCount;");
        builder.AppendLine("    }");
        builder.AppendLine();
        builder.AppendLine("    return kRuntimeWiiSceneEntries;");
        builder.AppendLine("}");
        return builder.ToString();
    }

    /// <summary>
    /// Resolves the startup scene id from the build manifest.
    /// </summary>
    /// <param name="manifest">Resolved build manifest.</param>
    /// <returns>Startup scene id that runtime-backed builds must expose.</returns>
    static string ResolveStartupSceneId(PlatformBuildManifest manifest) {
        if (string.IsNullOrWhiteSpace(manifest.StartupSceneId)) {
            throw new InvalidOperationException("The build manifest did not define a Wii startup scene id.");
        }

        return manifest.StartupSceneId;
    }

    /// <summary>
    /// Resolves the cooked-relative scene path from resolved metadata.
    /// </summary>
    /// <param name="scene">Resolved scene entry.</param>
    /// <returns>Cooked-relative path used by the runtime scene catalog.</returns>
    static string ResolveCookedRelativePath(PlatformBuildScene scene) {
        for (int index = 0; index < scene.ResolvedMetadata.Length; index++) {
            KeyValuePair<string, string> metadataEntry = scene.ResolvedMetadata[index];
            if (string.Equals(metadataEntry.Key, "cooked-relative-path", StringComparison.OrdinalIgnoreCase)
                && !string.IsNullOrWhiteSpace(metadataEntry.Value)) {
                return CanonicalPackagedAssetPath.Normalize(metadataEntry.Value);
            }
        }

        throw new InvalidOperationException($"The scene '{scene.SceneId}' did not define a cooked-relative-path metadata entry.");
    }

    /// <summary>
    /// Escapes one string value for embedding inside a C++ string literal.
    /// </summary>
    /// <param name="value">String value to escape.</param>
    /// <returns>Escaped C++ string-literal contents.</returns>
    static string EscapeCpp(string value) {
        return value.Replace("\\", "\\\\").Replace("\"", "\\\"");
    }
}
