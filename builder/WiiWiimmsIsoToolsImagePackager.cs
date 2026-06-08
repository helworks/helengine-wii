using System.Diagnostics;
using System.Runtime.InteropServices;

namespace helengine.wii.builder;

/// <summary>
/// Packages one extracted Wii disc layout into a final image artifact by invoking Wiimms ISO Tools.
/// </summary>
public sealed class WiiWiimmsIsoToolsImagePackager : IWiiImagePackager {
    readonly WiiWiimmsIsoToolsOptions Options;
    readonly IWiiProcessRunner ProcessRunner;
    readonly Action<WiiDiscLayoutResult, string> SuccessImageWriter;

    /// <summary>
    /// Initializes one Wiimms ISO Tools image packager.
    /// </summary>
    /// <param name="options">Explicit Wiimms ISO Tools configuration.</param>
    /// <param name="processRunner">Process runner used to invoke the external tool.</param>
    /// <param name="successImageWriter">Optional test hook that can materialize the expected image artifact after a successful process run.</param>
    public WiiWiimmsIsoToolsImagePackager(
        WiiWiimmsIsoToolsOptions options,
        IWiiProcessRunner processRunner,
        Action<WiiDiscLayoutResult, string> successImageWriter = null) {
        Options = options ?? throw new ArgumentNullException(nameof(options));
        ProcessRunner = processRunner ?? throw new ArgumentNullException(nameof(processRunner));
        SuccessImageWriter = successImageWriter;
    }

    /// <summary>
    /// Packages one extracted Wii disc layout into the final output image path.
    /// </summary>
    /// <param name="layout">Extracted disc layout that should be packaged.</param>
    /// <param name="outputImagePath">Final Wii image path to write.</param>
    /// <param name="cancellationToken">Cancellation token that can stop packaging cooperatively.</param>
    public void Package(WiiDiscLayoutResult layout, string outputImagePath, CancellationToken cancellationToken) {
        if (layout == null) {
            throw new ArgumentNullException(nameof(layout));
        } else if (string.IsNullOrWhiteSpace(outputImagePath)) {
            throw new ArgumentException("Output image path is required.", nameof(outputImagePath));
        } else if (string.IsNullOrWhiteSpace(Options.ExecutablePath)) {
            throw new InvalidOperationException("Wii image packager is not configured. Set HELENGINE_WII_WIT_PATH to the installed wit executable.");
        } else if (!File.Exists(Options.ExecutablePath)) {
            throw new FileNotFoundException("Configured Wii image packager executable was not found.", Options.ExecutablePath);
        }

        string fullDiscRootPath = Path.GetFullPath(layout.DiscRootPath);
        string fullOutputImagePath = Path.GetFullPath(outputImagePath);
        string outputDirectoryPath = Path.GetDirectoryName(fullOutputImagePath) ?? throw new InvalidOperationException("Output image directory path could not be resolved.");
        Directory.CreateDirectory(outputDirectoryPath);

        ProcessStartInfo startInfo = new() {
            WorkingDirectory = Path.GetDirectoryName(fullDiscRootPath) ?? fullDiscRootPath
        };
        ValidateDiscSource(layout);
        ConfigureProcessStartInfo(startInfo, fullDiscRootPath, fullOutputImagePath);

        WiiProcessRunResult result = ProcessRunner.Run(startInfo, cancellationToken);
        if (result.ExitCode != 0) {
            throw new InvalidOperationException(
                $"Wiimms ISO Tools failed with exit code {result.ExitCode}.{Environment.NewLine}{result.StandardOutput}{Environment.NewLine}{result.StandardError}");
        }

        if (SuccessImageWriter != null) {
            SuccessImageWriter(layout, fullOutputImagePath);
        }

        if (!File.Exists(fullOutputImagePath)) {
            throw new InvalidOperationException("Wiimms ISO Tools reported success but the Wii image artifact was not created.");
        }
    }

    /// <summary>
    /// Configures one process start info instance for direct or WSL-hosted Wiimms ISO Tools execution.
    /// </summary>
    /// <param name="startInfo">Start info instance to populate.</param>
    /// <param name="discRootPath">Extracted-disc source root.</param>
    /// <param name="outputImagePath">Final output image path.</param>
    void ConfigureProcessStartInfo(ProcessStartInfo startInfo, string discRootPath, string outputImagePath) {
        if (startInfo == null) {
            throw new ArgumentNullException(nameof(startInfo));
        }

        if (ShouldUseWindowsExecutableWrapper()) {
            string windowsExecutablePath = WiiWslWindowsPathTranslator.ConvertToWindowsPath(Options.ExecutablePath);
            string windowsDiscRootPath = WiiWslWindowsPathTranslator.ConvertToWindowsPath(discRootPath);
            string windowsOutputImagePath = WiiWslWindowsPathTranslator.ConvertToWindowsPath(outputImagePath);
            startInfo.FileName = "powershell.exe";
            startInfo.ArgumentList.Add("-NoProfile");
            startInfo.ArgumentList.Add("-Command");
            startInfo.ArgumentList.Add($"& '{windowsExecutablePath}' 'COPY' '{windowsDiscRootPath}' '{windowsOutputImagePath}' '--iso' '--trunc' '--overwrite' '--allow-fst=on'");
            return;
        }

        startInfo.FileName = Options.ExecutablePath;
        startInfo.ArgumentList.Add("COPY");
        startInfo.ArgumentList.Add(discRootPath);
        startInfo.ArgumentList.Add(outputImagePath);
        startInfo.ArgumentList.Add("--iso");
        startInfo.ArgumentList.Add("--trunc");
        startInfo.ArgumentList.Add("--overwrite");
        startInfo.ArgumentList.Add("--allow-fst=on");
    }

    /// <summary>
    /// Gets whether the configured Wiimms ISO Tools executable must be launched through the Windows host when running under WSL.
    /// </summary>
    /// <returns><see langword="true"/> when the configured executable is a Windows <c>.exe</c> path on Linux; otherwise <see langword="false"/>.</returns>
    bool ShouldUseWindowsExecutableWrapper() {
        return RuntimeInformation.IsOSPlatform(OSPlatform.Linux)
            && string.Equals(Path.GetExtension(Options.ExecutablePath), ".exe", StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Validates that the extracted-disc root contains the minimum files Wiimms ISO Tools expects for one Wii FST source.
    /// </summary>
    /// <param name="layout">Extracted-disc layout to validate.</param>
    static void ValidateDiscSource(WiiDiscLayoutResult layout) {
        string filesRootPath = Path.Combine(layout.DiscRootPath, "files");
        string bootBinPath = Path.Combine(layout.DiscRootPath, "sys", "boot.bin");
        string apploaderImagePath = Path.Combine(layout.DiscRootPath, "sys", "apploader.img");

        if (!File.Exists(bootBinPath)) {
            throw new InvalidOperationException("The extracted Wii disc root is missing sys/boot.bin.");
        } else if (!File.Exists(apploaderImagePath)) {
            throw new InvalidOperationException("The extracted Wii disc root is missing sys/apploader.img.");
        } else if (!File.Exists(layout.DiscExecutablePath)) {
            throw new InvalidOperationException("The extracted Wii disc root is missing sys/main.dol.");
        } else if (!Directory.Exists(filesRootPath)) {
            throw new InvalidOperationException("The extracted Wii disc root is missing the files/ payload directory.");
        }
    }
}
