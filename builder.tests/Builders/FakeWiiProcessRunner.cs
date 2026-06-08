using System.Diagnostics;

namespace helengine.wii.builder.tests.Builders;

/// <summary>
/// Returns one preconfigured process result so Wii Wiimms ISO Tools tests can avoid starting real processes.
/// </summary>
public sealed class FakeWiiProcessRunner : IWiiProcessRunner {
    readonly WiiProcessRunResult Result;

    /// <summary>
    /// Initializes the fake runner with one preconfigured process result.
    /// </summary>
    /// <param name="result">Preconfigured process result to return.</param>
    public FakeWiiProcessRunner(WiiProcessRunResult result) {
        Result = result ?? throw new ArgumentNullException(nameof(result));
    }

    /// <summary>
    /// Gets the last prepared process start info received by the fake runner.
    /// </summary>
    public ProcessStartInfo LastStartInfo { get; private set; }

    /// <summary>
    /// Returns the preconfigured result without starting a real process.
    /// </summary>
    /// <param name="startInfo">Prepared process start info that would have been used.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the fake process cooperatively.</param>
    /// <returns>The preconfigured process result.</returns>
    public WiiProcessRunResult Run(ProcessStartInfo startInfo, CancellationToken cancellationToken) {
        cancellationToken.ThrowIfCancellationRequested();
        LastStartInfo = startInfo ?? throw new ArgumentNullException(nameof(startInfo));
        return Result;
    }
}
