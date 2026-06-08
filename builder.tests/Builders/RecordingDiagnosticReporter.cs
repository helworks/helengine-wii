using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.wii.builder.tests.Builders;

/// <summary>
/// Captures streamed build diagnostics for assertions in Wii builder tests.
/// </summary>
public sealed class RecordingDiagnosticReporter : IPlatformBuildDiagnosticReporter {
    /// <summary>
    /// Initializes one empty diagnostic recorder.
    /// </summary>
    public RecordingDiagnosticReporter() {
        Diagnostics = [];
    }

    /// <summary>
    /// Gets the diagnostics reported by the builder under test.
    /// </summary>
    public List<PlatformBuildDiagnostic> Diagnostics { get; }

    /// <summary>
    /// Stores one diagnostic emitted by the builder.
    /// </summary>
    /// <param name="diagnostic">Diagnostic emitted by the builder.</param>
    public void Report(PlatformBuildDiagnostic diagnostic) {
        if (diagnostic == null) {
            throw new ArgumentNullException(nameof(diagnostic));
        }

        Diagnostics.Add(diagnostic);
    }
}
