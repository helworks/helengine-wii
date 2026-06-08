namespace helengine.wii.builder;

/// <summary>
/// Builds the packaged-mode Wii native executable for one builder workspace.
/// </summary>
public interface IWiiNativeBuildExecutor {
    /// <summary>
    /// Builds the packaged-mode native executable into the supplied workspace paths.
    /// </summary>
    /// <param name="paths">Workspace paths that define the packaged build inputs and outputs.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the native build cooperatively.</param>
    void Build(WiiBuilderPaths paths, CancellationToken cancellationToken);
}
