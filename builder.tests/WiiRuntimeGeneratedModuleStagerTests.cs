using System.Reflection;

namespace helengine.wii.builder.tests;

/// <summary>
/// Covers the Wii runtime generated-module staging registration output.
/// </summary>
public sealed class WiiRuntimeGeneratedModuleStagerTests : IDisposable {
    /// <summary>
    /// Temporary root used by the test-generated core files.
    /// </summary>
    readonly string TempRootPath;

    /// <summary>
    /// Creates one isolated temporary workspace for each test.
    /// </summary>
    public WiiRuntimeGeneratedModuleStagerTests() {
        TempRootPath = Path.Combine(Path.GetTempPath(), "helengine-wii-stager-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(TempRootPath);
    }

    /// <summary>
    /// Rebuilds the generated runtime registration source from the generated deserializer headers already present in generated-core.
    /// </summary>
    [Fact]
    public void WriteGeneratedRuntimeComponentDeserializerRegistrationFromGeneratedCore_WhenGeneratedDeserializerHeadersExist_RegistersAllGeneratedDeserializers() {
        string generatedCoreRootPath = Path.Combine(TempRootPath, "generated-core");
        Directory.CreateDirectory(generatedCoreRootPath);
        File.WriteAllText(Path.Combine(generatedCoreRootPath, "GeneratedRuntimeMenuComponentDeserializer.hpp"), "#pragma once\n");
        File.WriteAllText(Path.Combine(generatedCoreRootPath, "GeneratedRuntimeRoundedRectComponentDeserializer.hpp"), "#pragma once\n");
        File.WriteAllText(Path.Combine(generatedCoreRootPath, "GeneratedRuntimeComponentDeserializerRegistration.hpp"), "#pragma once\n");
        WiiRuntimeGeneratedModuleStager stager = new WiiRuntimeGeneratedModuleStager();
        MethodInfo writeRegistrationMethod = typeof(WiiRuntimeGeneratedModuleStager).GetMethod(
            "WriteGeneratedRuntimeComponentDeserializerRegistrationFromGeneratedCore",
            BindingFlags.Instance | BindingFlags.NonPublic)
            ?? throw new InvalidOperationException("Could not resolve WriteGeneratedRuntimeComponentDeserializerRegistrationFromGeneratedCore.");

        writeRegistrationMethod.Invoke(stager, [generatedCoreRootPath]);

        string registrationSource = File.ReadAllText(Path.Combine(generatedCoreRootPath, "GeneratedRuntimeComponentDeserializerRegistration.cpp"));
        Assert.Contains("#include \"GeneratedRuntimeMenuComponentDeserializer.hpp\"", registrationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"GeneratedRuntimeRoundedRectComponentDeserializer.hpp\"", registrationSource, StringComparison.Ordinal);
        Assert.Contains("registry->Register(new ::GeneratedRuntimeMenuComponentDeserializer());", registrationSource, StringComparison.Ordinal);
        Assert.Contains("registry->Register(new ::GeneratedRuntimeRoundedRectComponentDeserializer());", registrationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("registry->Register(new ::GeneratedRuntimeComponentDeserializerRegistration());", registrationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Rewrites generated native path support so Wii virtual <c>dvd:/</c> roots bypass host-style filesystem normalization.
    /// </summary>
    [Fact]
    public void AdaptGeneratedPathSource_WhenGeneratedPathSupportExists_PreservesWiiVirtualRootHandling() {
        string generatedPathSourcePath = Path.Combine(TempRootPath, "generated-core", "system", "io", "path.cpp");
        Directory.CreateDirectory(Path.GetDirectoryName(generatedPathSourcePath) ?? throw new InvalidOperationException("Generated path helper directory path could not be resolved."));
        File.WriteAllText(generatedPathSourcePath, """
#include "path.hpp"

#include "helcpp_config.hpp"

#include <algorithm>
#include <filesystem>

#if HE_CPP_PLATFORM_PS2
namespace {
    bool IsPs2DevicePath(const std::string& path) {
        return path.rfind("cdrom0:", 0) == 0;
    }

    std::string CombinePs2Path(const std::string& left, const std::string& right) {
        return left + right;
    }

    std::string GetPs2DirectoryName(const std::string& path) {
        return path;
    }

    std::string GetPs2FileName(const std::string& path) {
        return path;
    }

    std::string NormalizePs2Path(const std::string& path) {
        return path;
    }
}
#endif

#if HELENGINE_NINTENDO_DS_HAS_GENERATED_CORE
namespace {
    bool IsNintendoDsDevicePath(const std::string& path) {
        return path.rfind("nitro:", 0) == 0;
    }
}
#endif

std::string Path::Combine(const std::string& left, const std::string& right) {
#if HE_CPP_PLATFORM_PS2
    if (IsPs2DevicePath(left) || IsPs2DevicePath(right)) {
        return CombinePs2Path(left, right);
    }
#endif
#if HELENGINE_NINTENDO_DS_HAS_GENERATED_CORE
    if (IsNintendoDsDevicePath(left)) {
        if (right.empty()) {
            return left;
        }

        if (right[0] == '/') {
            return left + right;
        }

        return left + "/" + right;
    }
#endif
    if (left.empty()) {
        return right;
    }

    if (right.empty()) {
        return left;
    }

    return (std::filesystem::path(left) / right).lexically_normal().string();
}

std::string Path::GetDirectoryName(const std::string& path) {
    if (path.empty()) {
        return std::string();
    }

#if HE_CPP_PLATFORM_PS2
    if (IsPs2DevicePath(path)) {
        return GetPs2DirectoryName(path);
    }
#endif
    return std::filesystem::path(path).parent_path().string();
}

std::string Path::GetFileName(const std::string& path) {
    if (path.empty()) {
        return std::string();
    }

#if HE_CPP_PLATFORM_PS2
    if (IsPs2DevicePath(path)) {
        return GetPs2FileName(path);
    }
#endif
    return std::filesystem::path(path).filename().string();
}

std::string Path::GetFullPath(const std::string& path) {
#if HELENGINE_NINTENDO_DS_HAS_GENERATED_CORE
    if (IsNintendoDsDevicePath(path)) {
        return path;
    }
#endif
#if !HE_CPP_PLATFORM_IS_WINDOWS_HOST
    if (path.empty()) {
        return std::string(".");
    }

#if HE_CPP_PLATFORM_PS2
    if (IsPs2DevicePath(path)) {
        return NormalizePs2Path(path);
    }
#endif
    return std::filesystem::path(path).lexically_normal().string();
#else
    if (path.empty()) {
        return std::filesystem::current_path().string();
    }

    return std::filesystem::absolute(std::filesystem::path(path)).lexically_normal().string();
#endif
}

bool Path::IsPathRooted(const std::string& path) {
    if (path.empty()) {
        return false;
    }

#if HE_CPP_PLATFORM_PS2
    if (IsPs2DevicePath(path)) {
        return true;
    }
#endif
#if HELENGINE_NINTENDO_DS_HAS_GENERATED_CORE
    if (IsNintendoDsDevicePath(path)) {
        return true;
    }
#endif
    return std::filesystem::path(path).is_absolute();
}
""");
        WiiRuntimeGeneratedModuleStager stager = new WiiRuntimeGeneratedModuleStager();
        MethodInfo adaptPathMethod = typeof(WiiRuntimeGeneratedModuleStager).GetMethod(
            "AdaptGeneratedPathSource",
            BindingFlags.Instance | BindingFlags.NonPublic)
            ?? throw new InvalidOperationException("Could not resolve AdaptGeneratedPathSource.");

        adaptPathMethod.Invoke(stager, [generatedPathSourcePath]);

        string pathSource = File.ReadAllText(generatedPathSourcePath);
        Assert.Contains("bool IsWiiDevicePath(const std::string& path)", pathSource, StringComparison.Ordinal);
        Assert.Contains("if (IsWiiDevicePath(left))", pathSource, StringComparison.Ordinal);
        Assert.Contains("return std::string(\"dvd:/\");", pathSource, StringComparison.Ordinal);
        Assert.Contains("if (IsWiiDevicePath(path)) {", pathSource, StringComparison.Ordinal);
        Assert.Contains("return true;", pathSource, StringComparison.Ordinal);
        Assert.DoesNotContain("#endif#if", pathSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Removes the temporary workspace after each test.
    /// </summary>
    public void Dispose() {
        if (Directory.Exists(TempRootPath)) {
            Directory.Delete(TempRootPath, true);
        }
    }
}
