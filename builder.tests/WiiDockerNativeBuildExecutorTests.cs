using System.Diagnostics;
using System.Reflection;

namespace helengine.wii.builder.tests;

/// <summary>
/// Verifies Wii Docker native-build invocation maps generated-core roots correctly for editor-owned builds.
/// </summary>
public sealed class WiiDockerNativeBuildExecutorTests {
    /// <summary>
    /// Ensures packaged Wii native builds explicitly opt into packaged-disc boot instead of the direct-DOL default.
    /// </summary>
    [Fact]
    public void CreateStartInfo_PackagedBuild_ExportsPackagedDiscBootMode() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        WiiBuilderPaths paths = new(
            repositoryRootPath,
            Path.Combine(repositoryRootPath, "tmp", "generated-core"),
            Path.Combine(repositoryRootPath, "tmp", "staged-content"),
            Path.Combine(repositoryRootPath, "tmp", "disc"),
            Path.Combine(repositoryRootPath, "tmp", "game.iso"),
            Path.Combine(repositoryRootPath, "tmp", "native", "helengine_wii.dol"));
        MethodInfo createStartInfoMethod = typeof(WiiDockerNativeBuildExecutor).GetMethod(
            "CreateStartInfo",
            BindingFlags.Static | BindingFlags.NonPublic);

        Assert.NotNull(createStartInfoMethod);

        ProcessStartInfo startInfo = (ProcessStartInfo)createStartInfoMethod.Invoke(null, [paths]);

        Assert.Contains("HELENGINE_WII_BOOT_MODE=packaged-disc", startInfo.ArgumentList);
        Assert.Contains("make", startInfo.ArgumentList);
        Assert.Contains("clean", startInfo.ArgumentList);
        Assert.Contains("all", startInfo.ArgumentList);
    }

    /// <summary>
    /// Ensures external generated-core roots are mounted separately into Docker and exported through one absolute container path.
    /// </summary>
    [Fact]
    public void CreateStartInfo_WhenGeneratedCoreRootIsOutsideRepositoryRoot_MountsGeneratedCoreSeparately() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string generatedCoreRootPath = Path.Combine(Path.GetTempPath(), "wii-docker-generated-core", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(generatedCoreRootPath);

        try {
            WiiBuilderPaths paths = new(
                repositoryRootPath,
                generatedCoreRootPath,
                Path.Combine(repositoryRootPath, "tmp", "staged-content"),
                Path.Combine(repositoryRootPath, "tmp", "disc"),
                Path.Combine(repositoryRootPath, "tmp", "game.iso"),
                Path.Combine(repositoryRootPath, "tmp", "native", "helengine_wii.dol"));
            MethodInfo createStartInfoMethod = typeof(WiiDockerNativeBuildExecutor).GetMethod(
                "CreateStartInfo",
                BindingFlags.Static | BindingFlags.NonPublic);

            Assert.NotNull(createStartInfoMethod);

            ProcessStartInfo startInfo = (ProcessStartInfo)createStartInfoMethod.Invoke(null, [paths]);

            Assert.Contains(repositoryRootPath + ":/workspace", startInfo.ArgumentList);
            Assert.Contains(generatedCoreRootPath + ":/helengine-generated-core", startInfo.ArgumentList);
            Assert.Contains("HELENGINE_CORE_CPP_ROOT=/helengine-generated-core", startInfo.ArgumentList);
            Assert.False(startInfo.UseShellExecute);
            Assert.True(startInfo.RedirectStandardOutput);
            Assert.True(startInfo.RedirectStandardError);
        } finally {
            if (Directory.Exists(generatedCoreRootPath)) {
                Directory.Delete(generatedCoreRootPath, true);
            }
        }
    }
}
