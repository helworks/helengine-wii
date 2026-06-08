using System.Buffers.Binary;

namespace helengine.wii.builder.tests.Builders;

/// <summary>
/// Writes fake packaged-mode native outputs so builder tests can verify orchestration without Docker.
/// </summary>
public sealed class FakeWiiNativeBuildExecutor : IWiiNativeBuildExecutor {
    /// <summary>
    /// Byte offset of the patch marker inside the fake native apploader template.
    /// </summary>
    const int TemplateConfigOffset = 0x40;

    /// <summary>
    /// Writes a fake packaged-mode DOL plus one fake native apploader template into the requested workspace path.
    /// </summary>
    /// <param name="paths">Workspace paths that define the fake output destination.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the fake build cooperatively.</param>
    public void Build(WiiBuilderPaths paths, CancellationToken cancellationToken) {
        if (paths == null) {
            throw new ArgumentNullException(nameof(paths));
        }

        cancellationToken.ThrowIfCancellationRequested();
        string nativeExecutableDirectoryPath = Path.GetDirectoryName(paths.NativeExecutablePath) ?? throw new InvalidOperationException("Native executable directory path could not be resolved.");
        Directory.CreateDirectory(nativeExecutableDirectoryPath);
        File.WriteAllBytes(paths.NativeExecutablePath, CreateFakeDolBytes());
        File.WriteAllBytes(WiiNativeApploaderTemplatePathResolver.Resolve(paths.NativeExecutablePath), CreateFakeApploaderTemplateBytes());
    }

    /// <summary>
    /// Creates one minimal big-endian DOL payload with a single loadable text section.
    /// </summary>
    /// <returns>Minimal DOL payload used by builder tests.</returns>
    static byte[] CreateFakeDolBytes() {
        byte[] bytes = new byte[0x120];
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(0x00, sizeof(uint)), 0x00000100U);
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(0x48, sizeof(uint)), 0x80004000U);
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(0x90, sizeof(uint)), 0x00000020U);
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(0xD8, sizeof(uint)), 0x80005000U);
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(0xDC, sizeof(uint)), 0x00000040U);
        BinaryPrimitives.WriteUInt32BigEndian(bytes.AsSpan(0xE0, sizeof(uint)), 0x80004000U);
        return bytes;
    }

    /// <summary>
    /// Creates one fake native apploader template that exposes the same patch marker shape as the real template.
    /// </summary>
    /// <returns>Fake native apploader template bytes.</returns>
    static byte[] CreateFakeApploaderTemplateBytes() {
        byte[] bytes = new byte[0x200];
        bytes[TemplateConfigOffset + 0x00] = 0x48;
        bytes[TemplateConfigOffset + 0x01] = 0x45;
        bytes[TemplateConfigOffset + 0x02] = 0x4C;
        bytes[TemplateConfigOffset + 0x03] = 0x41;
        bytes[TemplateConfigOffset + 0x04] = 0x50;
        bytes[TemplateConfigOffset + 0x05] = 0x50;
        bytes[TemplateConfigOffset + 0x06] = 0x4C;
        bytes[TemplateConfigOffset + 0x07] = 0x44;
        bytes[TemplateConfigOffset + 0x08] = 0x30;
        bytes[TemplateConfigOffset + 0x09] = 0x30;
        bytes[TemplateConfigOffset + 0x0A] = 0x31;
        bytes[TemplateConfigOffset + 0x0B] = 0x31;
        bytes[TemplateConfigOffset + 0x0C] = 0xA5;
        bytes[TemplateConfigOffset + 0x0D] = 0x5A;
        bytes[TemplateConfigOffset + 0x0E] = 0xA5;
        bytes[TemplateConfigOffset + 0x0F] = 0x5A;
        return bytes;
    }
}
