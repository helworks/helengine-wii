namespace helengine.wii.builder;

/// <summary>
/// Carries the explicit Wiimms ISO Tools executable path used by packaged Wii image builds.
/// </summary>
public sealed class WiiWiimmsIsoToolsOptions {
    /// <summary>
    /// Initializes one explicit Wiimms ISO Tools option set.
    /// </summary>
    /// <param name="executablePath">Configured path to the <c>wit</c> executable.</param>
    public WiiWiimmsIsoToolsOptions(string executablePath) {
        ExecutablePath = executablePath ?? string.Empty;
    }

    /// <summary>
    /// Gets the configured path to the <c>wit</c> executable.
    /// </summary>
    public string ExecutablePath { get; }
}
