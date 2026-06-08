using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.wii.builder.tests.Builders;

/// <summary>
/// Captures streamed build progress updates for assertions in Wii builder tests.
/// </summary>
public sealed class RecordingProgressReporter : IPlatformBuildProgressReporter {
    /// <summary>
    /// Initializes one empty progress recorder.
    /// </summary>
    public RecordingProgressReporter() {
        Updates = [];
    }

    /// <summary>
    /// Gets the progress updates reported by the builder under test.
    /// </summary>
    public List<PlatformBuildProgressUpdate> Updates { get; }

    /// <summary>
    /// Stores one progress update emitted by the builder.
    /// </summary>
    /// <param name="update">Progress update emitted by the builder.</param>
    public void Report(PlatformBuildProgressUpdate update) {
        if (update == null) {
            throw new ArgumentNullException(nameof(update));
        }

        Updates.Add(update);
    }
}
