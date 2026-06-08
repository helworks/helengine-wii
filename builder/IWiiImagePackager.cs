namespace helengine.wii.builder;

/// <summary>
/// Packages one extracted Wii disc layout into a final disc-image artifact.
/// </summary>
public interface IWiiImagePackager {
    /// <summary>
    /// Packages one extracted Wii disc layout into the final output image path.
    /// </summary>
    /// <param name="layout">Extracted disc layout that should be packaged.</param>
    /// <param name="outputImagePath">Final Wii image path to write.</param>
    /// <param name="cancellationToken">Cancellation token that can stop packaging cooperatively.</param>
    void Package(WiiDiscLayoutResult layout, string outputImagePath, CancellationToken cancellationToken);
}
