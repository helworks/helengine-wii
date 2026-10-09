#pragma once

#include <cstddef>
#include <cstdint>

#include <gccore.h>

#include "RuntimeTexture.hpp"
#include "runtime/array.hpp"
#include "platform/wii/WiiNativeTextureLayout.hpp"

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

        /// Rewrites a direct RGBA region in native tiles and publishes modified bytes without reallocating its texture.
        void UpdateRegionFromRgba8(int32_t x, int32_t y, int32_t width, int32_t height, Array<uint8_t>* rgba8, int32_t sourceRowPitch);

        /// Loads the complete TLUT before binding its associated sampler to a hardware texture map.
        void BindNativeTexture(std::uint8_t map);

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

        /// Loads validated GXN1 native tiles and contiguous big-endian palette bytes without RGBA expansion.
        void LoadNativePayload(TextureAsset* data);

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

        /// Owned aligned palette memory retained with its texture until the GPU retirement fence.
        void* NativePaletteData;

        /// Descriptor carrying the palette's native encoding and actual entry count.
        GXTlutObj NativePaletteObject;

        /// Canonical GXN1 layout including physical sampler mapping for depth-copy views.
        WiiNativeTextureLayout NativeLayout;

        /// Separates native direct update policy from the historical raw RGB5A3 conversion.
        bool UsesNativePayload;
    };
}
