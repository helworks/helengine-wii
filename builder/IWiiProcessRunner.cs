using System.Diagnostics;

namespace helengine.wii.builder;

/// <summary>
/// Runs external processes used by the Wii builder and captures their outputs.
/// </summary>
public interface IWiiProcessRunner {
    /// <summary>
    /// Runs one configured process and returns its captured output.
    /// </summary>
    /// <param name="startInfo">Prepared process start info.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the process cooperatively.</param>
    /// <returns>Captured process result.</returns>
    WiiProcessRunResult Run(ProcessStartInfo startInfo, CancellationToken cancellationToken);
}
