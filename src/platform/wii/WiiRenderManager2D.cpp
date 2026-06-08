#include "platform/wii/WiiRenderManager2D.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <gccore.h>

#include "CameraComponent.hpp"
#include "Core.hpp"
#include "Entity.hpp"
#include "FontAsset.hpp"
#include "FontInfo.hpp"
#include "ICamera.hpp"
#include "IDrawable2D.hpp"
#include "ITextDrawable2D.hpp"
#include "IRenderQueue2D.hpp"
#include "ObjectManager.hpp"
#include "SceneManager.hpp"
#include "TextLayoutAlignmentUtils.hpp"
#include "TextLayoutUtils.hpp"
#include "TextureAsset.hpp"
#include "platform/wii/WiiRuntimeTexture.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/native_exceptions.hpp"

namespace helengine::wii {
    /// Creates the Wii 2D render bridge.
    WiiRenderManager2D::WiiRenderManager2D()
        : RenderManager2D()
        , VisitedCameraCount(0)
        , VisitedDrawableCount(0)
        , DidSubmitGlyph(false) {
    }

    /// Rebuilds one packaged texture asset into a Wii-native runtime texture.
    RuntimeTexture* WiiRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        }

        WiiRuntimeTexture* runtimeTexture = new WiiRuntimeTexture();
        runtimeTexture->LoadFromRaw(data);
        return runtimeTexture;
    }

    /// Fails because cooked texture loading is outside the generated-core boot slice.
    RuntimeTexture* WiiRenderManager2D::BuildTextureFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Wii cooked texture path is required.", "cookedAssetPath");
        }

        throw new InvalidOperationException("Wii generated-core boot does not support cooked texture loading yet.");
    }

    /// Releases one Wii runtime texture.
    void WiiRenderManager2D::ReleaseTexture(RuntimeTexture* texture) {
        if (texture == nullptr) {
            throw new ArgumentNullException("texture");
        }

        texture->Dispose();
        delete texture;
    }

    /// Releases one font asset.
    void WiiRenderManager2D::ReleaseFont(FontAsset* font) {
        if (font == nullptr) {
            throw new ArgumentNullException("font");
        }

        font->Dispose();
        delete font;
    }

    /// Walks the active camera 2D queue and lets each drawable submit itself into this frame capture.
    void WiiRenderManager2D::Draw() {
        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr) {
            return;
        }

        List<ICamera*>* cameras = core->get_ObjectManager()->get_Cameras();
        for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++) {
            CameraComponent* camera = he_cpp_try_cast<CameraComponent>((*cameras)[cameraIndex]);
            if (camera == nullptr || camera->get_Parent() == nullptr || !camera->get_Parent()->get_IsHierarchyEnabled()) {
                continue;
            }

            VisitedCameraCount++;
            camera->get_RenderQueue2D()->VisitOrdered(this);
        }
    }

    /// Visits one ordered 2D drawable from the active camera queue.
    void WiiRenderManager2D::Visit(IDrawable2D* drawable) {
        if (drawable == nullptr || drawable->get_Parent() == nullptr || !drawable->get_Parent()->get_IsHierarchyEnabled()) {
            return;
        }

        VisitedDrawableCount++;
        drawable->Draw();
    }

    /// Clears deferred release state after an engine frame.
    void WiiRenderManager2D::FlushReleasedTextures() {
    }

    /// Clears previously captured 2D draw requests before the next engine frame begins.
    void WiiRenderManager2D::BeginFrame() {
        SpriteQueue.clear();
        TextQueue.clear();
        RoundedRectQueue.clear();
        VisitedCameraCount = 0;
        VisitedDrawableCount = 0;
        DidSubmitGlyph = false;
    }

    /// Renders all captured text draw requests through the Wii GX overlay path.
    void WiiRenderManager2D::RenderCapturedText(uint16_t frameWidth, uint16_t frameHeight) {
        ConfigureSolidColorPipeline(frameWidth, frameHeight);
        if (VisitedCameraCount <= 0) {
            DrawSolidQuad2D(8.0f, 8.0f, 24.0f, 24.0f, byte4 { 0xFF, 0x00, 0x00, 0xFF });
        } else if (VisitedDrawableCount <= 0) {
            DrawSolidQuad2D(8.0f, 8.0f, 24.0f, 24.0f, byte4 { 0xFF, 0x80, 0x00, 0xFF });
        } else if (TextQueue.empty()) {
            DrawSolidQuad2D(8.0f, 8.0f, 24.0f, 24.0f, byte4 { 0xFF, 0xE0, 0x00, 0xFF });
        } else {
            DrawSolidQuad2D(8.0f, 8.0f, 24.0f, 24.0f, byte4 { 0x00, 0xC0, 0x20, 0xFF });
        }

        if (TextQueue.empty()) {
            return;
        }

        ConfigureTextPipeline(frameWidth, frameHeight);
        for (const WiiTextDrawCommand& command : TextQueue) {
            ITextDrawable2D* drawable = command.Drawable;
            if (drawable == nullptr || drawable->get_Parent() == nullptr || !drawable->get_Parent()->get_IsHierarchyEnabled()) {
                continue;
            }

            FontAsset* font = drawable->get_Font();
            if (font == nullptr || font->get_Texture() == nullptr) {
                continue;
            } else if (font->get_FontInfo() == nullptr) {
                throw new InvalidOperationException("Wii text rendering requires fonts with initialized FontInfo.");
            } else if (font->get_Characters() == nullptr) {
                throw new InvalidOperationException("Wii text rendering requires fonts with initialized glyph dictionaries.");
            }

            WiiRuntimeTexture* texture = static_cast<WiiRuntimeTexture*>(font->get_Texture());
            if (texture == nullptr || !texture->HasNativeTextureObject()) {
                continue;
            }

            std::string content = drawable->get_Text();
            const double fontScale = std::max(static_cast<double>(drawable->get_FontScale()), 0.0001);
            if (drawable->get_WrapText()) {
                content = TextLayoutUtils::WrapText(
                    content,
                    font,
                    std::max(1, static_cast<int32_t>(std::lround(drawable->get_Size().X / fontScale))));
            }

            std::vector<double> lineOffsets;
            std::size_t lineStartIndex = 0U;
            for (std::size_t index = 0U; index <= content.size(); index++) {
                if (index < content.size() && content[index] != '\n') {
                    continue;
                }

                const std::string line = content.substr(lineStartIndex, index - lineStartIndex);
                if (line.empty()) {
                    lineOffsets.push_back(0.0);
                    lineStartIndex = index + 1U;
                    continue;
                }

                const double visibleWidth = TextLayoutAlignmentUtils::MeasureVisibleLineWidth(line, font, fontScale, texture->get_Width());
                lineOffsets.push_back(TextLayoutAlignmentUtils::ResolveHorizontalOffset(drawable->get_Alignment(), drawable->get_Size().X, visibleWidth));
                lineStartIndex = index + 1U;
            }

            if (lineOffsets.empty()) {
                lineOffsets.push_back(0.0);
            }

            const auto position = drawable->get_Parent()->get_Position();
            const double baseX = std::round(position.X);
            const double baseY = std::round(position.Y);
            const double lineHeight = std::max(static_cast<double>(font->get_LineHeight()) * fontScale, 1.0);
            double offsetX = 0.0;
            double offsetY = 0.0;
            std::size_t lineIndex = 0U;
            double lineOriginX = baseX + lineOffsets[lineIndex];

            for (std::size_t index = 0U; index < content.size(); index++) {
                const char character = content[index];
                if (character == '\n') {
                    offsetY += lineHeight;
                    offsetX = 0.0;
                    lineIndex++;
                    lineOriginX = baseX + (lineIndex < lineOffsets.size() ? lineOffsets[lineIndex] : 0.0);
                    continue;
                }

                if (character == ' ') {
                    offsetX += font->get_FontInfo()->get_SpaceWidth() * fontScale;
                    continue;
                }

                FontChar glyph;
                if (!font->get_Characters()->TryGetValue(character, glyph)) {
                    continue;
                }

                const double glyphWidth = glyph.SourceRect.Z * font->get_AtlasWidth() * fontScale;
                const double glyphHeight = glyph.SourceRect.W * font->get_AtlasHeight() * fontScale;
                const double snappedLineOffsetY = std::round(offsetY);
                DrawTexturedQuad2D(
                    static_cast<float>(lineOriginX + offsetX),
                    static_cast<float>(baseY + snappedLineOffsetY + (glyph.OffsetY * fontScale)),
                    static_cast<float>(glyphWidth),
                    static_cast<float>(glyphHeight),
                    glyph.SourceRect,
                    drawable->get_Color(),
                    texture);
                DidSubmitGlyph = true;

                const double advanceWidth = glyph.AdvanceWidth > 0.0f
                    ? glyph.AdvanceWidth * fontScale
                    : glyphWidth;
                offsetX += advanceWidth;
            }
        }

        if (DidSubmitGlyph) {
            ConfigureSolidColorPipeline(frameWidth, frameHeight);
            DrawSolidQuad2D(40.0f, 8.0f, 24.0f, 24.0f, byte4 { 0x00, 0x40, 0xFF, 0xFF });
            ConfigureTextPipeline(frameWidth, frameHeight);
        }

        GX_SetScissor(0, 0, frameWidth, frameHeight);
    }

    /// Returns whether the current frame captured any 2D draw requests.
    bool WiiRenderManager2D::HasCapturedDrawables() const {
        return !SpriteQueue.empty() || !TextQueue.empty() || !RoundedRectQueue.empty();
    }

    /// Returns the number of enabled cameras visited during the current frame capture.
    int32_t WiiRenderManager2D::get_VisitedCameraCount() const {
        return VisitedCameraCount;
    }

    /// Returns the number of 2D drawables visited during the current frame capture.
    int32_t WiiRenderManager2D::get_VisitedDrawableCount() const {
        return VisitedDrawableCount;
    }

    /// Returns the number of queued text drawables captured during the current frame.
    int32_t WiiRenderManager2D::get_QueuedTextCount() const {
        return static_cast<int32_t>(TextQueue.size());
    }

    /// Returns whether the current frame submitted at least one glyph quad.
    bool WiiRenderManager2D::get_DidSubmitGlyph() const {
        return DidSubmitGlyph;
    }

    /// Accepts a sprite draw request without issuing native rendering yet.
    void WiiRenderManager2D::DrawSprite(ISpriteDrawable2D* sprite) {
        if (sprite == nullptr) {
            throw new ArgumentNullException("sprite");
        }

        SpriteQueue.push_back(WiiSpriteDrawCommand { sprite });
    }

    /// Accepts a text draw request without issuing native rendering yet.
    void WiiRenderManager2D::DrawText(ITextDrawable2D* text) {
        if (text == nullptr) {
            throw new ArgumentNullException("text");
        }

        TextQueue.push_back(WiiTextDrawCommand { text });
    }

    /// Accepts a rounded-rectangle draw request without issuing native rendering yet.
    void WiiRenderManager2D::DrawRoundedRect(IRoundedRectDrawable2D* shape) {
        if (shape == nullptr) {
            throw new ArgumentNullException("shape");
        }

        RoundedRectQueue.push_back(WiiRoundedRectDrawCommand { shape });
    }

    /// Configures one native GX pass that draws solid diagnostics without texturing.
    void WiiRenderManager2D::ConfigureSolidColorPipeline(uint16_t frameWidth, uint16_t frameHeight) {
        Mtx44 projectionMatrix {};
        Mtx modelViewMatrix {};
        guOrtho(projectionMatrix, 0.0f, static_cast<f32>(frameHeight), 0.0f, static_cast<f32>(frameWidth), 0.0f, 1.0f);
        guMtxIdentity(modelViewMatrix);

        GX_LoadProjectionMtx(projectionMatrix, GX_ORTHOGRAPHIC);
        GX_LoadPosMtxImm(modelViewMatrix, GX_PNMTX0);
        GX_SetCurrentMtx(GX_PNMTX0);
        GX_ClearVtxDesc();
        GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
        GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
        GX_SetNumChans(1);
        GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
        GX_SetNumTexGens(0);
        GX_SetNumTevStages(1);
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
        GX_SetCullMode(GX_CULL_NONE);
        GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
        GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
        GX_SetZCompLoc(GX_TRUE);
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
        GX_SetColorUpdate(GX_TRUE);
        GX_SetAlphaUpdate(GX_TRUE);
        GX_SetScissor(0, 0, frameWidth, frameHeight);
    }

    /// Emits one solid screen-space quad in pixel coordinates for minimal runtime capture diagnostics.
    void WiiRenderManager2D::DrawSolidQuad2D(float x, float y, float width, float height, byte4 color) {
        GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
        GX_Position3f32(x, y, 0.0f);
        GX_Color4u8(color.X, color.Y, color.Z, color.W);
        GX_Position3f32(x + width, y, 0.0f);
        GX_Color4u8(color.X, color.Y, color.Z, color.W);
        GX_Position3f32(x + width, y + height, 0.0f);
        GX_Color4u8(color.X, color.Y, color.Z, color.W);
        GX_Position3f32(x, y + height, 0.0f);
        GX_Color4u8(color.X, color.Y, color.Z, color.W);
        GX_End();
    }

    /// Configures the native GX state used by the Wii text overlay pass.
    void WiiRenderManager2D::ConfigureTextPipeline(uint16_t frameWidth, uint16_t frameHeight) {
        Mtx44 projectionMatrix {};
        Mtx modelViewMatrix {};
        guOrtho(projectionMatrix, 0.0f, static_cast<f32>(frameHeight), 0.0f, static_cast<f32>(frameWidth), 0.0f, 1.0f);
        guMtxIdentity(modelViewMatrix);

        GX_LoadProjectionMtx(projectionMatrix, GX_ORTHOGRAPHIC);
        GX_LoadPosMtxImm(modelViewMatrix, GX_PNMTX0);
        GX_SetCurrentMtx(GX_PNMTX0);
        GX_ClearVtxDesc();
        GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
        GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
        GX_SetNumChans(1);
        GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
        GX_SetNumTexGens(1);
        GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
        GX_SetNumTevStages(1);
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
        GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
        GX_SetCullMode(GX_CULL_NONE);
        GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
        GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
        GX_SetZCompLoc(GX_TRUE);
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
        GX_SetColorUpdate(GX_TRUE);
        GX_SetAlphaUpdate(GX_TRUE);
        GX_SetScissor(0, 0, frameWidth, frameHeight);
    }

    /// Emits one textured screen-space quad in pixel coordinates for the active glyph pass.
    void WiiRenderManager2D::DrawTexturedQuad2D(float x, float y, float width, float height, const float4& sourceRect, byte4 color, WiiRuntimeTexture* texture) {
        if (texture == nullptr) {
            throw new ArgumentNullException("texture");
        }

        GX_LoadTexObj(texture->GetNativeTextureObject(), GX_TEXMAP0);
        GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
        GX_Position3f32(x, y, 0.0f);
        GX_Color4u8(color.X, color.Y, color.Z, color.W);
        GX_TexCoord2f32(sourceRect.X, sourceRect.Y);
        GX_Position3f32(x + width, y, 0.0f);
        GX_Color4u8(color.X, color.Y, color.Z, color.W);
        GX_TexCoord2f32(sourceRect.X + sourceRect.Z, sourceRect.Y);
        GX_Position3f32(x + width, y + height, 0.0f);
        GX_Color4u8(color.X, color.Y, color.Z, color.W);
        GX_TexCoord2f32(sourceRect.X + sourceRect.Z, sourceRect.Y + sourceRect.W);
        GX_Position3f32(x, y + height, 0.0f);
        GX_Color4u8(color.X, color.Y, color.Z, color.W);
        GX_TexCoord2f32(sourceRect.X, sourceRect.Y + sourceRect.W);
        GX_End();
    }
}
