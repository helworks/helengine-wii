#pragma once

#include <cstddef>
#include <cstdint>

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

        /// Loads one shared-engine texture asset into one Wii-native GX texture object.
        void LoadFromRaw(TextureAsset* data);

        /// Returns whether one native GX texture object has been initialized for this runtime texture.
        bool HasNativeTextureObject() const;

        /// Returns the native GX texture object used by glyph rendering.
        GXTexObj* GetNativeTextureObject();

        /// Returns the native GX texture width used by the uploaded texture object.
        uint32_t GetNativeTextureWidth() const;

        /// Returns the native GX texture height used by the uploaded texture object.
        uint32_t GetNativeTextureHeight() const;

    private:
        /// Releases any previously allocated native texture memory and resets the GX texture object.
        void ResetNativeTextureData();

        /// Loads one prepacked GX RGB5A3 payload that is already stored in native tiled texture memory order.
        void LoadPrepackedRgb5A3(TextureAsset* data);

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

        /// Native uploaded GX texture width, rounded to a legal hardware dimension.
        uint32_t NativeTextureWidth;

        /// Native uploaded GX texture height, rounded to a legal hardware dimension.
        uint32_t NativeTextureHeight;
    };
}
