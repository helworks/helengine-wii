using System.Reflection;

namespace helengine.wii.builder.tests;

/// <summary>
/// Verifies the builder-owned Wii disc metadata normalization rules.
/// </summary>
public sealed class WiiBuildWorkspaceDiscMetadataTests {
    /// <summary>
    /// Ensures the generated Wii ID6 always follows a valid Wii header layout with a retail-safe region and maker code.
    /// </summary>
    [Fact]
    public void CreateDiscId_WhenProjectIdIsCity_ReturnsValidWiiShapedId6() {
        MethodInfo createDiscIdMethod = typeof(WiiBuildWorkspace).GetMethod("CreateDiscId", BindingFlags.Static | BindingFlags.NonPublic)
            ?? throw new InvalidOperationException("Could not resolve WiiBuildWorkspace.CreateDiscId.");

        string discId = (string)(createDiscIdMethod.Invoke(null, ["city"]) ?? throw new InvalidOperationException("CreateDiscId returned null."));

        Assert.Equal("RCIE01", discId);
    }
}
