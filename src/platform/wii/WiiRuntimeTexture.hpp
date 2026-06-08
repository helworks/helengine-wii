#pragma once

#include <cstddef>

#include <gccore.h>

#include "RuntimeTexture.hpp"

class TextureAsset;

namespace helengine::wii {
    /// Stores one Wii-native GX texture object plus its encoded texture memory.
    class WiiRuntimeTexture final : public RuntimeTexture {
    public:
        /// Creates one empty Wii runtime texture with no native texture memory yet.
        WiiRuntimeTexture();

        /// Releases any owned native texture memory and GX texture state.
        ~WiiRuntimeTexture() override;

        /// Encodes one shared-engine texture asset into the first Wii texture format used by the menu text proof.
        void LoadFromRaw(TextureAsset* data);

        /// Returns whether one native GX texture object has been initialized for this runtime texture.
        bool HasNativeTextureObject() const;

        /// Returns the native GX texture object used by glyph rendering.
        GXTexObj* GetNativeTextureObject();

    private:
        /// Releases any previously allocated native texture memory and resets the GX texture object.
        void ResetNativeTextureData();

        /// Encodes one logical RGBA32 texture into tiled GX RGB5A3 memory for Wii text rendering.
        void EncodeRgba32ToRgb5A3(TextureAsset* data);

        /// Owned native texture-memory buffer containing GX-ready texel bytes.
        void* NativeTextureData;

        /// Size in bytes of the owned native texture-memory buffer.
        std::size_t NativeTextureDataSize;

        /// Native GX texture object initialized from `NativeTextureData`.
        GXTexObj NativeTextureObject;

        /// Tracks whether `NativeTextureObject` currently contains valid GX texture state.
        bool NativeTextureObjectInitialized;
    };
}
