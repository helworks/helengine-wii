using System.Text.Json;
using helengine.baseplatform.Builders;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using FilesAssetSerializer = helengine.files.AssetSerializer;
using FilesFontAssetBinarySerializer = helengine.files.FontAssetBinarySerializer;

namespace helengine.wii.builder;

/// <summary>
/// Owns the Wii builder workspace operations that prepare generated-core runtime metadata.
/// </summary>
public static class WiiBuildWorkspace {
    /// <summary>
    /// Serializer options used when writing the Wii build manifest.
    /// </summary>
    static readonly JsonSerializerOptions JsonOptions = new() {
        WriteIndented = true
    };

    /// <summary>
    /// Executes one packaged Wii build request by staging cooked artifacts, emitting the packaged runtime manifest, invoking the native packaged build, writing the extracted disc layout, and invoking image packaging.
    /// </summary>
    /// <param name="request">Resolved packaged build request to process.</param>
    /// <param name="progressReporter">Progress reporter that receives streaming updates.</param>
    /// <param name="diagnosticReporter">Diagnostic reporter that receives streaming diagnostics.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the build cooperatively.</param>
    /// <param name="nativeBuildExecutor">Native packaged-build executor used to produce the DOL.</param>
    /// <param name="imagePackager">Optional image packager override used to turn the extracted disc layout into a final image artifact.</param>
    /// <returns>The final packaged-build report.</returns>
    public static Task<PlatformBuildReport> BuildPackagedAsync(
        PlatformBuildRequest request,
        IPlatformBuildProgressReporter progressReporter,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken,
        IWiiNativeBuildExecutor nativeBuildExecutor,
        IWiiImagePackager imagePackager = null,
        WiiDiscSystemAreaOptions discSystemAreaOptions = null) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (progressReporter == null) {
            throw new ArgumentNullException(nameof(progressReporter));
        } else if (diagnosticReporter == null) {
            throw new ArgumentNullException(nameof(diagnosticReporter));
        } else if (nativeBuildExecutor == null) {
            throw new ArgumentNullException(nameof(nativeBuildExecutor));
        }

        WiiBuilderPaths paths = WiiBuilderPaths.Create(request);
        string phaseMarkerPath = Path.Combine(request.OutputRoot, "wii-build-phase.txt");
        Directory.CreateDirectory(request.OutputRoot);
        Directory.CreateDirectory(request.WorkingRoot);
        Directory.CreateDirectory(paths.GeneratedCoreRootPath);

        List<PlatformBuildDiagnostic> diagnostics = [];
        List<PlatformBuildItemOutcome> sceneOutcomes = BuildSuccessfulSceneOutcomes(request.Manifest.Scenes);
        List<PlatformBuildItemOutcome> looseAssetOutcomes = BuildSuccessfulLooseAssetOutcomes(request.Manifest.LooseAssets);

        ResetDirectory(paths.StagingRootPath);
        Directory.CreateDirectory(paths.StagingRootPath);
        WritePhaseMarker(phaseMarkerPath, "platform cook work items begin");
        ExecutePlatformCookWorkItems(request.Manifest.PlatformCookWorkItems ?? [], paths.StagingRootPath);
        WritePhaseMarker(phaseMarkerPath, "platform cook work items completed");
        WritePhaseMarker(phaseMarkerPath, "stage cooked artifacts begin");
        StageCookedArtifacts(request, paths.StagingRootPath, progressReporter, diagnosticReporter, diagnostics, cancellationToken);
        WritePhaseMarker(phaseMarkerPath, "stage cooked artifacts completed");
        WritePhaseMarker(phaseMarkerPath, "normalize embedded font atlases begin");
        NormalizeEmbeddedFontAtlases(paths.StagingRootPath);
        WritePhaseMarker(phaseMarkerPath, "normalize embedded font atlases completed");

        if (diagnostics.Count > 0) {
            WritePhaseMarker(phaseMarkerPath, "diagnostics reported before packaged native build");
            return Task.FromResult(new PlatformBuildReport(false, [.. diagnostics], [.. sceneOutcomes], [.. looseAssetOutcomes]));
        }

        WriteRuntimeSceneManifest(paths, request.Manifest);
        WritePhaseMarker(phaseMarkerPath, "runtime scene manifest written");
        progressReporter.Report(new PlatformBuildProgressUpdate("Generate Runtime Manifest", "runtime-scene-manifest", 1, 4, "Generated Wii packaged runtime scene manifest."));

        WritePhaseMarker(phaseMarkerPath, "native build begin");
        nativeBuildExecutor.Build(paths, cancellationToken);
        WritePhaseMarker(phaseMarkerPath, "native build completed");
        progressReporter.Report(new PlatformBuildProgressUpdate("Build Native Executable", "helengine_wii.dol", 2, 4, "Built packaged-mode Wii native executable."));

        WiiDiscSystemAreaOptions effectiveDiscSystemAreaOptions = discSystemAreaOptions ?? CreateConfiguredDiscSystemAreaOptions(request.Manifest);
        WiiDiscLayoutResult discLayout = new WiiDiscLayoutWriter().Write(paths.StagingRootPath, paths.NativeExecutablePath, paths.DiscRootPath, effectiveDiscSystemAreaOptions);
        WritePhaseMarker(phaseMarkerPath, "disc layout written");
        progressReporter.Report(new PlatformBuildProgressUpdate("Write Disc Layout", "disc-root", 3, 4, "Wrote extracted Wii disc layout."));

        IWiiImagePackager effectiveImagePackager = imagePackager ?? CreateConfiguredImagePackager();
        effectiveImagePackager.Package(discLayout, paths.DiscImagePath, cancellationToken);
        WritePhaseMarker(phaseMarkerPath, "disc image packaged");
        VerifyPackagedOutputs(paths);
        WritePhaseMarker(phaseMarkerPath, "packaged outputs verified");
        progressReporter.Report(new PlatformBuildProgressUpdate("Package Disc Image", "game.iso", 4, 4, "Packaged Wii disc image artifact."));

        return Task.FromResult(new PlatformBuildReport(true, [.. diagnostics], [.. sceneOutcomes], [.. looseAssetOutcomes]));
    }

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

    /// <summary>
    /// Stages all cooked artifacts referenced by the packaged build manifest into the supplied staging root.
    /// </summary>
    /// <param name="request">Resolved packaged build request.</param>
    /// <param name="stagingRootPath">Working staging root that receives cooked artifacts.</param>
    /// <param name="progressReporter">Progress reporter that receives staging updates.</param>
    /// <param name="diagnosticReporter">Diagnostic reporter that receives staging failures.</param>
    /// <param name="diagnostics">Diagnostic list collecting staging failures.</param>
    /// <param name="cancellationToken">Cancellation token used to stop cooperative work.</param>
    static void StageCookedArtifacts(
        PlatformBuildRequest request,
        string stagingRootPath,
        IPlatformBuildProgressReporter progressReporter,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        List<PlatformBuildDiagnostic> diagnostics,
        CancellationToken cancellationToken) {
        PlatformBuildArtifact[] cookedArtifacts = request.Manifest.CookedArtifacts ?? [];
        for (int artifactIndex = 0; artifactIndex < cookedArtifacts.Length; artifactIndex++) {
            cancellationToken.ThrowIfCancellationRequested();

            PlatformBuildArtifact artifact = cookedArtifacts[artifactIndex];
            string sourcePath = ResolveSourcePath(artifact.RelativePath);
            if (!File.Exists(sourcePath)) {
                AddDiagnostic(
                    diagnostics,
                    diagnosticReporter,
                    PlatformBuildDiagnosticSeverity.Error,
                    "WIIPACK001",
                    $"Cooked artifact '{artifact.RelativePath}' was not found in the staged package root.",
                    string.Empty,
                    artifact.LogicalArtifactId,
                    artifact.RelativePath);
                continue;
            }

            string destinationPath = Path.Combine(stagingRootPath, NormalizeRelativePath(artifact.RelativePath));
            string destinationDirectoryPath = Path.GetDirectoryName(destinationPath) ?? throw new InvalidOperationException($"Destination directory could not be resolved for '{destinationPath}'.");
            Directory.CreateDirectory(destinationDirectoryPath);
            File.Copy(sourcePath, destinationPath, true);
            progressReporter.Report(new PlatformBuildProgressUpdate(
                "Stage Cooked Artifacts",
                artifact.LogicalArtifactId,
                artifactIndex + 1,
                cookedArtifacts.Length,
                $"Staged cooked artifact '{artifact.RelativePath}'."));
        }
    }

    /// <summary>
    /// Executes any builder-owned platform cook work items emitted by the shared editor build graph.
    /// </summary>
    /// <param name="platformCookWorkItems">Platform cook work items emitted by the editor build graph.</param>
    /// <param name="stagingRootPath">Staging root that receives the cooked outputs.</param>
    static void ExecutePlatformCookWorkItems(PlatformCookWorkItem[] platformCookWorkItems, string stagingRootPath) {
        if (platformCookWorkItems == null) {
            throw new ArgumentNullException(nameof(platformCookWorkItems));
        } else if (string.IsNullOrWhiteSpace(stagingRootPath)) {
            throw new ArgumentException("Staging root path must be provided.", nameof(stagingRootPath));
        }

        if (platformCookWorkItems.Length == 0) {
            return;
        }

        new WiiPlatformCookWorkItemExecutor().Execute(platformCookWorkItems, Directory.GetCurrentDirectory(), stagingRootPath);
    }

    /// <summary>
    /// Rewrites staged packaged fonts that still embed raw atlas bytes so Wii runtime can load one cooked external atlas texture.
    /// </summary>
    /// <param name="stagingRootPath">Staging root that already contains packaged cooked artifacts.</param>
    static void NormalizeEmbeddedFontAtlases(string stagingRootPath) {
        if (string.IsNullOrWhiteSpace(stagingRootPath)) {
            throw new ArgumentException("Staging root path must be provided.", nameof(stagingRootPath));
        }

        string cookedFontsRootPath = Path.Combine(stagingRootPath, "cooked", "fonts");
        if (!Directory.Exists(cookedFontsRootPath)) {
            return;
        }

        WiiTextureCooker textureCooker = new WiiTextureCooker();
        WiiTextureCookSettings cookSettings = new WiiTextureCookSettings(0, TextureAssetColorFormat.GxRgb5A3.ToString(), TextureAssetAlphaPrecision.A8);
        string[] fontPaths = Directory.GetFiles(cookedFontsRootPath, "*.hefont", SearchOption.AllDirectories);
        for (int fontIndex = 0; fontIndex < fontPaths.Length; fontIndex++) {
            string fontPath = fontPaths[fontIndex];
            FontAsset fontAsset = ReadPackagedFontAsset(fontPath);
            if (!HasEmbeddedFontAtlas(fontAsset)) {
                continue;
            }

            string cookedAtlasTextureRelativePath = ResolveCookedAtlasTextureRelativePath(stagingRootPath, fontPath, fontAsset);
            TextureAsset cookedAtlasTexture = textureCooker.CookTexture(fontAsset.SourceTextureAsset, cookSettings);
            WriteTextureAsset(Path.Combine(stagingRootPath, NormalizeRelativePath(cookedAtlasTextureRelativePath)), cookedAtlasTexture);
            fontAsset.CookedAtlasTextureRelativePath = cookedAtlasTextureRelativePath.Replace('\\', '/');
            fontAsset.SourceTextureAsset = null;
            WriteFontAsset(fontPath, fontAsset);
        }
    }

    /// <summary>
    /// Returns whether one packaged font still owns embedded source-texture bytes that must be externalized for Wii.
    /// </summary>
    /// <param name="fontAsset">Packaged font asset to inspect.</param>
    /// <returns>True when the font contains one raw source atlas payload; otherwise false.</returns>
    static bool HasEmbeddedFontAtlas(FontAsset fontAsset) {
        if (fontAsset == null) {
            throw new ArgumentNullException(nameof(fontAsset));
        }

        return fontAsset.SourceTextureAsset != null
            && fontAsset.SourceTextureAsset.Colors != null
            && fontAsset.SourceTextureAsset.Colors.Length > 0;
    }

    /// <summary>
    /// Resolves the cooked atlas texture path to assign to one staged packaged font during Wii normalization.
    /// </summary>
    /// <param name="stagingRootPath">Absolute staging root that owns the font artifact.</param>
    /// <param name="fontPath">Absolute staged packaged font path.</param>
    /// <param name="fontAsset">Packaged font asset being normalized.</param>
    /// <returns>Runtime-relative cooked atlas texture path.</returns>
    static string ResolveCookedAtlasTextureRelativePath(string stagingRootPath, string fontPath, FontAsset fontAsset) {
        if (string.IsNullOrWhiteSpace(stagingRootPath)) {
            throw new ArgumentException("Staging root path must be provided.", nameof(stagingRootPath));
        } else if (string.IsNullOrWhiteSpace(fontPath)) {
            throw new ArgumentException("Font path must be provided.", nameof(fontPath));
        } else if (fontAsset == null) {
            throw new ArgumentNullException(nameof(fontAsset));
        }

        if (!string.IsNullOrWhiteSpace(fontAsset.CookedAtlasTextureRelativePath)) {
            return NormalizeRelativePath(fontAsset.CookedAtlasTextureRelativePath);
        }

        string stagedFontRelativePath = NormalizeRelativePath(Path.GetRelativePath(stagingRootPath, fontPath));
        string cookedAtlasTextureRelativePath = Path.ChangeExtension(stagedFontRelativePath, ".ps2tex");
        if (string.IsNullOrWhiteSpace(cookedAtlasTextureRelativePath)) {
            throw new InvalidOperationException($"Cooked atlas texture path could not be resolved for '{fontPath}'.");
        }

        return NormalizeRelativePath(cookedAtlasTextureRelativePath);
    }

    /// <summary>
    /// Creates one cooked Wii atlas texture from the embedded source atlas bytes.
    /// </summary>
    /// <param name="sourceTextureAsset">Embedded source atlas texture.</param>
    /// <returns>Cooked atlas texture asset written beside the staged font.</returns>
    static TextureAsset CreateCookedAtlasTexture(TextureAsset sourceTextureAsset) {
        if (sourceTextureAsset == null) {
            throw new ArgumentNullException(nameof(sourceTextureAsset));
        }

        return new WiiTextureCooker().CookTexture(
            sourceTextureAsset,
            new WiiTextureCookSettings(0, TextureAssetColorFormat.GxRgb5A3.ToString(), TextureAssetAlphaPrecision.A8));
    }

    /// <summary>
    /// Reads one packaged font asset from disk using the shared files serializer.
    /// </summary>
    /// <param name="fontPath">Absolute packaged font path to load.</param>
    /// <returns>Deserialized packaged font asset.</returns>
    static FontAsset ReadPackagedFontAsset(string fontPath) {
        if (string.IsNullOrWhiteSpace(fontPath)) {
            throw new ArgumentException("Font path must be provided.", nameof(fontPath));
        }

        using FileStream stream = new FileStream(fontPath, FileMode.Open, FileAccess.Read, FileShare.Read);
        return FilesFontAssetBinarySerializer.Deserialize(stream);
    }

    /// <summary>
    /// Writes one normalized packaged font asset back to disk.
    /// </summary>
    /// <param name="fontPath">Absolute packaged font destination path.</param>
    /// <param name="fontAsset">Normalized packaged font asset to serialize.</param>
    static void WriteFontAsset(string fontPath, FontAsset fontAsset) {
        if (string.IsNullOrWhiteSpace(fontPath)) {
            throw new ArgumentException("Font path must be provided.", nameof(fontPath));
        } else if (fontAsset == null) {
            throw new ArgumentNullException(nameof(fontAsset));
        }

        using FileStream stream = new FileStream(fontPath, FileMode.Create, FileAccess.Write, FileShare.None);
        FilesFontAssetBinarySerializer.Serialize(stream, fontAsset);
    }

    /// <summary>
    /// Writes one cooked texture asset into the staged disc tree.
    /// </summary>
    /// <param name="texturePath">Absolute texture destination path.</param>
    /// <param name="textureAsset">Cooked texture asset to serialize.</param>
    static void WriteTextureAsset(string texturePath, TextureAsset textureAsset) {
        if (string.IsNullOrWhiteSpace(texturePath)) {
            throw new ArgumentException("Texture path must be provided.", nameof(texturePath));
        } else if (textureAsset == null) {
            throw new ArgumentNullException(nameof(textureAsset));
        }

        string directoryPath = Path.GetDirectoryName(texturePath) ?? throw new InvalidOperationException("Texture directory path could not be resolved.");
        Directory.CreateDirectory(directoryPath);
        using FileStream stream = new FileStream(texturePath, FileMode.Create, FileAccess.Write, FileShare.None);
        FilesAssetSerializer.Serialize(stream, textureAsset);
    }

    /// <summary>
    /// Builds successful scene outcomes for the packaged-build report.
    /// </summary>
    /// <param name="scenes">Scenes included in the packaged build request.</param>
    /// <returns>Successful scene outcomes for the packaged build request.</returns>
    static List<PlatformBuildItemOutcome> BuildSuccessfulSceneOutcomes(PlatformBuildScene[] scenes) {
        List<PlatformBuildItemOutcome> outcomes = [];
        if (scenes == null) {
            return outcomes;
        }

        for (int index = 0; index < scenes.Length; index++) {
            outcomes.Add(new PlatformBuildItemOutcome(scenes[index].SceneId, PlatformBuildItemOutcomeKind.Succeeded));
        }

        return outcomes;
    }

    /// <summary>
    /// Builds successful loose-asset outcomes for the packaged-build report.
    /// </summary>
    /// <param name="looseAssets">Loose assets included in the packaged build request.</param>
    /// <returns>Successful loose-asset outcomes for the packaged build request.</returns>
    static List<PlatformBuildItemOutcome> BuildSuccessfulLooseAssetOutcomes(PlatformBuildAsset[] looseAssets) {
        List<PlatformBuildItemOutcome> outcomes = [];
        if (looseAssets == null) {
            return outcomes;
        }

        for (int index = 0; index < looseAssets.Length; index++) {
            outcomes.Add(new PlatformBuildItemOutcome(looseAssets[index].AssetId, PlatformBuildItemOutcomeKind.Succeeded));
        }

        return outcomes;
    }

    /// <summary>
    /// Removes one directory tree when it already exists so the packaged staging root can be rebuilt deterministically.
    /// </summary>
    /// <param name="path">Directory path to remove before rebuilding it.</param>
    static void ResetDirectory(string path) {
        if (Directory.Exists(path)) {
            Directory.Delete(path, recursive: true);
        }
    }

    /// <summary>
    /// Appends one build-phase marker into the current Wii output root so editor-owned build failures can be recovered after the host process exits.
    /// </summary>
    /// <param name="phaseMarkerPath">Absolute phase-marker file path inside the active output root.</param>
    /// <param name="message">Human-readable phase message to append.</param>
    static void WritePhaseMarker(string phaseMarkerPath, string message) {
        if (string.IsNullOrWhiteSpace(phaseMarkerPath)) {
            throw new ArgumentException("Phase marker path must be provided.", nameof(phaseMarkerPath));
        } else if (string.IsNullOrWhiteSpace(message)) {
            throw new ArgumentException("Phase marker message must be provided.", nameof(message));
        }

        File.AppendAllText(phaseMarkerPath, message + Environment.NewLine);
        Console.WriteLine("[helengine-wii] " + message);
    }

    /// <summary>
    /// Verifies the packaged Wii outputs exist after the builder finishes writing the disc layout and disc image.
    /// </summary>
    /// <param name="paths">Resolved Wii builder paths that define the expected packaged outputs.</param>
    static void VerifyPackagedOutputs(WiiBuilderPaths paths) {
        if (paths == null) {
            throw new ArgumentNullException(nameof(paths));
        }

        string discMainDolPath = Path.Combine(paths.DiscRootPath, "sys", "main.dol");
        string discBootImagePath = Path.Combine(paths.DiscRootPath, "sys", "boot.bin");
        string discBi2Path = Path.Combine(paths.DiscRootPath, "sys", "bi2.bin");
        string discApploaderPath = Path.Combine(paths.DiscRootPath, "sys", "apploader.img");
        string discSetupPath = Path.Combine(paths.DiscRootPath, "setup.txt");
        string nativeApploaderTemplatePath = WiiNativeApploaderTemplatePathResolver.Resolve(paths.NativeExecutablePath);
        if (!File.Exists(paths.NativeExecutablePath)) {
            throw new FileNotFoundException("The packaged native Wii DOL was not staged into the build output.", paths.NativeExecutablePath);
        } else if (!File.Exists(nativeApploaderTemplatePath)) {
            throw new FileNotFoundException("The packaged native Wii apploader template was not staged into the build output.", nativeApploaderTemplatePath);
        } else if (!File.Exists(discMainDolPath)) {
            throw new FileNotFoundException("The extracted Wii disc main.dol was not produced.", discMainDolPath);
        } else if (!File.Exists(discBootImagePath)) {
            throw new FileNotFoundException("The extracted Wii disc boot.bin was not produced.", discBootImagePath);
        } else if (!File.Exists(discBi2Path)) {
            throw new FileNotFoundException("The extracted Wii disc bi2.bin was not produced.", discBi2Path);
        } else if (!File.Exists(discApploaderPath)) {
            throw new FileNotFoundException("The extracted Wii disc apploader.img was not produced.", discApploaderPath);
        } else if (!File.Exists(discSetupPath)) {
            throw new FileNotFoundException("The extracted Wii disc setup.txt was not produced.", discSetupPath);
        } else if (!File.Exists(paths.DiscImagePath)) {
            throw new FileNotFoundException("The packaged Wii disc image was not produced.", paths.DiscImagePath);
        }
    }

    /// <summary>
    /// Creates the configured Wii image packager for the staged builder-owned FST partition path.
    /// </summary>
    /// <returns>Configured Wii image packager.</returns>
    static IWiiImagePackager CreateConfiguredImagePackager() {
        string witExecutablePath = Environment.GetEnvironmentVariable("HELENGINE_WII_WIT_PATH");
        if (!string.IsNullOrWhiteSpace(witExecutablePath)) {
            return new WiiWiimmsIsoToolsImagePackager(
                new WiiWiimmsIsoToolsOptions(witExecutablePath),
                new WiiProcessRunner());
        }

        throw new InvalidOperationException("Wii packaged builds require HELENGINE_WII_WIT_PATH to point at the installed wit executable.");
    }

    /// <summary>
    /// Creates the configured extracted-disc system-area options from explicit environment configuration and manifest metadata.
    /// </summary>
    /// <param name="manifest">Manifest supplying project metadata for the disc header.</param>
    /// <returns>Configured extracted-disc system-area options.</returns>
    static WiiDiscSystemAreaOptions CreateConfiguredDiscSystemAreaOptions(PlatformBuildManifest manifest) {
        if (manifest == null) {
            throw new ArgumentNullException(nameof(manifest));
        }

        return new WiiDiscSystemAreaOptions(
            CreateDiscId(manifest.ProjectId),
            manifest.ProjectId);
    }

    /// <summary>
    /// Creates one stable Wii-shaped ID6 value from the authored project id.
    /// </summary>
    /// <param name="projectId">Authored project identifier.</param>
    /// <returns>Stable Wii-shaped ID6 value.</returns>
    static string CreateDiscId(string projectId) {
        if (string.IsNullOrWhiteSpace(projectId)) {
            throw new ArgumentException("Project id is required.", nameof(projectId));
        }

        List<char> gameCodeCharacters = [];
        for (int index = 0; index < projectId.Length && gameCodeCharacters.Count < 2; index++) {
            char character = char.ToUpperInvariant(projectId[index]);
            if (char.IsAsciiLetterOrDigit(character)) {
                gameCodeCharacters.Add(character);
            }
        }

        while (gameCodeCharacters.Count < 2) {
            gameCodeCharacters.Add('X');
        }

        return string.Create(
            6,
            gameCodeCharacters,
            static (destination, source) => {
                destination[0] = 'R';
                destination[1] = source[0];
                destination[2] = source[1];
                destination[3] = 'E';
                destination[4] = '0';
                destination[5] = '1';
            });
    }

    /// <summary>
    /// Adds one diagnostic to the shared list and mirrors it to the reporter.
    /// </summary>
    /// <param name="diagnostics">Collected diagnostics.</param>
    /// <param name="diagnosticReporter">Diagnostic reporter to mirror.</param>
    /// <param name="severity">Diagnostic severity.</param>
    /// <param name="code">Diagnostic code.</param>
    /// <param name="message">Diagnostic message.</param>
    /// <param name="sceneId">Scene identifier for the diagnostic.</param>
    /// <param name="assetId">Asset identifier for the diagnostic.</param>
    /// <param name="sourceIdentity">Source identity for the diagnostic.</param>
    static void AddDiagnostic(
        List<PlatformBuildDiagnostic> diagnostics,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        PlatformBuildDiagnosticSeverity severity,
        string code,
        string message,
        string sceneId,
        string assetId,
        string sourceIdentity) {
        PlatformBuildDiagnostic diagnostic = new(severity, code, message, sceneId, assetId, sourceIdentity);
        diagnostics.Add(diagnostic);
        diagnosticReporter.Report(diagnostic);
    }

    /// <summary>
    /// Resolves one relative source identity into a full path from the current working directory.
    /// </summary>
    /// <param name="sourceIdentity">The source identity recorded in the request.</param>
    /// <returns>The full source path.</returns>
    static string ResolveSourcePath(string sourceIdentity) {
        string normalizedSourceIdentity = NormalizeRelativePath(sourceIdentity);
        if (Path.IsPathRooted(normalizedSourceIdentity)) {
            return Path.GetFullPath(normalizedSourceIdentity);
        }

        return Path.GetFullPath(Path.Combine(Directory.GetCurrentDirectory(), normalizedSourceIdentity));
    }

    /// <summary>
    /// Resolves one output path under the supplied output root.
    /// </summary>
    /// <param name="outputRoot">The final output root.</param>
    /// <param name="sourceIdentity">The request source identity.</param>
    /// <returns>The full output path.</returns>
    static string ResolveOutputPath(string outputRoot, string sourceIdentity) {
        string normalizedSourceIdentity = NormalizeRelativePath(sourceIdentity);
        if (Path.IsPathRooted(normalizedSourceIdentity)) {
            normalizedSourceIdentity = Path.GetFileName(normalizedSourceIdentity);
        }

        return Path.GetFullPath(Path.Combine(outputRoot, normalizedSourceIdentity));
    }

    /// <summary>
    /// Writes the Wii build manifest to the working root for traceability.
    /// </summary>
    /// <param name="request">Resolved build request.</param>
    static void WriteBuildManifest(PlatformBuildRequest request) {
        string manifestPath = Path.Combine(request.WorkingRoot, "wii-build-manifest.json");
        object manifest = new {
            request.Manifest.ProjectId,
            request.Manifest.ProjectVersion,
            request.Manifest.RequiredEngineVersion,
            request.OutputRoot
        };

        File.WriteAllText(manifestPath, JsonSerializer.Serialize(manifest, JsonOptions));
    }

    /// <summary>
    /// Normalizes one path for the current host operating system.
    /// </summary>
    /// <param name="path">The path to normalize.</param>
    /// <returns>The normalized path.</returns>
    static string NormalizeRelativePath(string path) {
        return path.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);
    }
}
