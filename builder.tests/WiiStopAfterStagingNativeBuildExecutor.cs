namespace helengine.wii.builder.tests;

/// <summary>Stops the real managed build at the native boundary so tests inspect real staged assets without producing fake native artifacts.</summary>
public sealed class WiiStopAfterStagingNativeBuildExecutor : IWiiNativeBuildExecutor {
    /// <summary>Identifies the intentional stop after all real texture and font staging completed.</summary>
    public const string StopMessage = "Stop after real Wii texture staging";
    /// <summary>Gets whether managed preparation reached the native build boundary.</summary>
    public bool WasInvoked { get; private set; }
    /// <summary>Records the native boundary and exits before Docker, SDK compilation or output fabrication.</summary>
    public void Build(WiiBuilderPaths paths, CancellationToken cancellationToken) {
        cancellationToken.ThrowIfCancellationRequested();
        WasInvoked = true;
        throw new InvalidOperationException(StopMessage);
    }
}
