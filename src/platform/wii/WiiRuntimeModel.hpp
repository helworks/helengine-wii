#pragma once

#include "platform/wii/WiiCachedMeshData.hpp"
#include "ModelAsset.hpp"
#include "RuntimeModel.hpp"
#include "float2.hpp"
#include "float3.hpp"
#include "runtime/array.hpp"

namespace helengine::wii {
    /// Exposes one concrete public RuntimeModel constructor for the Wii native runtime bridge.
    class WiiRuntimeModel final : public RuntimeModel {
    public:
        /// Creates one concrete runtime-model instance that the Wii renderer can populate from cooked/shared model assets.
        WiiRuntimeModel();

        /// Authored model positions used by the first Wii GX triangle path.
        Array<float3>* Positions;

        /// Authored model normals kept alive for later lit Wii rendering tiers.
        Array<float3>* Normals;

        /// Authored model texture coordinates kept alive for later textured Wii rendering tiers.
        Array<float2>* TexCoords;

        /// Authored 16-bit indices when the source mesh uses them.
        Array<uint16_t>* Indices16;

        /// Authored 32-bit indices when the source mesh uses them.
        Array<uint32_t>* Indices32;

        /// Tracks whether the runtime mesh must read from <c>Indices32</c>.
        bool Uses32BitIndices;

        /// Owns the cached Wii-native draw data built from the authored mesh arrays.
        WiiCachedMeshData* CachedMeshData;

        /// Owns one deserialized cooked model asset when the runtime model was created from a packaged payload.
        ModelAsset* OwnedSourceModelAsset;
    };
}
