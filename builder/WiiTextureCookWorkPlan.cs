using helengine.baseplatform.Manifest;

namespace helengine.wii.builder;

/// <summary>Retains a validated cook request and prepared serialized bytes until the complete batch can be published.</summary>
internal sealed class WiiTextureCookWorkPlan {
    /// <summary>Identifies the authored source and final logical output declared by the editor.</summary>
    public PlatformCookWorkItem WorkItem { get; init; }
    /// <summary>Holds settings that passed texture, alpha and quantizer validation.</summary>
    public WiiTextureCookSettings Settings { get; init; }
    /// <summary>Gets the exact staging root used for containment checks.</summary>
    public string StagingRootPath { get; init; }
    /// <summary>Gets the validated absolute destination within staging.</summary>
    public string DestinationPath { get; init; }
    /// <summary>Gets the validated source path which must remain unchanged during cooking.</summary>
    public string SourcePath { get; init; }
    /// <summary>Holds serialized bytes only after the source decode and native cook succeeded.</summary>
    public byte[] Payload { get; set; }
}
