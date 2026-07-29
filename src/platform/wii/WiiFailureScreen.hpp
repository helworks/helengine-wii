#pragma once

#include <cstdint>

#include <gccore.h>

#include "platform/wii/WiiFailureCode.hpp"

namespace helengine::wii {
    /// Draws a compact hexadecimal failure code directly into Wii external framebuffers.
    class WiiFailureScreen {
    public:
        /// Writes the supplied code into both external framebuffers without using GX drawing, fonts, or assets.
        static void WriteCode(const GXRModeObj* renderMode, void* const frameBuffers[2], WiiFailureCode code);

    private:
        /// Returns the three-bit pixel pattern for one row of a hexadecimal glyph.
        static uint8_t GetGlyphRow(uint8_t hexadecimalDigit, uint8_t row);
    };
}
