#include "platform/wii/WiiRuntimeTexture.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <malloc.h>
#include <ogc/cache.h>
#include <ogc/system.h>

#include "TextureAsset.hpp"
#include "TextureAssetColorFormat.hpp"
#include "TextureAssetPixelCodec.hpp"
#include "runtime/native_exceptions.hpp"

namespace {
    /// Stable regions owned by this renderer, with nonoverlapping small and full fourteen-bit palette slots.
    static GXTlutRegion NativeTlutRegions[12] {};
    /// Callback retained for names not assigned by this renderer; initialization is global rather than tied to one texture lifetime.
    static GXTlutRegionCallback PreviousTlutCallback = nullptr;
    /// Records whether GX's existing callback was captured after GX initialization.
    static bool NativeTlutCallbackInstalled = false;

    /// Returns the same region object for a given renderer slot, preserving libogc's TLUT-name contract.
    static GXTlutRegion* ResolveNativeTlutRegion(u32 name) {
        if (name < 8) return &NativeTlutRegions[name];
        if (name >= 16 && name < 20) return &NativeTlutRegions[name - 8];
        return PreviousTlutCallback == nullptr ? nullptr : PreviousTlutCallback(name);
    }


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
        , NativeTextureHeight(0U)
        , NativePaletteData(nullptr)
        , NativePaletteObject {}
        , NativeLayout {}
        , UsesNativePayload(false) {
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
        if (data->ColorFormat == TextureAssetColorFormat::GxNative) {
            LoadNativePayload(data);
            return;
        } else if (data->ColorFormat == TextureAssetColorFormat::GxRgb5A3) {
            SYS_Report("[Wii] WiiRuntimeTexture::LoadFromRaw using prepacked GxRgb5A3 path.\n");
            LoadPrepackedRgb5A3(data);
            return;
        } else if (data->ColorFormat != TextureAssetColorFormat::Rgba32) {
            // Generic conversion is separate from the native tiled upload and keeps its decoded array locally owned.
            Array<uint8_t>* decoded = TextureAssetPixelCodec::DecodeToRgba32(data);
            try {
                TextureAsset rgba;
                rgba.Width = data->Width; rgba.Height = data->Height; rgba.Colors = decoded;
                EncodeRgba32ToRgb5A3(&rgba);
                delete decoded;
                return;
            } catch (...) {
                delete decoded;
                throw;
            }
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

        free(NativePaletteData);
        NativePaletteData = nullptr;
        NativePaletteObject = {};
        NativeLayout = {};
        UsesNativePayload = false;
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
    /// Loads native tiled bytes and TLUT entries without decoding into RGBA.
    void WiiRuntimeTexture::LoadNativePayload(TextureAsset* data) {
        WiiNativeTextureLayout layout;
        if (data->Colors == nullptr || data->Colors->Length < 0
            || (data->PaletteColors != nullptr && data->PaletteColors->Length < 0)
            || !TryParseNativeTexturePayload(data->Colors->Data, static_cast<std::size_t>(data->Colors->Length),
                data->PaletteColors == nullptr ? nullptr : data->PaletteColors->Data,
                data->PaletteColors == nullptr ? 0 : static_cast<std::size_t>(data->PaletteColors->Length), layout)
            || layout.Width != data->Width || layout.Height != data->Height
            || layout.AlphaPrecision != static_cast<std::uint32_t>(data->AlphaPrecision)) {
            throw new InvalidOperationException("Wii GXN1 header, metadata, tiled bytes or TLUT indices are invalid.");
        }
        NativeTextureData = memalign(32, layout.PixelBytes);
        if (NativeTextureData == nullptr) throw new InvalidOperationException("Could not allocate native Wii texture memory.");
        if (layout.PaletteBytes != 0) {
            NativePaletteData = memalign(32, layout.PaletteBytes);
            if (NativePaletteData == nullptr) {
                ResetNativeTextureData();
                throw new InvalidOperationException("Could not allocate native Wii TLUT memory.");
            }
            std::memcpy(NativePaletteData, data->PaletteColors->Data, layout.PaletteBytes);
            DCFlushRange(NativePaletteData, layout.PaletteBytes);
            GX_InitTlutObj(&NativePaletteObject, NativePaletteData, layout.TlutFormat, layout.PaletteEntries);
            GX_InitTexObjCI(&NativeTextureObject, NativeTextureData, layout.Width, layout.Height, layout.SamplerFormat,
                GX_CLAMP, GX_CLAMP, GX_FALSE, layout.SamplerFormat == 10 ? GX_BIGTLUT0 : GX_TLUT0);
        } else {
            GX_InitTexObj(&NativeTextureObject, NativeTextureData, layout.Width, layout.Height, layout.SamplerFormat,
                GX_CLAMP, GX_CLAMP, GX_FALSE);
        }
        std::memcpy(NativeTextureData, data->Colors->Data + 48, layout.PixelBytes);
        DCFlushRange(NativeTextureData, layout.PixelBytes);
        GX_InitTexObjFilterMode(&NativeTextureObject, GX_LINEAR, GX_LINEAR);
        NativeTextureDataSize = layout.PixelBytes;
        NativeTextureWidth = layout.Width;
        NativeTextureHeight = layout.Height;
        NativeLayout = layout;
        UsesNativePayload = true;
        NativeTextureObjectInitialized = true;
        this->set_Width(layout.Width); this->set_Height(layout.Height);
    }

    /// Binds the complete palette before its sampler while retaining the previously installed callback owner.
    void WiiRuntimeTexture::BindNativeTexture(std::uint8_t map) {
        if (!NativeTextureObjectInitialized || map >= 8) {
            throw new InvalidOperationException("Wii texture binding requires an initialized texture and a valid map.");
        }
        if (UsesNativePayload) {
            // Wii materials request repeat wrapping, but libogc GX_InitTexObjCI documents repeat
            // and mirror only for power-of-two axes. Native descriptors use logical dimensions.
            const u8 wrapS = (NativeLayout.Width & (NativeLayout.Width - 1)) == 0
                ? GX_GetTexObjWrapS(&NativeTextureObject) : GX_CLAMP;
            const u8 wrapT = (NativeLayout.Height & (NativeLayout.Height - 1)) == 0
                ? GX_GetTexObjWrapT(&NativeTextureObject) : GX_CLAMP;
            GX_InitTexObjWrapMode(&NativeTextureObject, wrapS, wrapT);
        }
        if (NativePaletteData != nullptr) {
            WiiNativeTlutBinding binding;
            if (!TryBuildNativeTlutBinding(NativeLayout, map, binding)) {
                throw new InvalidOperationException("Wii CI14 palettes require one of the four independent large TLUT map slots.");
            }
            if (!NativeTlutCallbackInstalled) {
                PreviousTlutCallback = GX_SetTlutRegionCallback(ResolveNativeTlutRegion);
                NativeTlutCallbackInstalled = true;
            }
            GXTlutRegion* region = ResolveNativeTlutRegion(binding.Name);
            GX_InitTlutRegion(region, binding.TmemAddress, GX_TLUT_16);
            // libogc df5c999 gx.c takes a u8 size and masks ten count bits. The hardware BP 0x65
            // count is eleven bits (Dolphin BPMemory.h BPU_LoadTlutInfo), so 16,384 entries need
            // this complete precompiled register word. Count reflects actual bytes, not slot capacity.
            region->val[0] = binding.LoadRegister;
            GX_InitTexObjTlut(&NativeTextureObject, binding.Name);
            GX_LoadTlut(&NativePaletteObject, binding.Name);
        }
        GX_LoadTexObj(&NativeTextureObject, map);
    }

    /// Updates a direct color region, preserving native layout and replicated edge texels.
    void WiiRuntimeTexture::UpdateRegionFromRgba8(int32_t x, int32_t y, int32_t width, int32_t height, Array<uint8_t>* rgba8, int32_t sourceRowPitch) {
        if (rgba8 == nullptr || rgba8->Data == nullptr) {
            throw new ArgumentNullException("rgba8");
        } else if (!NativeTextureObjectInitialized || NativeTextureData == nullptr) {
            throw new InvalidOperationException("Wii texture region updates require initialized native texture memory.");
        } else if (x < 0 || y < 0 || width <= 0 || height <= 0) {
            throw new ArgumentOutOfRangeException("texture region");
        }


        const int32_t textureWidth = this->get_Width();
        const int32_t textureHeight = this->get_Height();
        if (textureWidth <= 0
            || textureHeight <= 0
            || static_cast<int64_t>(x) + width > textureWidth
            || static_cast<int64_t>(y) + height > textureHeight) {
            throw new ArgumentOutOfRangeException("texture region", "Texture region exceeds the destination texture bounds.");
        }

        if (width > (INT32_MAX / 4)) {
            throw new ArgumentOutOfRangeException("width", "Texture region row size is too large.");
        }

        const int32_t requiredRowBytes = width * 4;
        if (sourceRowPitch < requiredRowBytes || (sourceRowPitch % 4) != 0) {
            throw new ArgumentOutOfRangeException("sourceRowPitch", "Source row pitch is smaller than the requested RGBA8 row.");
        }

        const int64_t requiredSourceBytes = static_cast<int64_t>(sourceRowPitch) * (height - 1) + requiredRowBytes;
        if (requiredSourceBytes > rgba8->Length) {
            throw new InvalidOperationException("Source buffer is shorter than the requested Wii texture region.");
        }

        if (UsesNativePayload && NativeLayout.NativeFormat > 6) {
            throw new InvalidOperationException("Wii RGBA updates cannot alter immutable indexed, CMPR or depth-view native textures.");
        }
        // Command building for a later camera can add glyphs while the preceding camera still
        // samples this atlas. Complete those GX reads before modifying their retained memory.
        GX_DrawDone();
        if (UsesNativePayload) {
            if (!PatchNativeTextureRegion(NativeLayout, static_cast<std::uint8_t*>(NativeTextureData), NativeTextureDataSize,
                x, y, width, height, rgba8->Data, rgba8->Length, sourceRowPitch)) {
                throw new InvalidOperationException("Wii RGBA updates cannot alter immutable indexed, CMPR or depth-view native textures.");
            }
            DCFlushRange(NativeTextureData, NativeTextureDataSize);
            GX_InvalidateTexAll();
            return;
        }

        const uint32_t paddedWidth = (static_cast<uint32_t>(textureWidth) + 3U) & ~3U;
        const uint32_t paddedHeight = (static_cast<uint32_t>(textureHeight) + 3U) & ~3U;
        const uint32_t blockColumnCount = paddedWidth / 4U;
        const uint32_t regionLeft = static_cast<uint32_t>(x);
        const uint32_t regionTop = static_cast<uint32_t>(y);
        const uint32_t regionRight = regionLeft + static_cast<uint32_t>(width);
        const uint32_t regionBottom = regionTop + static_cast<uint32_t>(height);

        // Padded texels replicate the clamped edge texel exactly as the full-texture encoder does, so a region that
        // reaches the logical right or bottom edge must also rewrite the padding columns and rows that mirror it.
        const uint32_t firstColumn = regionLeft & ~3U;
        const uint32_t columnEnd = (regionRight >= static_cast<uint32_t>(textureWidth)) ? paddedWidth : ((regionRight + 3U) & ~3U);
        const uint32_t firstRow = regionTop & ~3U;
        const uint32_t rowEnd = (regionBottom >= static_cast<uint32_t>(textureHeight)) ? paddedHeight : ((regionBottom + 3U) & ~3U);

        auto* destination = static_cast<uint16_t*>(NativeTextureData);
        for (uint32_t destinationY = firstRow; destinationY < rowEnd; destinationY++) {
            const uint32_t sampleY = std::min(destinationY, static_cast<uint32_t>(textureHeight) - 1U);
            if (sampleY < regionTop || sampleY >= regionBottom) {
                continue;
            }

            const std::size_t sourceRowOffset = static_cast<std::size_t>(sampleY - regionTop) * static_cast<std::size_t>(sourceRowPitch);
            for (uint32_t destinationX = firstColumn; destinationX < columnEnd; destinationX++) {
                const uint32_t sampleX = std::min(destinationX, static_cast<uint32_t>(textureWidth) - 1U);
                if (sampleX < regionLeft || sampleX >= regionRight) {
                    continue;
                }

                const std::size_t sourceOffset = sourceRowOffset + static_cast<std::size_t>(sampleX - regionLeft) * 4U;
                const uint8_t red = (*rgba8)[static_cast<int32_t>(sourceOffset + 0U)];
                const uint8_t green = (*rgba8)[static_cast<int32_t>(sourceOffset + 1U)];
                const uint8_t blue = (*rgba8)[static_cast<int32_t>(sourceOffset + 2U)];
                const uint8_t alpha = (*rgba8)[static_cast<int32_t>(sourceOffset + 3U)];
                const std::size_t blockIndex = static_cast<std::size_t>(destinationY >> 2) * static_cast<std::size_t>(blockColumnCount)
                    + static_cast<std::size_t>(destinationX >> 2);
                const std::size_t texelIndex = blockIndex * 16U
                    + static_cast<std::size_t>(destinationY & 3U) * 4U
                    + static_cast<std::size_t>(destinationX & 3U);
                destination[texelIndex] = EncodeRgb5A3Pixel(red, green, blue, alpha);
            }
        }

        // Whole four-row texel blocks are contiguous, so only the touched block rows need to reach main memory.
        const std::size_t blockRowByteCount = static_cast<std::size_t>(blockColumnCount) * 32U;
        const std::size_t flushOffset = static_cast<std::size_t>(firstRow >> 2) * blockRowByteCount;
        const std::size_t flushByteCount = static_cast<std::size_t>((rowEnd - firstRow) >> 2) * blockRowByteCount;
        DCFlushRange(static_cast<uint8_t*>(NativeTextureData) + flushOffset, flushByteCount);
        GX_InvalidateTexAll();
    }

}
