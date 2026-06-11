#include "platform/wii/WiiRuntimeModel.hpp"

namespace helengine::wii {
    /// Creates one concrete runtime-model instance that the Wii renderer can populate from cooked/shared model assets.
    WiiRuntimeModel::WiiRuntimeModel()
        : RuntimeModel()
        , Positions(nullptr)
        , Normals(nullptr)
        , TexCoords(nullptr)
        , Indices16(nullptr)
        , Indices32(nullptr)
        , Uses32BitIndices(false)
        , CachedMeshData(nullptr)
        , OwnedSourceModelAsset(nullptr) {
    }
}
