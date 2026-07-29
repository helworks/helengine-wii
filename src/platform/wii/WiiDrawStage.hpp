#pragma once

#include <cstdint>

namespace helengine::wii {
    /// Identifies the last Wii-native rendering boundary entered during one generated-core draw call.
    enum class WiiDrawStage : uint8_t {
        /// No Wii-native rendering boundary has been entered for the current draw call.
        None,

        /// The Wii 2D render manager is capturing overlay drawables for the current frame.
        OverlayCapture,

        /// The Wii scene bridge is constructing and validating the native frame plan.
        FramePlanBuild,

        /// The Wii raster renderer is submitting the completed frame plan through GX.
        RasterSubmission
    };
}
