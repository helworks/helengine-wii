using helengine.baseplatform.Requests;

namespace helengine.wii.builder;

/// <summary>
/// Centralizes the filesystem paths used by one Wii builder invocation.
/// </summary>
public sealed class WiiBuilderPaths {
    /// <summary>
    /// Environment variable that overrides the Wii repository root when the builder is hosted inside the editor process.
    /// </summary>
    const string RepositoryRootEnvironmentVariableName = "HELENGINE_WII_REPOSITORY_ROOT";

    /// <summary>
    /// Creates one Wii build path set from a resolved build request.
    /// </summary>
    /// <param name="request">Resolved build request that owns the build paths.</param>
    /// <returns>Build path set for the supplied request.</returns>
    public static WiiBuilderPaths Create(PlatformBuildRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        }

        string repositoryRootPath = ResolveRepositoryRootPath();
        return new WiiBuilderPaths(
            repositoryRootPath,
            request.GeneratedCoreCppRootPath,
            Path.Combine(request.WorkingRoot, "staged-content"),
            Path.Combine(request.OutputRoot, "disc"),
            Path.Combine(request.OutputRoot, "game.iso"),
            Path.Combine(request.OutputRoot, "native", "helengine_wii.dol"));
    }

    /// <summary>
    /// Resolves the Wii repository root from the Wii builder assembly location instead of the hosting application directory.
    /// </summary>
    /// <returns>Absolute Wii repository root path.</returns>
    /// <exception cref="InvalidOperationException">Thrown when the Wii repository root cannot be resolved from the builder assembly location.</exception>
    static string ResolveRepositoryRootPath() {
        string configuredRepositoryRootPath = Environment.GetEnvironmentVariable(RepositoryRootEnvironmentVariableName) ?? string.Empty;
        if (IsRepositoryRootPath(configuredRepositoryRootPath)) {
            return Path.GetFullPath(configuredRepositoryRootPath);
        }

        string assemblyLocation = typeof(WiiBuilderPaths).Assembly.Location;
        if (string.IsNullOrWhiteSpace(assemblyLocation)) {
            throw new InvalidOperationException("The Wii builder assembly location could not be resolved.");
        }

        string currentPath = Path.GetDirectoryName(assemblyLocation) ?? string.Empty;
        while (!string.IsNullOrWhiteSpace(currentPath)) {
            if (IsRepositoryRootPath(currentPath)) {
                return currentPath;
            }

            DirectoryInfo parentDirectory = Directory.GetParent(currentPath);
            if (parentDirectory == null) {
                break;
            }

            currentPath = parentDirectory.FullName;
        }

        throw new InvalidOperationException("Could not resolve the helengine-wii repository root from the builder assembly location.");
    }

    /// <summary>
    /// Returns true when one path contains the Wii repository markers needed for generated-core manifest output.
    /// </summary>
    /// <param name="path">Candidate repository root path.</param>
    /// <returns>True when the candidate path is the Wii repository root.</returns>
    static bool IsRepositoryRootPath(string path) {
        if (string.IsNullOrWhiteSpace(path)) {
            return false;
        }

        string makefilePath = Path.Combine(path, "Makefile");
        string applicationPath = Path.Combine(path, "src", "platform", "wii", "WiiApplication.cpp");
        return File.Exists(makefilePath) && File.Exists(applicationPath);
    }

    /// <summary>
    /// Initializes one Wii build path set.
    /// </summary>
    /// <param name="repositoryRootPath">Repository root that contains the native Wii project files.</param>
    /// <param name="generatedCoreRootPath">Generated core root that receives runtime manifest files.</param>
    /// <param name="stagingRootPath">Working staging root that receives cooked artifacts before disc layout.</param>
    /// <param name="discRootPath">Extracted disc root written for inspection and image packaging.</param>
    /// <param name="discImagePath">Final Wii disc-image output path.</param>
    /// <param name="nativeExecutablePath">Packaged-mode native DOL output path staged by the builder.</param>
    public WiiBuilderPaths(
        string repositoryRootPath,
        string generatedCoreRootPath,
        string stagingRootPath,
        string discRootPath,
        string discImagePath,
        string nativeExecutablePath) {
        RepositoryRootPath = string.IsNullOrWhiteSpace(repositoryRootPath)
            ? throw new ArgumentException("Repository root path is required.", nameof(repositoryRootPath))
            : repositoryRootPath;
        GeneratedCoreRootPath = string.IsNullOrWhiteSpace(generatedCoreRootPath)
            ? throw new ArgumentException("Generated core root path is required.", nameof(generatedCoreRootPath))
            : generatedCoreRootPath;
        StagingRootPath = string.IsNullOrWhiteSpace(stagingRootPath)
            ? throw new ArgumentException("Staging root path is required.", nameof(stagingRootPath))
            : stagingRootPath;
        DiscRootPath = string.IsNullOrWhiteSpace(discRootPath)
            ? throw new ArgumentException("Disc root path is required.", nameof(discRootPath))
            : discRootPath;
        DiscImagePath = string.IsNullOrWhiteSpace(discImagePath)
            ? throw new ArgumentException("Disc image path is required.", nameof(discImagePath))
            : discImagePath;
        NativeExecutablePath = string.IsNullOrWhiteSpace(nativeExecutablePath)
            ? throw new ArgumentException("Native executable path is required.", nameof(nativeExecutablePath))
            : nativeExecutablePath;
    }

    /// <summary>
    /// Gets the repository root that contains the native Wii project files.
    /// </summary>
    public string RepositoryRootPath { get; }

    /// <summary>
    /// Gets the generated core root that receives runtime manifest files.
    /// </summary>
    public string GeneratedCoreRootPath { get; }

    /// <summary>
    /// Gets the working staging root that receives cooked artifacts before disc layout.
    /// </summary>
    public string StagingRootPath { get; }

    /// <summary>
    /// Gets the extracted disc root written for inspection and image packaging.
    /// </summary>
    public string DiscRootPath { get; }

    /// <summary>
    /// Gets the final Wii disc-image output path.
    /// </summary>
    public string DiscImagePath { get; }

    /// <summary>
    /// Gets the packaged-mode native DOL output path staged by the builder.
    /// </summary>
    public string NativeExecutablePath { get; }

    /// <summary>
    /// Gets the generated-core root relative to the repository root for future Docker path mapping.
    /// </summary>
    public string GeneratedCoreRelativePath => Path.GetRelativePath(RepositoryRootPath, GeneratedCoreRootPath).Replace('\\', '/');
}
