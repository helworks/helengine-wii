#include "platform/wii/WiiRuntimeTexture.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <malloc.h>
#include <ogc/cache.h>
#include <ogc/system.h>

#include "TextureAsset.hpp"
#include "TextureAssetColorFormat.hpp"
#include "runtime/native_exceptions.hpp"

namespace {
    /// Converts one 8-bit channel into the 5-bit range used by opaque GX RGB5A3 texels.
    uint16_t Convert8To5(uint8_t value) {
        return static_cast<uint16_t>((static_cast<uint32_t>(value) * 31U + 127U) / 255U);
    }

    /// Converts one 8-bit channel into the 4-bit range used by translucent GX RGB5A3 texels.
    uint16_t Convert8To4(uint8_t value) {
        return static_cast<uint16_t>((static_cast<uint32_t>(value) * 15U + 127U) / 255U);
    }

    /// Converts one 8-bit alpha channel into the 3-bit range used by translucent GX RGB5A3 texels.
    uint16_t Convert8To3(uint8_t value) {
        return static_cast<uint16_t>((static_cast<uint32_t>(value) * 7U + 127U) / 255U);
    }

    /// Encodes one logical RGBA texel into the GX RGB5A3 packed representation.
    uint16_t EncodeRgb5A3Pixel(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
        if (alpha >= 224U) {
            return static_cast<uint16_t>(
                0x8000U
                | (Convert8To5(red) << 10)
                | (Convert8To5(green) << 5)
                | Convert8To5(blue));
        }

        return static_cast<uint16_t>(
            (Convert8To3(alpha) << 12)
            | (Convert8To4(red) << 8)
            | (Convert8To4(green) << 4)
            | Convert8To4(blue));
    }
}

namespace helengine::wii {
    /// Creates one empty Wii runtime texture with no native texture memory yet.
    WiiRuntimeTexture::WiiRuntimeTexture()
        : RuntimeTexture()
        , NativeTextureData(nullptr)
        , NativeTextureDataSize(0U)
        , NativeTextureObject {}
        , NativeTextureObjectInitialized(false)
        , NativeTextureWidth(0U)
        , NativeTextureHeight(0U) {
    }

    /// Releases any owned native texture memory and GX texture state.
    WiiRuntimeTexture::~WiiRuntimeTexture() {
        ResetNativeTextureData();
    }

    /// Loads one shared-engine texture asset into one Wii-native GX texture object.
    void WiiRuntimeTexture::LoadFromRaw(TextureAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        }

        SYS_Report(
            "[Wii] WiiRuntimeTexture::LoadFromRaw width=%u height=%u format=%d colors=%p palette=%p\n",
            data->Width,
            data->Height,
            static_cast<int32_t>(data->ColorFormat),
            data->Colors,
            data->PaletteColors);
        ResetNativeTextureData();
        if (data->ColorFormat == TextureAssetColorFormat::GxRgb5A3) {
            SYS_Report("[Wii] WiiRuntimeTexture::LoadFromRaw using prepacked GxRgb5A3 path.\n");
            LoadPrepackedRgb5A3(data);
            return;
        } else if (data->ColorFormat != TextureAssetColorFormat::Rgba32) {
            throw new InvalidOperationException("Wii runtime textures require either GxRgb5A3 or RGBA32 texture assets.");
        }

        SYS_Report("[Wii] WiiRuntimeTexture::LoadFromRaw using RGBA32 transcode path.\n");
        EncodeRgba32ToRgb5A3(data);
    }

    /// Returns whether one native GX texture object has been initialized for this runtime texture.
    bool WiiRuntimeTexture::HasNativeTextureObject() const {
        return NativeTextureObjectInitialized;
    }

    /// Returns the native GX texture object used by glyph rendering.
    GXTexObj* WiiRuntimeTexture::GetNativeTextureObject() {
        if (!NativeTextureObjectInitialized) {
            throw new InvalidOperationException("Wii runtime texture does not own an initialized GX texture object.");
        }

        return &NativeTextureObject;
    }

    /// Returns the native GX texture width used by the uploaded texture object.
    uint32_t WiiRuntimeTexture::GetNativeTextureWidth() const {
        return NativeTextureWidth;
    }

    /// Returns the native GX texture height used by the uploaded texture object.
    uint32_t WiiRuntimeTexture::GetNativeTextureHeight() const {
        return NativeTextureHeight;
    }

    /// Releases any previously allocated native texture memory and resets the GX texture object.
    void WiiRuntimeTexture::ResetNativeTextureData() {
        if (NativeTextureData != nullptr) {
            free(NativeTextureData);
            NativeTextureData = nullptr;
        }

        NativeTextureDataSize = 0U;
        std::memset(&NativeTextureObject, 0, sizeof(NativeTextureObject));
        NativeTextureObjectInitialized = false;
        NativeTextureWidth = 0U;
        NativeTextureHeight = 0U;
    }

    /// Loads one prepacked GX RGB5A3 payload that is already stored in native tiled texture memory order.
    void WiiRuntimeTexture::LoadPrepackedRgb5A3(TextureAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        } else if (data->Width == 0U || data->Height == 0U) {
            throw new InvalidOperationException("Wii runtime textures require nonzero dimensions.");
        } else if (data->Colors == nullptr) {
            throw new InvalidOperationException("Wii runtime textures require prepacked RGB5A3 color data.");
        }

        const uint32_t width = data->Width;
        const uint32_t height = data->Height;
        const uint32_t paddedWidth = (width + 3U) & ~3U;
        const uint32_t paddedHeight = (height + 3U) & ~3U;
        const std::size_t expectedColorByteCount = static_cast<std::size_t>(paddedWidth) * static_cast<std::size_t>(paddedHeight) * 2U;
        SYS_Report(
            "[Wii] LoadPrepackedRgb5A3 padded=%ux%u expectedBytes=%u actualBytes=%d\n",
            paddedWidth,
            paddedHeight,
            static_cast<uint32_t>(expectedColorByteCount),
            data->Colors->Length);
        if (data->Colors->Length != static_cast<int32_t>(expectedColorByteCount)) {
            throw new InvalidOperationException("Wii prepacked textures must contain padded tiled RGB5A3 bytes.");
        }

        NativeTextureDataSize = expectedColorByteCount;
        NativeTextureData = memalign(32, NativeTextureDataSize);
        if (NativeTextureData == nullptr) {
            throw new InvalidOperationException("Could not allocate Wii texture memory.");
        }

        std::memcpy(NativeTextureData, &(*data->Colors)[0], NativeTextureDataSize);
        DCFlushRange(NativeTextureData, NativeTextureDataSize);
        GX_InitTexObj(&NativeTextureObject, NativeTextureData, paddedWidth, paddedHeight, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GX_InitTexObjFilterMode(&NativeTextureObject, GX_LINEAR, GX_LINEAR);
        NativeTextureObjectInitialized = true;
        NativeTextureWidth = paddedWidth;
        NativeTextureHeight = paddedHeight;
        this->set_Width(static_cast<int32_t>(width));
        this->set_Height(static_cast<int32_t>(height));
        SYS_Report("[Wii] LoadPrepackedRgb5A3 upload completed native=%ux%u\n", paddedWidth, paddedHeight);
    }

    /// Encodes one logical RGBA32 texture into tiled GX RGB5A3 memory for Wii text rendering.
    void WiiRuntimeTexture::EncodeRgba32ToRgb5A3(TextureAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        } else if (data->Width == 0 || data->Height == 0) {
            throw new InvalidOperationException("Wii runtime textures require nonzero dimensions.");
        } else if (data->Colors == nullptr) {
            throw new InvalidOperationException("Wii runtime textures require RGBA32 color data.");
        }

        const uint32_t width = data->Width;
        const uint32_t height = data->Height;
        const std::size_t expectedColorByteCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
        SYS_Report(
            "[Wii] EncodeRgba32ToRgb5A3 source=%ux%u expectedBytes=%u actualBytes=%d\n",
            width,
            height,
            static_cast<uint32_t>(expectedColorByteCount),
            data->Colors->get_Length());
        if (data->Colors->get_Length() != static_cast<int32_t>(expectedColorByteCount)) {
            throw new InvalidOperationException("Wii runtime textures require tightly packed RGBA32 color bytes.");
        }

        const uint32_t nativeWidth = (width + 3U) & ~3U;
        const uint32_t nativeHeight = (height + 3U) & ~3U;
        NativeTextureDataSize = static_cast<std::size_t>(nativeWidth) * static_cast<std::size_t>(nativeHeight) * 2U;
        NativeTextureData = memalign(32, NativeTextureDataSize);
        if (NativeTextureData == nullptr) {
            throw new InvalidOperationException("Could not allocate Wii texture memory.");
        }

        std::memset(NativeTextureData, 0, NativeTextureDataSize);
        uint16_t* destination = static_cast<uint16_t*>(NativeTextureData);
        for (uint32_t blockY = 0; blockY < nativeHeight; blockY += 4U) {
            for (uint32_t blockX = 0; blockX < nativeWidth; blockX += 4U) {
                for (uint32_t innerY = 0; innerY < 4U; innerY++) {
                    for (uint32_t innerX = 0; innerX < 4U; innerX++) {
                        const uint32_t sampleX = std::min(blockX + innerX, width - 1U);
                        const uint32_t sampleY = std::min(blockY + innerY, height - 1U);
                        const std::size_t sourceOffset = (static_cast<std::size_t>(sampleY) * static_cast<std::size_t>(width) + sampleX) * 4U;
                        *destination++ = EncodeRgb5A3Pixel(
                            (*data->Colors)[static_cast<int32_t>(sourceOffset + 0U)],
                            (*data->Colors)[static_cast<int32_t>(sourceOffset + 1U)],
                            (*data->Colors)[static_cast<int32_t>(sourceOffset + 2U)],
                            (*data->Colors)[static_cast<int32_t>(sourceOffset + 3U)]);
                    }
                }
            }
        }

        DCFlushRange(NativeTextureData, NativeTextureDataSize);
        GX_InitTexObj(&NativeTextureObject, NativeTextureData, nativeWidth, nativeHeight, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GX_InitTexObjFilterMode(&NativeTextureObject, GX_LINEAR, GX_LINEAR);
        NativeTextureObjectInitialized = true;
        NativeTextureWidth = nativeWidth;
        NativeTextureHeight = nativeHeight;
        this->set_Width(static_cast<int32_t>(width));
        this->set_Height(static_cast<int32_t>(height));
        SYS_Report("[Wii] EncodeRgba32ToRgb5A3 upload completed native=%ux%u\n", nativeWidth, nativeHeight);
    }
}
