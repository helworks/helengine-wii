namespace helengine.wii.builder.tests.Builders;

/// <summary>
/// Writes a fake Wii image artifact so builder tests can verify orchestration without a real ISO writer.
/// </summary>
public sealed class FakeWiiImagePackager : IWiiImagePackager {
    /// <summary>
    /// Writes a fake Wii image artifact to the requested output path.
    /// </summary>
    /// <param name="layout">Extracted disc layout that would be packaged by a real implementation.</param>
    /// <param name="outputImagePath">Destination path for the fake image artifact.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the fake packaging cooperatively.</param>
    public void Package(WiiDiscLayoutResult layout, string outputImagePath, CancellationToken cancellationToken) {
        if (layout == null) {
            throw new ArgumentNullException(nameof(layout));
        } else if (string.IsNullOrWhiteSpace(outputImagePath)) {
            throw new ArgumentException("Output image path is required.", nameof(outputImagePath));
        }

        cancellationToken.ThrowIfCancellationRequested();
        string outputDirectoryPath = Path.GetDirectoryName(outputImagePath) ?? throw new InvalidOperationException("Output image directory path could not be resolved.");
        Directory.CreateDirectory(outputDirectoryPath);
        File.WriteAllText(outputImagePath, "iso");
    }
}
