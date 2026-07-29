#pragma once

#include <cstdint>

namespace helengine::wii {
    /// Identifies the last risky Wii runtime boundary entered before a visible failure.
    enum class WiiFailureCode : uint16_t {
        /// No more specific runtime boundary was assigned.
        Unknown = 0xE000U,

        /// The generated core object was being constructed.
        CoreConstruction = 0xA001U,

        /// The generated core initialization options were being read.
        CoreOptions = 0xA002U,

        /// Packaged disc storage was being initialized.
        PackagedStorage = 0xA003U,

        /// Packaged content paths and the scene catalog were being configured.
        ContentConfiguration = 0xA004U,

        /// Platform bridge services were being constructed.
        BridgeConstruction = 0xA005U,

        /// The primary render window was being registered.
        PrimaryWindow = 0xA006U,

        /// The generated core initialization call was active.
        CoreInitialization = 0xA007U,

        /// Generated runtime modules were being registered.
        RuntimeModuleRegistration = 0xA008U,

        /// The packaged startup scene was being queued.
        SceneQueue = 0xB001U,

        /// The update path rejected invalid initialized state.
        UpdatePrecondition = 0xB002U,

        /// The 2D frame capture was beginning.
        BeginFrame = 0xB003U,

        /// The generated core update call was active.
        CoreUpdate = 0xB004U,

        /// Released render assets were being flushed after update.
        ReleaseFlush = 0xB005U,

        /// The draw path rejected invalid initialized state.
        DrawPrecondition = 0xC001U,

        /// The generated core draw call was active.
        CoreDraw = 0xC002U,

        /// Captured 2D commands were being submitted to the Wii renderer.
        RenderCapturedCommands = 0xC003U,

        /// Pending scene operations and packaged assets were being committed at the generated draw boundary.
        SceneCommit = 0xC101U,

        /// The Wii 2D renderer was capturing overlay drawables for the native frame.
        OverlayCapture = 0xC102U,

        /// The Wii scene bridge was constructing and validating the native frame plan.
        FramePlanBuild = 0xC103U,

        /// The Wii raster renderer was submitting the native frame plan through GX.
        RasterSubmission = 0xC104U,

        /// The scene manager was committing a queued operation at the generated frame boundary.
        SceneOperationCommit = 0xC110U,

        /// The scene manager was resolving the content path for a requested scene.
        ScenePathResolution = 0xC111U,

        /// The scene manager was locating or preparing the record that tracks a loaded scene.
        SceneRecordLookup = 0xC112U,

        /// The scene manager was beginning the requested scene content load.
        SceneContentLoad = 0xC113U,

        /// The scene manager was calling the service that materializes scene content.
        SceneMaterializationCall = 0xC114U,

        /// The scene materialization service had returned control to the scene manager.
        SceneMaterializationReturn = 0xC115U,

        /// The scene manager was preparing to track a newly materialized scene record.
        SceneRecordTrack = 0xC116U,

        /// The scene manager had added a scene record to its ordered loaded-scene list.
        SceneRecordListInsertion = 0xC117U,

        /// The scene manager had added a scene record to its lookup dictionary.
        SceneRecordDictionaryInsertion = 0xC118U,

        /// The scene manager was dispatching the scene-loaded event.
        SceneLoadedEvent = 0xC119U,

        /// The scene-loaded event dispatch had returned to generated core code.
        SceneLoadedEventReturned = 0xC11AU,

        /// Generated core code was about to release scene-loaded event arguments.
        SceneLoadedEventArgsRelease = 0xC11BU,

        /// Generated core code had released scene-loaded event arguments.
        SceneLoadedEventArgsReleased = 0xC11CU,

        /// Generated scene loading was releasing transition assets or completing cleanup.
        SceneLoadCleanup = 0xC11DU,

        /// The scene manager was registering textures owned by a newly loaded scene.
        OwnedTextureRegistration = 0xC140U,

        /// The scene manager was registering fonts owned by a newly loaded scene.
        OwnedFontRegistration = 0xC141U,

        /// The scene manager was registering audio assets owned by a newly loaded scene.
        OwnedAudioRegistration = 0xC142U,

        /// The scene manager was registering models owned by a newly loaded scene.
        OwnedModelRegistration = 0xC143U,

        /// The scene manager was registering materials owned by a newly loaded scene.
        OwnedMaterialRegistration = 0xC144U,

        /// The scene manager had completed registration of all assets owned by a newly loaded scene.
        OwnedAssetRegistrationCompleted = 0xC145U,

        /// The generated core draw method had entered its frame setup boundary.
        DrawSetup = 0xC105U,

        /// The generated core had completed or exited frame-boundary scene bookkeeping.
        FrameBoundaryBookkeeping = 0xC106U,

        /// The generated core was about to call the platform 3D render manager.
        RenderManagerBoundary = 0xC107U,

        /// The generated core had returned from native rendering and was recording frame metrics.
        PostRenderMetrics = 0xC108U,

        /// The generated core was recording the FPS component's rendered-frame counter.
        FpsRenderFrame = 0xC109U,

        /// The generated core was recording the debug component's rendered-frame counter.
        DebugRenderFrame = 0xC10AU
    };
}
