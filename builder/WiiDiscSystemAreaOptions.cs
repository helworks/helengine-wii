namespace helengine.wii.builder;

/// <summary>
/// Carries the explicit system-area inputs required to stage a valid extracted Wii FST partition root for image packaging.
/// </summary>
public sealed class WiiDiscSystemAreaOptions {
    /// <summary>
    /// Initializes one Wii disc system-area option set.
    /// </summary>
    /// <param name="discId">Six-character Wii disc identifier written into <c>sys/boot.bin</c> and <c>setup.txt</c>.</param>
    /// <param name="discTitle">Disc title written into <c>sys/boot.bin</c> and <c>setup.txt</c>.</param>
    public WiiDiscSystemAreaOptions(
        string discId,
        string discTitle) {
        if (string.IsNullOrWhiteSpace(discId)) {
            throw new ArgumentException("Disc id is required.", nameof(discId));
        } else if (string.IsNullOrWhiteSpace(discTitle)) {
            throw new ArgumentException("Disc title is required.", nameof(discTitle));
        }

        DiscId = discId;
        DiscTitle = discTitle;
    }

    /// <summary>
    /// Gets the six-character Wii disc identifier written into <c>sys/boot.bin</c> and <c>setup.txt</c>.
    /// </summary>
    public string DiscId { get; }

    /// <summary>
    /// Gets the disc title written into <c>sys/boot.bin</c> and <c>setup.txt</c>.
    /// </summary>
    public string DiscTitle { get; }
}
