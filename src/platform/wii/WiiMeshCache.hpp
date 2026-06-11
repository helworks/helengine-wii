#pragma once

#include "platform/wii/WiiCachedMeshData.hpp"

class RuntimeModel;

namespace helengine::wii {
    class WiiRuntimeModel;

    /// Validates authored runtime models once and reuses the backend-local geometry view across frames.
    class WiiMeshCache {
    public:
        /// Creates one Wii mesh cache that keeps cached data on each runtime-model instance.
        WiiMeshCache();

        /// Resolves a runtime model into a validated Wii runtime model or fails loudly.
        WiiRuntimeModel* Resolve(RuntimeModel* runtimeModel);

    private:
        /// Builds one cached Wii mesh representation from the authored runtime-model arrays.
        WiiCachedMeshData* BuildCachedMeshData(WiiRuntimeModel* runtimeModel);
    };
}
