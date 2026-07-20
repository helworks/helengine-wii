using System.Diagnostics;
using helengine.baseplatform.Builders;

namespace helengine.wii.builder;

/// <summary>
/// Runs real external processes for the Wii builder.
/// </summary>
public sealed class WiiProcessRunner : IWiiProcessRunner {
    /// <summary>
    /// Runs one configured process and captures exit code, stdout, and stderr.
    /// </summary>
    /// <param name="startInfo">Prepared process start info.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the process cooperatively.</param>
    /// <returns>Captured process result.</returns>
    public WiiProcessRunResult Run(ProcessStartInfo startInfo, CancellationToken cancellationToken) {
        if (startInfo == null) {
            throw new ArgumentNullException(nameof(startInfo));
        }

        NativeProcessRunResult result = new NativeProcessRunner().Run(startInfo, cancellationToken);
        return new WiiProcessRunResult(result.ExitCode, result.StandardOutput, result.StandardError);
    }
}
