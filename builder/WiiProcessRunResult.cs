namespace helengine.wii.builder;

/// <summary>
/// Carries the result of one external process invocation used by the Wii builder.
/// </summary>
public sealed class WiiProcessRunResult {
    /// <summary>
    /// Initializes one captured process result.
    /// </summary>
    /// <param name="exitCode">Process exit code.</param>
    /// <param name="standardOutput">Captured standard output.</param>
    /// <param name="standardError">Captured standard error.</param>
    public WiiProcessRunResult(int exitCode, string standardOutput, string standardError) {
        ExitCode = exitCode;
        StandardOutput = standardOutput ?? string.Empty;
        StandardError = standardError ?? string.Empty;
    }

    /// <summary>
    /// Gets the process exit code.
    /// </summary>
    public int ExitCode { get; }

    /// <summary>
    /// Gets the captured standard output.
    /// </summary>
    public string StandardOutput { get; }

    /// <summary>
    /// Gets the captured standard error.
    /// </summary>
    public string StandardError { get; }
}
