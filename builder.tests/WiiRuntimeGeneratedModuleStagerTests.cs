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
    /// Removes the temporary workspace after each test.
    /// </summary>
    public void Dispose() {
        if (Directory.Exists(TempRootPath)) {
            Directory.Delete(TempRootPath, true);
        }
    }
}
