#include "platform/wii/WiiRenderManager2D.hpp"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <vector>

#include <gccore.h>
#include <ogc/system.h>

#include "CameraComponent.hpp"
#include "Core.hpp"
#include "Entity.hpp"
#include "FontAsset.hpp"
#include "ICamera.hpp"
#include "IDrawable2D.hpp"
#include "ITextDrawable2D.hpp"
#include "IRenderQueue2D.hpp"
#include "ObjectManager.hpp"
#include "RenderCommand2DType.hpp"
#include "RenderCommandList2D.hpp"
#include "RenderCommandListBuilder2D.hpp"
#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "IContentStreamSource.hpp"
#include "TextureAsset.hpp"
#include "system/io/file.hpp"
#include "platform/wii/WiiRuntimeTexture.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/native_exceptions.hpp"
#include "runtime/finally.hpp"

namespace helengine::wii {
    /// Creates the Wii 2D render bridge.
    WiiRenderManager2D::WiiRenderManager2D()
        : RenderManager2D()
        , CommandListBuilder(new RenderCommandListBuilder2D())
        , VisitedCameraCount(0)
        , VisitedDrawableCount(0)
        , DidSubmitGlyph(false) {
    }

    /// Releases reusable generated command-list helper state owned by the Wii 2D render bridge.
    WiiRenderManager2D::~WiiRenderManager2D() {
        if (CommandListBuilder != nullptr) {
            CommandListBuilder->Dispose();
            delete CommandListBuilder;
            CommandListBuilder = nullptr;
        }
    }

    /// Rebuilds one packaged texture asset into a Wii-native runtime texture.
    RuntimeTexture* WiiRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        }

        SYS_Report(
            "[Wii] BuildTextureFromRaw width=%u height=%u format=%d colors=%p palette=%p\n",
            data->Width,
            data->Height,
            static_cast<int32_t>(data->ColorFormat),
            data->Colors,
            data->PaletteColors);
        WiiRuntimeTexture* runtimeTexture = new WiiRuntimeTexture();
        runtimeTexture->LoadFromRaw(data);
        SYS_Report("[Wii] BuildTextureFromRaw completed.\n");
        return runtimeTexture;
    }

    /// Rebuilds one platform-owned cooked texture payload into a Wii-native runtime texture.
    RuntimeTexture* WiiRenderManager2D::BuildTextureFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Wii cooked texture path is required.", "cookedAssetPath");
        } else if (contentStreamSource == nullptr) {
            throw new ArgumentNullException("contentStreamSource");
        }

        SYS_Report("[Wii] BuildTextureFromCooked open path=%s\n", cookedAssetPath.c_str());
        Stream* stream = contentStreamSource->OpenRead(cookedAssetPath);
        auto streamGuard = he_cpp_make_scope_exit([&]() {
            if (stream != nullptr) {
                stream->Dispose();
                delete stream;
            }
        });
        SYS_Report("[Wii] BuildTextureFromCooked deserialize begin.\n");
        Asset* asset = AssetSerializer::Deserialize(stream);
        SYS_Report("[Wii] BuildTextureFromCooked deserialize completed asset=%p\n", asset);
        TextureAsset* textureAsset = he_cpp_try_cast<TextureAsset>(asset);
        if (textureAsset == nullptr) {
            delete asset;
            throw new InvalidOperationException("Wii cooked texture payload did not deserialize into a TextureAsset.");
        }

        SYS_Report(
            "[Wii] BuildTextureFromCooked texture width=%u height=%u format=%d colors=%p palette=%p\n",
            textureAsset->Width,
            textureAsset->Height,
            static_cast<int32_t>(textureAsset->ColorFormat),
            textureAsset->Colors,
            textureAsset->PaletteColors);
        auto textureAssetGuard = he_cpp_make_scope_exit([&]() {
            ReleaseTransientTextureAsset(textureAsset);
        });
        return BuildTextureFromRaw(textureAsset);
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

    /// Releases one transient cooked texture asset after the runtime texture has been rebuilt from its payload.
    void WiiRenderManager2D::ReleaseTransientTextureAsset(TextureAsset* asset) {
        if (asset == nullptr) {
            return;
        }

        Array<uint8_t>* colors = asset->Colors;
        Array<uint8_t>* paletteColors = asset->PaletteColors;
        asset->Colors = nullptr;
        asset->PaletteColors = nullptr;
        if (colors != nullptr && colors != Array<uint8_t>::Empty()) {
            delete colors;
        }

        if (paletteColors != nullptr && paletteColors != Array<uint8_t>::Empty()) {
            delete paletteColors;
        }
        delete asset;
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

    /// Builds and executes the shared 2D command list for the current Wii frame.
    void WiiRenderManager2D::RenderCapturedCommands(uint16_t logicalFrameWidth, uint16_t logicalFrameHeight, uint16_t physicalFrameWidth, uint16_t physicalFrameHeight) {
        ConfigureSolidColorPipeline(logicalFrameWidth, logicalFrameHeight, physicalFrameWidth, physicalFrameHeight);
        ResetClipRect(physicalFrameWidth, physicalFrameHeight);

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

            RenderCommandList2D* commandList = CommandListBuilder->Build(camera->get_RenderQueue2D());
            if (commandList == nullptr || commandList->get_Count() <= 0) {
                continue;
            }

            ExecuteCommandList(commandList, logicalFrameWidth, logicalFrameHeight, physicalFrameWidth, physicalFrameHeight);
        }

        ResetClipRect(physicalFrameWidth, physicalFrameHeight);
    }

    /// Executes one shared 2D command list through the Wii GX overlay path.
    void WiiRenderManager2D::ExecuteCommandList(RenderCommandList2D* commandList, uint16_t logicalFrameWidth, uint16_t logicalFrameHeight, uint16_t physicalFrameWidth, uint16_t physicalFrameHeight) {
        if (commandList == nullptr) {
            throw new ArgumentNullException("commandList");
        }

        std::vector<float4> clipRectStack;
        bool texturedPipelineActive = false;
        ConfigureSolidColorPipeline(logicalFrameWidth, logicalFrameHeight, physicalFrameWidth, physicalFrameHeight);
        ResetClipRect(physicalFrameWidth, physicalFrameHeight);

        for (int32_t commandIndex = 0; commandIndex < commandList->get_Count(); commandIndex++) {
            switch (commandList->GetCommandType(commandIndex)) {
                case RenderCommand2DType::ClipPush: {
                    int32_t payloadIndex = commandList->GetClipPushPayloadIndex(commandIndex);
                    float4 clipRect = commandList->GetClipPushRect(payloadIndex);
                    clipRectStack.push_back(clipRect);
                    ApplyClipRect(clipRect, logicalFrameWidth, logicalFrameHeight, physicalFrameWidth, physicalFrameHeight);
                    break;
                }
                case RenderCommand2DType::ClipPop: {
                    if (!clipRectStack.empty()) {
                        clipRectStack.pop_back();
                    }

                    if (clipRectStack.empty()) {
                        ResetClipRect(physicalFrameWidth, physicalFrameHeight);
                    } else {
                        ApplyClipRect(clipRectStack.back(), logicalFrameWidth, logicalFrameHeight, physicalFrameWidth, physicalFrameHeight);
                    }
                    break;
                }
                case RenderCommand2DType::TexturedQuad: {
                    if (!texturedPipelineActive) {
                        ConfigureTextPipeline(logicalFrameWidth, logicalFrameHeight, physicalFrameWidth, physicalFrameHeight);
                        if (clipRectStack.empty()) {
                            ResetClipRect(physicalFrameWidth, physicalFrameHeight);
                        } else {
                            ApplyClipRect(clipRectStack.back(), logicalFrameWidth, logicalFrameHeight, physicalFrameWidth, physicalFrameHeight);
                        }

                        texturedPipelineActive = true;
                    }

                    int32_t payloadIndex = commandList->GetTexturedQuadPayloadIndex(commandIndex);
                    ExecuteTexturedQuadCommand(commandList, payloadIndex, logicalFrameWidth, logicalFrameHeight);
                    break;
                }
                case RenderCommand2DType::GlyphQuad: {
                    if (!texturedPipelineActive) {
                        ConfigureTextPipeline(logicalFrameWidth, logicalFrameHeight, physicalFrameWidth, physicalFrameHeight);
                        if (clipRectStack.empty()) {
                            ResetClipRect(physicalFrameWidth, physicalFrameHeight);
                        } else {
                            ApplyClipRect(clipRectStack.back(), logicalFrameWidth, logicalFrameHeight, physicalFrameWidth, physicalFrameHeight);
                        }

                        texturedPipelineActive = true;
                    }

                    int32_t payloadIndex = commandList->GetGlyphQuadPayloadIndex(commandIndex);
                    ExecuteGlyphQuadCommand(commandList, payloadIndex, logicalFrameWidth, logicalFrameHeight);
                    break;
                }
                case RenderCommand2DType::RoundedRect: {
                    if (texturedPipelineActive) {
                        ConfigureSolidColorPipeline(logicalFrameWidth, logicalFrameHeight, physicalFrameWidth, physicalFrameHeight);
                        if (clipRectStack.empty()) {
                            ResetClipRect(physicalFrameWidth, physicalFrameHeight);
                        } else {
                            ApplyClipRect(clipRectStack.back(), logicalFrameWidth, logicalFrameHeight, physicalFrameWidth, physicalFrameHeight);
                        }

                        texturedPipelineActive = false;
                    }

                    int32_t payloadIndex = commandList->GetRoundedRectPayloadIndex(commandIndex);
                    ExecuteRoundedRectCommand(commandList, payloadIndex, logicalFrameWidth, logicalFrameHeight);
                    break;
                }
                default:
                    throw new InvalidOperationException("Wii 2D rendering received an unsupported command type.");
            }
        }

        ResetClipRect(physicalFrameWidth, physicalFrameHeight);
    }

    /// Executes one textured-quad command from the shared 2D command list.
    void WiiRenderManager2D::ExecuteTexturedQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, uint16_t logicalFrameWidth, uint16_t logicalFrameHeight) {
        if (commandList == nullptr) {
            throw new ArgumentNullException("commandList");
        }

        float4 bounds = commandList->GetTexturedQuadBounds(payloadIndex);
        if (bounds.Z <= 0.0f || bounds.W <= 0.0f) {
            return;
        }

        RuntimeTexture* runtimeTexture = commandList->GetTexturedQuadTexture(payloadIndex);
        WiiRuntimeTexture* texture = static_cast<WiiRuntimeTexture*>(runtimeTexture);
        if (texture == nullptr || !texture->HasNativeTextureObject()) {
            return;
        }

        float4 sourceRect = commandList->GetTexturedQuadSourceRect(payloadIndex);
        byte4 color = commandList->GetTexturedQuadColor(payloadIndex);
        DrawTexturedQuad2D(bounds.X, bounds.Y, bounds.Z, bounds.W, sourceRect, color, texture);
        static_cast<void>(logicalFrameWidth);
        static_cast<void>(logicalFrameHeight);
    }

    /// Executes one glyph-quad command from the shared 2D command list.
    void WiiRenderManager2D::ExecuteGlyphQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, uint16_t logicalFrameWidth, uint16_t logicalFrameHeight) {
        if (commandList == nullptr) {
            throw new ArgumentNullException("commandList");
        }

        float4 bounds = commandList->GetGlyphQuadBounds(payloadIndex);
        if (bounds.Z <= 0.0f || bounds.W <= 0.0f) {
            return;
        }

        RuntimeTexture* runtimeTexture = commandList->GetGlyphQuadTexture(payloadIndex);
        WiiRuntimeTexture* texture = static_cast<WiiRuntimeTexture*>(runtimeTexture);
        if (texture == nullptr || !texture->HasNativeTextureObject()) {
            return;
        }

        float4 sourceRect = commandList->GetGlyphQuadSourceRect(payloadIndex);
        byte4 color = commandList->GetGlyphQuadColor(payloadIndex);
        DrawTexturedQuad2D(bounds.X, bounds.Y, bounds.Z, bounds.W, sourceRect, color, texture);
        DidSubmitGlyph = true;
        static_cast<void>(logicalFrameWidth);
        static_cast<void>(logicalFrameHeight);
    }

    /// Executes one rounded-rectangle command from the shared 2D command list.
    void WiiRenderManager2D::ExecuteRoundedRectCommand(RenderCommandList2D* commandList, int32_t payloadIndex, uint16_t logicalFrameWidth, uint16_t logicalFrameHeight) {
        if (commandList == nullptr) {
            throw new ArgumentNullException("commandList");
        }

        float4 bounds = commandList->GetRoundedRectBounds(payloadIndex);
        if (bounds.Z <= 0.0f || bounds.W <= 0.0f) {
            return;
        }

        float radius = commandList->GetRoundedRectRadius(payloadIndex);
        float borderThickness = std::max(0.0f, commandList->GetRoundedRectBorderThickness(payloadIndex));
        RoundedRectCorners corners = commandList->GetRoundedRectCorners(payloadIndex);
        byte4 fillColor = commandList->GetRoundedRectFillColor(payloadIndex);
        byte4 borderColor = commandList->GetRoundedRectBorderColor(payloadIndex);
        static_cast<void>(radius);
        static_cast<void>(corners);
        static_cast<void>(logicalFrameWidth);
        static_cast<void>(logicalFrameHeight);

        if (borderThickness > 0.0f && borderColor.W > 0U) {
            DrawSolidQuad2D(bounds.X, bounds.Y, bounds.Z, bounds.W, borderColor);
        }

        float inset = std::min(borderThickness, std::min(bounds.Z * 0.5f, bounds.W * 0.5f));
        float innerWidth = bounds.Z - (inset * 2.0f);
        float innerHeight = bounds.W - (inset * 2.0f);
        if (fillColor.W > 0U && innerWidth > 0.0f && innerHeight > 0.0f) {
            DrawSolidQuad2D(bounds.X + inset, bounds.Y + inset, innerWidth, innerHeight, fillColor);
        }
    }

    /// Applies one clip rectangle from the shared 2D command list to the active GX scissor state.
    void WiiRenderManager2D::ApplyClipRect(const float4& clipRect, uint16_t logicalFrameWidth, uint16_t logicalFrameHeight, uint16_t physicalFrameWidth, uint16_t physicalFrameHeight) {
        const float horizontalScale = static_cast<float>(physicalFrameWidth) / static_cast<float>(logicalFrameWidth);
        const float verticalScale = static_cast<float>(physicalFrameHeight) / static_cast<float>(logicalFrameHeight);
        float left = std::max(0.0f, clipRect.X);
        float top = std::max(0.0f, clipRect.Y);
        float right = std::min(static_cast<float>(logicalFrameWidth), clipRect.X + std::max(0.0f, clipRect.Z));
        float bottom = std::min(static_cast<float>(logicalFrameHeight), clipRect.Y + std::max(0.0f, clipRect.W));
        if (right <= left || bottom <= top) {
            GX_SetScissor(0U, 0U, 0U, 0U);
            return;
        }

        uint32_t x = static_cast<uint32_t>(std::floor(left * horizontalScale));
        uint32_t y = static_cast<uint32_t>(std::floor(top * verticalScale));
        uint32_t width = static_cast<uint32_t>(std::ceil(right * horizontalScale) - std::floor(left * horizontalScale));
        uint32_t height = static_cast<uint32_t>(std::ceil(bottom * verticalScale) - std::floor(top * verticalScale));
        GX_SetScissor(x, y, width, height);
    }

    /// Restores the active GX scissor state to the full frame bounds.
    void WiiRenderManager2D::ResetClipRect(uint16_t physicalFrameWidth, uint16_t physicalFrameHeight) {
        GX_SetScissor(0U, 0U, physicalFrameWidth, physicalFrameHeight);
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
    void WiiRenderManager2D::ConfigureSolidColorPipeline(uint16_t logicalFrameWidth, uint16_t logicalFrameHeight, uint16_t physicalFrameWidth, uint16_t physicalFrameHeight) {
        Mtx44 projectionMatrix {};
        Mtx modelViewMatrix {};
        guOrtho(projectionMatrix, 0.0f, static_cast<f32>(logicalFrameHeight), 0.0f, static_cast<f32>(logicalFrameWidth), 0.0f, 1.0f);
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
        GX_SetScissor(0U, 0U, physicalFrameWidth, physicalFrameHeight);
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
    void WiiRenderManager2D::ConfigureTextPipeline(uint16_t logicalFrameWidth, uint16_t logicalFrameHeight, uint16_t physicalFrameWidth, uint16_t physicalFrameHeight) {
        Mtx44 projectionMatrix {};
        Mtx modelViewMatrix {};
        guOrtho(projectionMatrix, 0.0f, static_cast<f32>(logicalFrameHeight), 0.0f, static_cast<f32>(logicalFrameWidth), 0.0f, 1.0f);
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
        GX_SetScissor(0U, 0U, physicalFrameWidth, physicalFrameHeight);
    }

    /// Emits one textured screen-space quad in pixel coordinates for the active glyph pass.
    void WiiRenderManager2D::DrawTexturedQuad2D(float x, float y, float width, float height, const float4& sourceRect, byte4 color, WiiRuntimeTexture* texture) {
        if (texture == nullptr) {
            throw new ArgumentNullException("texture");
        }

        const float logicalWidth = static_cast<float>(std::max(1, texture->get_Width()));
        const float logicalHeight = static_cast<float>(std::max(1, texture->get_Height()));
        const float nativeWidth = static_cast<float>(std::max(1U, texture->GetNativeTextureWidth()));
        const float nativeHeight = static_cast<float>(std::max(1U, texture->GetNativeTextureHeight()));
        const float4 nativeSourceRect(
            sourceRect.X * logicalWidth / nativeWidth,
            sourceRect.Y * logicalHeight / nativeHeight,
            sourceRect.Z * logicalWidth / nativeWidth,
            sourceRect.W * logicalHeight / nativeHeight);

        GX_LoadTexObj(texture->GetNativeTextureObject(), GX_TEXMAP0);
        GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
        GX_Position3f32(x, y, 0.0f);
        GX_Color4u8(color.X, color.Y, color.Z, color.W);
        GX_TexCoord2f32(nativeSourceRect.X, nativeSourceRect.Y);
        GX_Position3f32(x + width, y, 0.0f);
        GX_Color4u8(color.X, color.Y, color.Z, color.W);
        GX_TexCoord2f32(nativeSourceRect.X + nativeSourceRect.Z, nativeSourceRect.Y);
        GX_Position3f32(x + width, y + height, 0.0f);
        GX_Color4u8(color.X, color.Y, color.Z, color.W);
        GX_TexCoord2f32(nativeSourceRect.X + nativeSourceRect.Z, nativeSourceRect.Y + nativeSourceRect.W);
        GX_Position3f32(x, y + height, 0.0f);
        GX_Color4u8(color.X, color.Y, color.Z, color.W);
        GX_TexCoord2f32(nativeSourceRect.X, nativeSourceRect.Y + nativeSourceRect.W);
        GX_End();
    }
}
