using helengine.baseplatform.Builders;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Descriptors;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.wii.builder;

/// <summary>
/// Implements the Wii platform asset builder contract consumed by the shared editor build graph.
/// </summary>
public sealed class WiiPlatformAssetBuilder : IPlatformAssetBuilder {
    /// <summary>
    /// Runtime specialization id that selects the packaged-disc Wii build flow.
    /// </summary>
    const string PackagedDiscRuntimeSpecializationId = "wii-disc-layout";

    /// <summary>
    /// Native build executor used when the request selects the packaged-disc flow.
    /// </summary>
    readonly IWiiNativeBuildExecutor NativeBuildExecutor;

    /// <summary>
    /// Material cooker that translates authored Wii material schemas into platform-owned cooked payloads.
    /// </summary>
    readonly WiiMaterialCooker MaterialCooker;

    /// <summary>
    /// Optional image packager override used by packaged-disc tests and custom tooling flows.
    /// </summary>
    readonly IWiiImagePackager ImagePackager;

    /// <summary>
    /// Optional disc system-area override used by packaged-disc tests and custom tooling flows.
    /// </summary>
    readonly WiiDiscSystemAreaOptions DiscSystemAreaOptions;

    /// <summary>
    /// Initializes one Wii builder instance with the current platform metadata.
    /// </summary>
    public WiiPlatformAssetBuilder()
        : this(new WiiDockerNativeBuildExecutor(), null, null) {
    }

    /// <summary>
    /// Initializes one Wii builder instance with explicit packaged-build collaborators.
    /// </summary>
    /// <param name="nativeBuildExecutor">Native build executor used when the packaged-disc flow is selected.</param>
    /// <param name="imagePackager">Optional image packager override used when the packaged-disc flow is selected.</param>
    /// <param name="discSystemAreaOptions">Optional system-area override used when the packaged-disc flow is selected.</param>
    public WiiPlatformAssetBuilder(
        IWiiNativeBuildExecutor nativeBuildExecutor,
        IWiiImagePackager imagePackager,
        WiiDiscSystemAreaOptions discSystemAreaOptions) {
        NativeBuildExecutor = nativeBuildExecutor ?? throw new ArgumentNullException(nameof(nativeBuildExecutor));
        MaterialCooker = new WiiMaterialCooker();
        ImagePackager = imagePackager;
        DiscSystemAreaOptions = discSystemAreaOptions;
        Descriptor = new PlatformBuilderDescriptor(
            "helengine.wii.builder",
            "1.0.0",
            "wii",
            new EngineCompatibilityRange("1.0.0", "999.0.0"),
            new ManifestCompatibilityRange(1, 2),
            ["wii"],
            ["wii-default"]);
        Definition = WiiPlatformDefinitionFactory.Create();
    }

    /// <summary>
    /// Gets the explicit builder descriptor for the Wii builder assembly.
    /// </summary>
    public PlatformBuilderDescriptor Descriptor { get; }

    /// <summary>
    /// Gets the typed Wii platform definition exposed to the editor.
    /// </summary>
    public PlatformDefinition Definition { get; }

    /// <summary>
    /// Translates one Wii material schema request into the current cooked payload contract.
    /// </summary>
    /// <param name="request">Material translation request for the Wii builder.</param>
    /// <returns>Minimal cooked material payload plus referenced shader dependencies.</returns>
    public PlatformMaterialCookResult CookMaterial(PlatformMaterialCookRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        }

        return MaterialCooker.Cook(request);
    }

    /// <summary>
    /// Executes one Wii build request through the packaged-disc workspace flow.
    /// </summary>
    /// <param name="request">The resolved build request.</param>
    /// <param name="progressReporter">The progress reporter.</param>
    /// <param name="diagnosticReporter">The diagnostic reporter.</param>
    /// <param name="cancellationToken">The cancellation token.</param>
    /// <returns>The final build report.</returns>
    public Task<PlatformBuildReport> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildProgressReporter progressReporter,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (progressReporter == null) {
            throw new ArgumentNullException(nameof(progressReporter));
        } else if (diagnosticReporter == null) {
            throw new ArgumentNullException(nameof(diagnosticReporter));
        }

        if (!string.Equals(
            request.Manifest.ContainerWritePlan.RuntimeSpecializationId,
            PackagedDiscRuntimeSpecializationId,
            StringComparison.Ordinal)) {
            throw new InvalidOperationException(
                $"Wii builder only supports runtime specialization '{PackagedDiscRuntimeSpecializationId}', but received '{request.Manifest.ContainerWritePlan.RuntimeSpecializationId}'.");
        }

        return WiiBuildWorkspace.BuildPackagedAsync(
            request,
            progressReporter,
            diagnosticReporter,
            cancellationToken,
            NativeBuildExecutor,
            ImagePackager,
            DiscSystemAreaOptions);
    }
}
