#include "platform/wii/WiiFailureScreen.hpp"

namespace helengine::wii {
    namespace {
        constexpr uint32_t ForegroundColor = 0xEB80EB80U;
        constexpr uint32_t BackgroundColor = 0x10801080U;
        constexpr uint32_t HorizontalScale = 3U;
        constexpr uint32_t VerticalScale = 4U;
        constexpr uint32_t GlyphWidth = 3U;
        constexpr uint32_t GlyphHeight = 5U;
        constexpr uint32_t GlyphGap = 2U;
        constexpr uint32_t Margin = 8U;
        constexpr uint32_t DigitCount = 4U;
    }

    /// Returns the diagnostic background color assigned to the supplied persistent failure code.
    GXColor WiiFailureScreen::ResolveBackgroundColor(WiiFailureCode code) {
        switch (code) {
            case WiiFailureCode::CoreDraw:
                return GXColor { 0xC0, 0x40, 0x00, 0xFF };
            case WiiFailureCode::SceneCommit:
                return GXColor { 0x60, 0x20, 0x90, 0xFF };
            case WiiFailureCode::OverlayCapture:
                return GXColor { 0x00, 0x30, 0xA0, 0xFF };
            case WiiFailureCode::FramePlanBuild:
                return GXColor { 0x00, 0x70, 0x60, 0xFF };
            case WiiFailureCode::RasterSubmission:
                return GXColor { 0xA0, 0x00, 0x60, 0xFF };
            case WiiFailureCode::DrawSetup:
                return GXColor { 0x70, 0x30, 0x10, 0xFF };
            case WiiFailureCode::FrameBoundaryBookkeeping:
                return GXColor { 0x60, 0x60, 0x00, 0xFF };
            case WiiFailureCode::RenderManagerBoundary:
                return GXColor { 0x10, 0x20, 0x70, 0xFF };
            case WiiFailureCode::PostRenderMetrics:
                return GXColor { 0x00, 0x70, 0x90, 0xFF };
            case WiiFailureCode::FpsRenderFrame:
                return GXColor { 0x00, 0x70, 0x20, 0xFF };
            case WiiFailureCode::DebugRenderFrame:
                return GXColor { 0x70, 0x20, 0xA0, 0xFF };
            case WiiFailureCode::SceneOperationCommit:
                return GXColor { 0x70, 0x10, 0x10, 0xFF };
            case WiiFailureCode::ScenePathResolution:
                return GXColor { 0x80, 0x28, 0x08, 0xFF };
            case WiiFailureCode::SceneRecordLookup:
                return GXColor { 0x78, 0x48, 0x00, 0xFF };
            case WiiFailureCode::SceneContentLoad:
                return GXColor { 0x68, 0x60, 0x00, 0xFF };
            case WiiFailureCode::SceneMaterializationCall:
                return GXColor { 0x38, 0x68, 0x00, 0xFF };
            case WiiFailureCode::SceneMaterializationReturn:
                return GXColor { 0x08, 0x68, 0x18, 0xFF };
            case WiiFailureCode::SceneRecordTrack:
                return GXColor { 0x00, 0x68, 0x48, 0xFF };
            case WiiFailureCode::SceneRecordListInsertion:
                return GXColor { 0x00, 0x58, 0x70, 0xFF };
            case WiiFailureCode::SceneRecordDictionaryInsertion:
                return GXColor { 0x00, 0x38, 0x80, 0xFF };
            case WiiFailureCode::SceneLoadedEvent:
                return GXColor { 0x28, 0x28, 0x80, 0xFF };
            case WiiFailureCode::SceneLoadedEventReturned:
                return GXColor { 0x50, 0x20, 0x80, 0xFF };
            case WiiFailureCode::SceneLoadedEventArgsRelease:
                return GXColor { 0x78, 0x10, 0x70, 0xFF };
            case WiiFailureCode::SceneLoadedEventArgsReleased:
                return GXColor { 0x80, 0x18, 0x48, 0xFF };
            case WiiFailureCode::SceneLoadCleanup:
                return GXColor { 0x58, 0x38, 0x48, 0xFF };
            case WiiFailureCode::SceneMaterializationBegin:
                return GXColor { 0x50, 0x18, 0x08, 0xFF };
            case WiiFailureCode::SceneEntityConstruction:
                return GXColor { 0x60, 0x38, 0x08, 0xFF };
            case WiiFailureCode::SceneComponentDeserialization:
                return GXColor { 0x58, 0x58, 0x10, 0xFF };
            case WiiFailureCode::SceneChildEntity:
                return GXColor { 0x20, 0x58, 0x20, 0xFF };
            case WiiFailureCode::SceneEntityCompletion:
                return GXColor { 0x10, 0x50, 0x58, 0xFF };
            case WiiFailureCode::SceneMaterializationCompleted:
                return GXColor { 0x20, 0x30, 0x68, 0xFF };
            case WiiFailureCode::CameraComponentDeserialization:
                return GXColor { 0x68, 0x18, 0x30, 0xFF };
            case WiiFailureCode::RoundedRectComponentDeserialization:
                return GXColor { 0x68, 0x30, 0x18, 0xFF };
            case WiiFailureCode::ViewportComponentDeserialization:
                return GXColor { 0x48, 0x58, 0x08, 0xFF };
            case WiiFailureCode::ReferenceCanvasFitComponentDeserialization:
                return GXColor { 0x18, 0x58, 0x38, 0xFF };
            case WiiFailureCode::SplashComponentDeserialization:
                return GXColor { 0x18, 0x40, 0x68, 0xFF };
            case WiiFailureCode::SpriteComponentDeserialization:
                return GXColor { 0x48, 0x18, 0x68, 0xFF };
            case WiiFailureCode::OwnedTextureRegistration:
                return GXColor { 0x48, 0x20, 0x10, 0xFF };
            case WiiFailureCode::OwnedFontRegistration:
                return GXColor { 0x58, 0x40, 0x10, 0xFF };
            case WiiFailureCode::OwnedAudioRegistration:
                return GXColor { 0x38, 0x58, 0x18, 0xFF };
            case WiiFailureCode::OwnedModelRegistration:
                return GXColor { 0x10, 0x58, 0x50, 0xFF };
            case WiiFailureCode::OwnedMaterialRegistration:
                return GXColor { 0x18, 0x38, 0x70, 0xFF };
            case WiiFailureCode::OwnedAssetRegistrationCompleted:
                return GXColor { 0x50, 0x18, 0x58, 0xFF };
            default:
                return GXColor { 0xFF, 0x00, 0x00, 0xFF };
        }
    }

    /// Writes the supplied code into both external framebuffers without using GX drawing, fonts, or assets.
    void WiiFailureScreen::WriteCode(const GXRModeObj* renderMode, void* const frameBuffers[2], WiiFailureCode code) {
        if (renderMode == nullptr || frameBuffers == nullptr || frameBuffers[0] == nullptr || frameBuffers[1] == nullptr) {
            return;
        }

        const uint32_t frameBufferWordWidth = static_cast<uint32_t>(renderMode->fbWidth) / 2U;
        const uint32_t frameBufferHeight = static_cast<uint32_t>(renderMode->xfbHeight);
        const uint32_t overlayWidth = (DigitCount * GlyphWidth * HorizontalScale) + ((DigitCount - 1U) * GlyphGap);
        const uint32_t overlayHeight = GlyphHeight * VerticalScale;
        if (frameBufferWordWidth <= Margin + overlayWidth || frameBufferHeight <= Margin + overlayHeight) {
            return;
        }

        const uint16_t numericCode = static_cast<uint16_t>(code);
        for (uint32_t frameBufferIndex = 0U; frameBufferIndex < 2U; frameBufferIndex++) {
            volatile uint32_t* const frameBufferWords = static_cast<volatile uint32_t*>(frameBuffers[frameBufferIndex]);
            for (uint32_t y = 0U; y < overlayHeight; y++) {
                volatile uint32_t* const row = frameBufferWords + ((Margin + y) * frameBufferWordWidth) + Margin;
                for (uint32_t x = 0U; x < overlayWidth; x++) {
                    row[x] = BackgroundColor;
                }
            }

            for (uint32_t digitIndex = 0U; digitIndex < DigitCount; digitIndex++) {
                const uint32_t shift = (DigitCount - 1U - digitIndex) * 4U;
                const uint8_t hexadecimalDigit = static_cast<uint8_t>((numericCode >> shift) & 0x0FU);
                const uint32_t digitStartX = Margin + (digitIndex * ((GlyphWidth * HorizontalScale) + GlyphGap));
                for (uint32_t glyphRow = 0U; glyphRow < GlyphHeight; glyphRow++) {
                    const uint8_t glyphBits = GetGlyphRow(hexadecimalDigit, static_cast<uint8_t>(glyphRow));
                    for (uint32_t verticalOffset = 0U; verticalOffset < VerticalScale; verticalOffset++) {
                        volatile uint32_t* const row = frameBufferWords + ((Margin + (glyphRow * VerticalScale) + verticalOffset) * frameBufferWordWidth) + digitStartX;
                        for (uint32_t glyphColumn = 0U; glyphColumn < GlyphWidth; glyphColumn++) {
                            const bool foreground = (glyphBits & (1U << (GlyphWidth - 1U - glyphColumn))) != 0U;
                            const uint32_t pixelColor = foreground ? ForegroundColor : BackgroundColor;
                            for (uint32_t horizontalOffset = 0U; horizontalOffset < HorizontalScale; horizontalOffset++) {
                                row[(glyphColumn * HorizontalScale) + horizontalOffset] = pixelColor;
                            }
                        }
                    }
                }
            }
        }
    }

    /// Returns the three-bit pixel pattern for one row of a hexadecimal glyph.
    uint8_t WiiFailureScreen::GetGlyphRow(uint8_t hexadecimalDigit, uint8_t row) {
        static constexpr uint8_t GlyphRows[16][5] = {
            { 0x07U, 0x05U, 0x05U, 0x05U, 0x07U },
            { 0x02U, 0x06U, 0x02U, 0x02U, 0x07U },
            { 0x07U, 0x01U, 0x07U, 0x04U, 0x07U },
            { 0x07U, 0x01U, 0x07U, 0x01U, 0x07U },
            { 0x05U, 0x05U, 0x07U, 0x01U, 0x01U },
            { 0x07U, 0x04U, 0x07U, 0x01U, 0x07U },
            { 0x07U, 0x04U, 0x07U, 0x05U, 0x07U },
            { 0x07U, 0x01U, 0x02U, 0x02U, 0x02U },
            { 0x07U, 0x05U, 0x07U, 0x05U, 0x07U },
            { 0x07U, 0x05U, 0x07U, 0x01U, 0x07U },
            { 0x02U, 0x05U, 0x07U, 0x05U, 0x05U },
            { 0x06U, 0x05U, 0x06U, 0x05U, 0x06U },
            { 0x07U, 0x04U, 0x04U, 0x04U, 0x07U },
            { 0x06U, 0x05U, 0x05U, 0x05U, 0x06U },
            { 0x07U, 0x04U, 0x07U, 0x04U, 0x07U },
            { 0x07U, 0x04U, 0x07U, 0x04U, 0x04U }
        };
        return hexadecimalDigit < 16U && row < 5U ? GlyphRows[hexadecimalDigit][row] : 0U;
    }
}
