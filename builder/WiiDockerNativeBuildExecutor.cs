using System.Diagnostics;

namespace helengine.wii.builder;

/// <summary>
/// Invokes the Dockerized Wii native build.
/// </summary>
public sealed class WiiDockerNativeBuildExecutor : IWiiNativeBuildExecutor {
    /// <summary>
    /// Builds the packaged-mode native DOL plus the generic apploader template and stages them into the builder workspace.
    /// </summary>
    /// <param name="paths">Workspace paths that define the packaged build inputs and outputs.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the native build cooperatively.</param>
    public void Build(WiiBuilderPaths paths, CancellationToken cancellationToken) {
        if (paths == null) {
            throw new ArgumentNullException(nameof(paths));
        }

        ProcessStartInfo startInfo = CreateStartInfo(paths);

        using Process process = Process.Start(startInfo) ?? throw new InvalidOperationException("Could not start the Wii Docker build process.");
        Task<string> standardOutputTask = process.StandardOutput.ReadToEndAsync();
        Task<string> standardErrorTask = process.StandardError.ReadToEndAsync();
        while (!process.HasExited) {
            cancellationToken.ThrowIfCancellationRequested();
            process.WaitForExit(100);
        }

        process.WaitForExit();
        Task.WaitAll(standardOutputTask, standardErrorTask);

        if (process.ExitCode != 0) {
            string standardOutput = standardOutputTask.Result;
            string standardError = standardErrorTask.Result;
            throw new InvalidOperationException(
                "Wii native packaged-disc build failed."
                + Environment.NewLine
                + standardOutput
                + Environment.NewLine
                + standardError);
        }

        string builtDolPath = Path.Combine(paths.RepositoryRootPath, "build", "helengine_wii.dol");
        string builtApploaderTemplatePath = Path.Combine(paths.RepositoryRootPath, "build", WiiNativeApploaderTemplatePathResolver.FileName);
        if (!File.Exists(builtDolPath)) {
            throw new FileNotFoundException("The packaged native Wii DOL was not produced by the Docker build.", builtDolPath);
        } else if (!File.Exists(builtApploaderTemplatePath)) {
            throw new FileNotFoundException("The packaged native Wii apploader template was not produced by the Docker build.", builtApploaderTemplatePath);
        }

        string nativeExecutableDirectoryPath = Path.GetDirectoryName(paths.NativeExecutablePath) ?? throw new InvalidOperationException("Native executable directory path could not be resolved.");
        Directory.CreateDirectory(nativeExecutableDirectoryPath);
        File.Copy(builtDolPath, paths.NativeExecutablePath, true);
        File.Copy(builtApploaderTemplatePath, WiiNativeApploaderTemplatePathResolver.Resolve(paths.NativeExecutablePath), true);
    }

    /// <summary>
    /// Creates the Docker process start info for one packaged native Wii build.
    /// </summary>
    /// <param name="paths">Workspace paths that define the packaged build inputs and outputs.</param>
    /// <returns>Configured Docker process start info.</returns>
    static ProcessStartInfo CreateStartInfo(WiiBuilderPaths paths) {
        if (paths == null) {
            throw new ArgumentNullException(nameof(paths));
        }

        ProcessStartInfo startInfo = new() {
            FileName = "docker",
            WorkingDirectory = paths.RepositoryRootPath,
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };

        string generatedCoreContainerPath = "/workspace/" + paths.GeneratedCoreRelativePath;
        startInfo.ArgumentList.Add("run");
        startInfo.ArgumentList.Add("--rm");
        startInfo.ArgumentList.Add("-v");
        startInfo.ArgumentList.Add(paths.RepositoryRootPath + ":/workspace");
        if (!IsPathUnderRoot(paths.GeneratedCoreRootPath, paths.RepositoryRootPath)) {
            generatedCoreContainerPath = "/helengine-generated-core";
            startInfo.ArgumentList.Add("-v");
            startInfo.ArgumentList.Add(paths.GeneratedCoreRootPath + ":" + generatedCoreContainerPath);
        }

        startInfo.ArgumentList.Add("-w");
        startInfo.ArgumentList.Add("/workspace");
        startInfo.ArgumentList.Add("-e");
        startInfo.ArgumentList.Add("HELENGINE_CORE_CPP_ROOT=" + generatedCoreContainerPath);
        startInfo.ArgumentList.Add("-e");
        startInfo.ArgumentList.Add("HELENGINE_WII_BOOT_MODE=packaged-disc");
        startInfo.ArgumentList.Add("helengine-wii");
        startInfo.ArgumentList.Add("sh");
        startInfo.ArgumentList.Add("-lc");
        startInfo.ArgumentList.Add("make clean; make");
        return startInfo;
    }

    /// <summary>
    /// Returns true when one filesystem path is inside the supplied root path.
    /// </summary>
    /// <param name="candidatePath">Candidate filesystem path.</param>
    /// <param name="rootPath">Containing filesystem root path.</param>
    /// <returns>True when the candidate path is inside the supplied root path.</returns>
    static bool IsPathUnderRoot(string candidatePath, string rootPath) {
        if (string.IsNullOrWhiteSpace(candidatePath)) {
            throw new ArgumentException("Candidate path is required.", nameof(candidatePath));
        }
        if (string.IsNullOrWhiteSpace(rootPath)) {
            throw new ArgumentException("Root path is required.", nameof(rootPath));
        }

        string fullCandidatePath = Path.GetFullPath(candidatePath);
        string fullRootPath = Path.GetFullPath(rootPath);
        string relativePath = Path.GetRelativePath(fullRootPath, fullCandidatePath);
        return !relativePath.StartsWith("..", StringComparison.Ordinal) && !Path.IsPathRooted(relativePath);
    }
}
