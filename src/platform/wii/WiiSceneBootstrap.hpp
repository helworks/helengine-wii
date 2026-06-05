#pragma once

#include "runtime/native_string.hpp"

class RuntimeSceneCatalog;

namespace helengine::wii {
    /// Declares the runtime scene-manifest helpers used by manifest-backed Wii startup.
    class WiiSceneBootstrap {
    public:
        /// Creates the runtime scene catalog emitted by the Wii builder.
        static RuntimeSceneCatalog* CreatePackagedSceneCatalog();

        /// Returns the startup scene id emitted by the Wii builder.
        static std::string GetPackagedStartupSceneId();
    };
}
