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

        /// Pending scene-operation commit was entering, executing a queued operation, or completing at the generated frame boundary.
        SceneOperationCommit = 0xC110U,

        /// Immediate scene loading had entered or was about to resolve the requested scene's content path.
        ScenePathResolution = 0xC111U,

        /// Immediate scene loading had resolved the content path and was looking up the loaded-scene record or performing single-mode pre-load cleanup.
        /// Cleanup includes disposing untracked roots, unloading existing scenes, flushing released textures, and resetting physics timing.
        SceneRecordLookup = 0xC112U,

        /// Immediate scene loading was about to load the resolved scene content.
        SceneContentLoad = 0xC113U,

        /// The scene manager was about to call the scene-load service to materialize resolved scene content.
        SceneMaterializationCall = 0xC114U,

        /// The scene-load service had returned materialized scene content to the scene manager.
        SceneMaterializationReturn = 0xC115U,

        /// The scene manager was about to track the newly materialized loaded-scene record.
        SceneRecordTrack = 0xC116U,

        /// The scene manager had inserted the newly materialized record into its ordered loaded-scene list.
        SceneRecordListInsertion = 0xC117U,

        /// The scene manager had inserted the newly materialized record into its loaded-scene lookup dictionary.
        SceneRecordDictionaryInsertion = 0xC118U,

        /// The scene manager was about to dispatch the scene-loaded event for the newly tracked scene.
        SceneLoadedEvent = 0xC119U,

        /// The scene-loaded event dispatch had returned control to generated core code.
        SceneLoadedEventReturned = 0xC11AU,

        /// Generated core code was about to release the scene-loaded event arguments after dispatch.
        SceneLoadedEventArgsRelease = 0xC11BU,

        /// Generated core code had released the scene-loaded event arguments after dispatch.
        SceneLoadedEventArgsReleased = 0xC11CU,

        /// Immediate scene loading had returned from the scene-loaded event or reached its end, or transition cleanup had released scene assets or committed the transition.
        SceneLoadCleanup = 0xC11DU,

        /// Scene materialization had begun or was about to load the authored root entity.
        SceneMaterializationBegin = 0xC120U,

        /// Scene materialization had begun constructing an authored entity.
        SceneEntityConstruction = 0xC121U,

        /// Scene materialization was preparing to deserialize or had begun deserializing an authored component with no more specific mapping.
        SceneComponentDeserialization = 0xC122U,

        /// Scene materialization was about to load an authored child entity.
        SceneChildEntity = 0xC123U,

        /// Scene materialization had completed loading an authored entity or was initializing the materialized hierarchy after the final entity completed.
        SceneEntityCompletion = 0xC124U,

        /// Scene materialization had completed reconstruction of the entire authored scene graph.
        SceneMaterializationCompleted = 0xC125U,

        /// Scene materialization was preparing to deserialize or had begun deserializing a camera component.
        CameraComponentDeserialization = 0xC130U,

        /// Scene materialization was preparing to deserialize or had begun deserializing a rounded-rectangle component.
        RoundedRectComponentDeserialization = 0xC131U,

        /// Scene materialization was preparing to deserialize or had begun deserializing a viewport component.
        ViewportComponentDeserialization = 0xC132U,

        /// Scene materialization was preparing to deserialize or had begun deserializing a reference-canvas-fit component.
        ReferenceCanvasFitComponentDeserialization = 0xC133U,

        /// Scene materialization was preparing to deserialize or had begun deserializing the Helen of Code splash component.
        SplashComponentDeserialization = 0xC134U,

        /// Scene materialization was preparing to deserialize or had begun deserializing a sprite component.
        SpriteComponentDeserialization = 0xC135U,

        /// The scene manager was about to register textures owned by the newly loaded scene.
        OwnedTextureRegistration = 0xC140U,

        /// The scene manager was about to register fonts owned by the newly loaded scene.
        OwnedFontRegistration = 0xC141U,

        /// The scene manager was about to register audio assets owned by the newly loaded scene.
        OwnedAudioRegistration = 0xC142U,

        /// The scene manager was about to register models owned by the newly loaded scene.
        OwnedModelRegistration = 0xC143U,

        /// The scene manager was about to register materials owned by the newly loaded scene.
        OwnedMaterialRegistration = 0xC144U,

        /// The scene manager had registered every owned asset category for the newly loaded scene.
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
