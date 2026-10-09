#include "platform/wii/WiiNativeTextureLayout.hpp"

namespace helengine::wii {
    namespace {
        /// Reads header words independently of the host CPU's endian order.
        std::uint32_t ReadLittleWord(const std::uint8_t* bytes) {
            return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
                | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
        }
        /// Applies only documented native alpha contracts; coupled intensity has no independent alpha plane.
        bool SupportsAlpha(std::uint32_t format, std::uint32_t alpha) {
            if (format == 0) return alpha == 2;
            if (format == 1 || format == 17 || format == 19) return alpha == 3;
            if (format == 2) return alpha <= 2;
            if (format == 3 || format == 6) return alpha <= 3;
            if (format == 4 || format == 22) return alpha == 0;
            if (format == 5) return alpha == 0 || alpha == 1 || alpha == 5;
            if (format == 14) return alpha <= 1;
            return false;
        }
        /// Quantizes coverage using the same native precision contract as the managed GX pixel encoder.
        std::uint32_t QuantizeAlpha(std::uint8_t value, std::uint32_t alpha) {
            if (alpha == 0) return 255;
            if (alpha == 1) return value >= 128 ? 255 : 0;
            if (alpha == 2) return ((std::uint32_t(value) * 15 + 127) / 255) * 17;
            if (alpha == 5) {
                const auto a = (std::uint32_t(value) * 7 + 127) / 255;
                return (a << 5) | (a << 2) | (a >> 1);
            }
            return value;
        }
        /// Writes one direct color texel in native tile order without overwriting its packed neighbour or RGBA plane.
        void EncodeNativePixel(const WiiNativeTextureLayout& layout, std::uint8_t* pixels,
            std::uint32_t x, std::uint32_t y, const std::uint8_t* rgba) {
            const std::size_t tile = ((y / layout.BlockHeight) * (layout.PaddedWidth / layout.BlockWidth) + x / layout.BlockWidth) * layout.BlockBytes;
            const auto pixel = (y % layout.BlockHeight) * layout.BlockWidth + x % layout.BlockWidth;
            const auto intensity = (std::uint32_t(rgba[0]) * 77 + std::uint32_t(rgba[1]) * 150 + std::uint32_t(rgba[2]) * 29 + 128) >> 8;
            const auto alpha = QuantizeAlpha(rgba[3], layout.AlphaPrecision);
            if (layout.NativeFormat == 0) {
                const auto shift = (pixel & 1) == 0 ? 4 : 0;
                auto& target = pixels[tile + pixel / 2];
                target = (target & ~(15u << shift)) | (((intensity * 15 + 127) / 255) << shift);
            } else if (layout.NativeFormat == 1) pixels[tile + pixel] = intensity;
            else if (layout.NativeFormat == 2) pixels[tile + pixel] = ((alpha * 15 + 127) / 255 << 4) | (intensity * 15 + 127) / 255;
            else if (layout.NativeFormat == 3) { pixels[tile + pixel * 2] = alpha; pixels[tile + pixel * 2 + 1] = intensity; }
            else if (layout.NativeFormat == 4 || layout.NativeFormat == 5) {
                const auto r5 = (std::uint32_t(rgba[0]) * 31 + 127) / 255;
                const auto g5 = (std::uint32_t(rgba[1]) * 31 + 127) / 255;
                const auto b5 = (std::uint32_t(rgba[2]) * 31 + 127) / 255;
                const auto word = layout.NativeFormat == 4 ? (r5 << 11) | (((std::uint32_t(rgba[1]) * 63 + 127) / 255) << 5) | b5
                    : alpha == 255 ? 0x8000 | (r5 << 10) | (g5 << 5) | b5
                    : (((alpha * 7 + 127) / 255) << 12) | (((std::uint32_t(rgba[0]) * 15 + 127) / 255) << 8)
                        | (((std::uint32_t(rgba[1]) * 15 + 127) / 255) << 4) | (std::uint32_t(rgba[2]) * 15 + 127) / 255;
                pixels[tile + pixel * 2] = word >> 8; pixels[tile + pixel * 2 + 1] = word;
            } else {
                pixels[tile + pixel * 2] = alpha; pixels[tile + pixel * 2 + 1] = rgba[0];
                pixels[tile + 32 + pixel * 2] = rgba[1]; pixels[tile + 33 + pixel * 2] = rgba[2];
            }
        }
    }

    bool TryBuildNativeTextureLayout(std::uint32_t format, std::uint32_t width, std::uint32_t height,
        std::uint32_t tlutFormat, std::uint32_t paletteEntries, std::uint32_t alpha, WiiNativeTextureLayout& layout) {
        layout = {};
        if (width == 0 || height == 0 || width > 1024 || height > 1024) return false;
        WiiNativeTextureLayout result;
        result.NativeFormat = format;
        result.SamplerFormat = format == 17 ? 1 : format == 19 ? 3 : format == 22 ? 6 : format;
        const auto sampler = result.SamplerFormat;
        const bool indexed = sampler == 8 || sampler == 9 || sampler == 10;
        if (indexed) {
            const std::uint32_t maximum = sampler == 8 ? 16 : sampler == 9 ? 256 : 16384;
            if (tlutFormat > 2 || paletteEntries == 0 || paletteEntries > maximum || (paletteEntries & 15) != 0) return false;
            if (!SupportsAlpha(tlutFormat == 0 ? 3 : tlutFormat == 1 ? 4 : 5, alpha)) return false;
        } else if (tlutFormat != UINT32_MAX || paletteEntries != 0 || !SupportsAlpha(format, alpha)) return false;
        switch (sampler) {
            case 0: case 8: case 14: result.BlockWidth = 8; result.BlockHeight = 8; result.BlockBytes = 32; break;
            case 1: case 2: case 9: result.BlockWidth = 8; result.BlockHeight = 4; result.BlockBytes = 32; break;
            case 3: case 4: case 5: case 10: result.BlockWidth = 4; result.BlockHeight = 4; result.BlockBytes = 32; break;
            case 6: result.BlockWidth = 4; result.BlockHeight = 4; result.BlockBytes = 64; break;
            default: return false;
        }
        result.Width = width; result.Height = height;
        result.PaddedWidth = ((width + result.BlockWidth - 1) / result.BlockWidth) * result.BlockWidth;
        result.PaddedHeight = ((height + result.BlockHeight - 1) / result.BlockHeight) * result.BlockHeight;
        result.PixelBytes = (result.PaddedWidth / result.BlockWidth) * (result.PaddedHeight / result.BlockHeight) * result.BlockBytes;
        result.TlutFormat = tlutFormat; result.PaletteEntries = paletteEntries;
        result.PaletteBytes = paletteEntries * 2; result.AlphaPrecision = alpha;
        layout = result;
        return true;
    }

    bool TryParseNativeTexturePayload(const std::uint8_t* colors, std::size_t colorBytes,
        const std::uint8_t* palette, std::size_t paletteBytes, WiiNativeTextureLayout& layout) {
        layout = {};
        if (colors == nullptr || colorBytes < 48 || ReadLittleWord(colors) != 0x314e5847 || ReadLittleWord(colors + 4) != 1
            || ReadLittleWord(colors + 40) != 0 || ReadLittleWord(colors + 44) != 0) return false;
        WiiNativeTextureLayout result;
        if (!TryBuildNativeTextureLayout(ReadLittleWord(colors + 8), ReadLittleWord(colors + 12), ReadLittleWord(colors + 16),
            ReadLittleWord(colors + 24), ReadLittleWord(colors + 28), ReadLittleWord(colors + 36), result)
            || ReadLittleWord(colors + 20) != result.PixelBytes || ReadLittleWord(colors + 32) != result.PaletteBytes
            || colorBytes != std::size_t(48) + result.PixelBytes || paletteBytes != result.PaletteBytes
            || (paletteBytes != 0 && palette == nullptr)) return false;
        const auto* pixels = colors + 48;
        if (result.PaletteEntries != 0) {
            for (std::size_t offset = 0; offset < result.PixelBytes; offset++) {
                if (result.SamplerFormat == 8) {
                    if ((pixels[offset] >> 4) >= result.PaletteEntries || (pixels[offset] & 15) >= result.PaletteEntries) return false;
                } else if (result.SamplerFormat == 9) {
                    if (pixels[offset] >= result.PaletteEntries) return false;
                } else {
                    const std::uint32_t index = (std::uint32_t(pixels[offset]) << 8) | pixels[offset + 1];
                    if ((index & 0xc000) != 0 || index >= result.PaletteEntries) return false;
                    offset++;
                }
            }
        }
        layout = result;
        return true;
    }

    bool TryBuildNativeTlutBinding(const WiiNativeTextureLayout& layout, std::uint32_t map,
        WiiNativeTlutBinding& binding) {
        binding = {};
        if (layout.PaletteEntries == 0 || layout.TlutFormat > 2 || map >= 8) return false;
        const bool large = layout.SamplerFormat == 10;
        if (large && map >= 4) return false;
        WiiNativeTlutBinding result;
        result.Name = large ? 16 + map : map;
        result.TmemAddress = large ? 0xe0000 + map * 32768 : 0x80000 + map * 512;
        result.LoadRegister = 0x65000000 | ((result.TmemAddress - 0x80000) >> 9) | ((layout.PaletteEntries / 16) << 10);
        binding = result;
        return true;
    }

    bool PatchNativeTextureRegion(const WiiNativeTextureLayout& layout, std::uint8_t* pixels, std::size_t pixelBytes,
        std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height,
        const std::uint8_t* rgba, std::size_t rgbaBytes, std::int32_t sourceRowPitch) {
        if (layout.NativeFormat > 6 || layout.PaletteEntries != 0 || pixels == nullptr || rgba == nullptr
            || pixelBytes != layout.PixelBytes || x < 0 || y < 0 || width <= 0 || height <= 0 || sourceRowPitch <= 0
            || std::uint64_t(x) + width > layout.Width || std::uint64_t(y) + height > layout.Height
            || std::uint64_t(width) * 4 > std::uint64_t(sourceRowPitch)
            || std::uint64_t(height - 1) * sourceRowPitch + std::uint64_t(width) * 4 > rgbaBytes) return false;
        for (std::uint32_t targetY = y; targetY < layout.PaddedHeight; targetY++) {
            const auto sampleY = targetY < layout.Height ? targetY : layout.Height - 1;
            if (sampleY >= std::uint32_t(y + height)) break;
            for (std::uint32_t targetX = x; targetX < layout.PaddedWidth; targetX++) {
                const auto sampleX = targetX < layout.Width ? targetX : layout.Width - 1;
                if (sampleX >= std::uint32_t(x + width)) break;
                EncodeNativePixel(layout, pixels, targetX, targetY,
                    rgba + std::size_t(sampleY - y) * sourceRowPitch + std::size_t(sampleX - x) * 4);
            }
        }
        return true;
    }
}
