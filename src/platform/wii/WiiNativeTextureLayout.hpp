#pragma once

#include <cstddef>
#include <cstdint>

namespace helengine::wii {
    /// Validated GXN1 metadata describing native tiled bytes and their optional contiguous big-endian TLUT.
    struct WiiNativeTextureLayout {
        /// Public native code, including depth-copy aliases whose sampling code is mapped explicitly.
        std::uint32_t NativeFormat = 0;
        /// Physical four-bit sampler format passed to GX_InitTexObj or GX_InitTexObjCI.
        std::uint32_t SamplerFormat = 0;
        /// Logical dimensions used by GX sampling, independent of tile padding.
        std::uint32_t Width = 0, Height = 0;
        /// Complete-tile dimensions present in the native payload.
        std::uint32_t PaddedWidth = 0, PaddedHeight = 0;
        /// Tile dimensions and byte count for one native storage block.
        std::uint32_t BlockWidth = 0, BlockHeight = 0, BlockBytes = 0;
        /// Exact native tiled texel bytes after the forty-eight-byte header.
        std::uint32_t PixelBytes = 0;
        /// Native TLUT encoding, or UINT32_MAX for direct textures.
        std::uint32_t TlutFormat = UINT32_MAX;
        /// Number of TLUT entries and exact big-endian entry bytes, both zero for direct textures.
        std::uint32_t PaletteEntries = 0, PaletteBytes = 0;
        /// Shared-engine alpha enum stored in the native header and checked against its format policy.
        std::uint32_t AlphaPrecision = 0;
    };

    /// Describes one nonoverlapping hardware TLUT slot and the full eleven-bit load count unavailable through libogc's u8 initializer.
    struct WiiNativeTlutBinding {
        /// Libogc small or large TLUT name used by both GX_LoadTlut and GX_InitTexObjCI.
        std::uint32_t Name = 0;
        /// TMEM address reserved for this map's palette.
        std::uint32_t TmemAddress = 0;
        /// BP register 0x65 containing address and the exact number of sixteen-entry lines.
        std::uint32_t LoadRegister = 0;
    };

    /// Builds canonical format, tile, alpha and TLUT metadata for one bounded single-level GX texture.
    bool TryBuildNativeTextureLayout(std::uint32_t format, std::uint32_t width, std::uint32_t height,
        std::uint32_t tlutFormat, std::uint32_t paletteEntries, std::uint32_t alpha, WiiNativeTextureLayout& layout);
    /// Parses the exact GXN1 little-endian header and validates native bytes, palette lengths and every padded color index.
    bool TryParseNativeTexturePayload(const std::uint8_t* colors, std::size_t colorBytes,
        const std::uint8_t* palette, std::size_t paletteBytes, WiiNativeTextureLayout& layout);
    /// Assigns independent small slots for maps zero through seven or independent CI14 slots for maps zero through three.
    bool TryBuildNativeTlutBinding(const WiiNativeTextureLayout& layout, std::uint32_t map,
        WiiNativeTlutBinding& binding);
    /// Re-encodes an RGBA rectangle into mutable direct color tiles, including replicated edge padding; indexed, compressed and depth views stay immutable.
    bool PatchNativeTextureRegion(const WiiNativeTextureLayout& layout, std::uint8_t* pixels, std::size_t pixelBytes,
        std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height,
        const std::uint8_t* rgba, std::size_t rgbaBytes, std::int32_t sourceRowPitch);
}
