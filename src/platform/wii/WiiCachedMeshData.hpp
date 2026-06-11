#pragma once

#include <cstdint>

#include "float2.hpp"
#include "float3.hpp"
#include "runtime/array.hpp"

namespace helengine::wii {
    /// Stores one tightly packed GX position entry without managed-runtime object overhead.
    struct WiiPackedPosition3 {
        float X;
        float Y;
        float Z;
    };

    /// Stores one tightly packed GX normal entry without managed-runtime object overhead.
    struct WiiPackedNormal3 {
        float X;
        float Y;
        float Z;
    };

    /// Stores one tightly packed GX texture-coordinate entry without managed-runtime object overhead.
    struct WiiPackedTexCoord2 {
        float U;
        float V;
    };

    /// Owns the cached Wii-native mesh data that the GX renderer can reuse across frames.
    class WiiCachedMeshData {
    public:
        /// Creates an empty cached mesh container before cached arrays are attached.
        WiiCachedMeshData()
            : PackedPositions(nullptr)
            , PackedPositionBuffer(nullptr)
            , PackedNormals(nullptr)
            , Normals(nullptr)
            , PackedNormalBuffer(nullptr)
            , PackedTexCoords(nullptr)
            , PackedTexCoordBuffer(nullptr)
            , Indices16(nullptr)
            , SubmeshIndexStarts(nullptr)
            , SubmeshIndexCounts(nullptr)
            , HasNormals(false)
            , HasTexCoords(false) {
        }

        /// Packed positions stored for the default Wii indexed draw path.
        Array<WiiPackedPosition3>* PackedPositions;

        /// 32-byte-aligned packed position buffer bound directly to GX.
        WiiPackedPosition3* PackedPositionBuffer;

        /// Packed normals stored for later lit Wii draw paths.
        Array<WiiPackedNormal3>* PackedNormals;

        /// Cached normals stored when the source mesh supports lit rendering.
        Array<float3>* Normals;

        /// 32-byte-aligned packed normal buffer bound directly to GX for later lit paths.
        WiiPackedNormal3* PackedNormalBuffer;

        /// Packed texture coordinates stored when the source mesh supports textured rendering.
        Array<WiiPackedTexCoord2>* PackedTexCoords;

        /// 32-byte-aligned packed texture-coordinate buffer bound directly to GX for later textured paths.
        WiiPackedTexCoord2* PackedTexCoordBuffer;

        /// Cached 16-bit indices used by the Wii draw path.
        Array<uint16_t>* Indices16;

        /// Cached submesh start offsets measured in cached index entries.
        Array<int32_t>* SubmeshIndexStarts;

        /// Cached submesh index counts for each authored submesh.
        Array<int32_t>* SubmeshIndexCounts;

        /// Tracks whether cached normals were attached for lit rendering.
        bool HasNormals;

        /// Tracks whether cached texture coordinates were attached for textured rendering.
        bool HasTexCoords;
    };
}
