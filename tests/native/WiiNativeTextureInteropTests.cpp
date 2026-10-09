#include "platform/wii/WiiNativeTextureLayout.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    /// Reads an actual managed cooker buffer without interpreting or rewriting its native byte order.
    std::vector<std::uint8_t> ReadBytes(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) throw std::runtime_error("Fixture file could not be read: " + path.string());
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    /// Reads one independent little-endian metadata word emitted by the managed fixture producer.
    std::uint32_t ReadWord(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        if (offset + 4 > bytes.size()) throw std::runtime_error("Metadata is truncated.");
        return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8)
            | (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
    }

    /// Reports a mismatched contract rather than silently accepting a fallback texture representation.
    void Require(bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
    }
}

/// Verifies the native parser against sixty actual builder outputs and three complete fourteen-bit palettes.
int main(int argc, char** argv) {
    try {
        Require(argc == 2, "Supply the managed fixture directory.");
        std::size_t count = 0;
        std::size_t fullPalettes = 0;
        for (const auto& entry : std::filesystem::directory_iterator(argv[1])) {
            if (entry.path().extension() != ".colors") continue;
            auto prefix = entry.path(); prefix.replace_extension();
            const auto colors = ReadBytes(prefix.string() + ".colors");
            const auto palette = ReadBytes(prefix.string() + ".palette");
            const auto metadata = ReadBytes(prefix.string() + ".meta");
            const auto rgba = ReadBytes(prefix.string() + ".rgba");
            Require(metadata.size() == 40, "Unexpected fixture metadata length.");
            helengine::wii::WiiNativeTextureLayout layout;
            Require(helengine::wii::TryParseNativeTexturePayload(colors.data(), colors.size(),
                palette.data(), palette.size(), layout), "Native parser rejected a managed cooked payload.");
            const std::array<std::uint32_t, 10> actual = {layout.Width, layout.Height, layout.NativeFormat,
                layout.SamplerFormat, layout.TlutFormat, layout.PaletteEntries, layout.AlphaPrecision,
                layout.PixelBytes, layout.PaletteBytes, 17};
            for (std::size_t index = 0; index < actual.size(); index++) {
                Require(actual[index] == ReadWord(metadata, index * 4), "Managed/native texture metadata differs.");
            }
            Require(rgba.size() == std::size_t(layout.Width) * layout.Height * 4, "Managed preview dimensions differ.");
            auto damaged = colors;
            damaged[40] = 1;
            Require(!helengine::wii::TryParseNativeTexturePayload(damaged.data(), damaged.size(),
                palette.data(), palette.size(), layout), "Reserved flags were accepted.");
            Require(!helengine::wii::TryParseNativeTexturePayload(colors.data(), colors.size() - 1,
                palette.data(), palette.size(), layout), "Truncated texels were accepted.");
            if (ReadWord(metadata, 20) == 16384) {
                fullPalettes++;
                Require(ReadWord(metadata, 8) == 10, "A full CI14 palette was mislabeled.");
                Require(colors[52] == 0x3f && colors[53] == 0xff, "Last fourteen-bit palette index was truncated.");
                Require(helengine::wii::TryParseNativeTexturePayload(colors.data(), colors.size(),
                    palette.data(), palette.size(), layout), "Full CI14 palette was rejected.");
                helengine::wii::WiiNativeTlutBinding binding;
                Require(helengine::wii::TryBuildNativeTlutBinding(layout, 0, binding), "Full CI14 TLUT binding was rejected.");
                Require(((binding.LoadRegister >> 10) & 0x7ff) == 1024, "Sixteen-thousand-entry TLUT load count was truncated.");
            }
            count++;
        }
        Require(count == 63 && fullPalettes == 3, "Fixture matrix is incomplete.");
        std::cout << count << " actual managed/native fixtures passed, including " << fullPalettes << " complete CI14 palettes.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
