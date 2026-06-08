#pragma once

namespace helengine::wii {
    /// Identifies the visible runtime startup phase reported through diagnostic clear colors.
    enum class WiiBootPhase {
        /// Native video and GX setup has not completed yet.
        NativeStartup,

        /// The generated core object is being constructed.
        CoreConstruction,

        /// Core initialization options are being read and configured.
        CoreOptions,

        /// Authored-scene bootstrap data is being resolved and validated.
        SceneBootstrap,

        /// Wii bridge services are being constructed.
        BridgeConstruction,

        /// The generated core is receiving its initialization call.
        CoreInitialization,

        /// The generated runtime scene is being queued for loading.
        SceneLoad,

        /// The generated core has initialized and the runtime frame loop is active.
        Running,

        /// The generated core update step is active.
        CoreUpdate,

        /// The generated core draw step is active.
        CoreDraw,

        /// The runtime failed and the visible frame should remain on a failure color.
        Failed
    };
}
