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
        RasterSubmission = 0xC104U
    };
}
