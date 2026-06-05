#include "platform/wii/WiiRenderManager2D.hpp"

#include "CameraComponent.hpp"
#include "Core.hpp"
#include "FontAsset.hpp"
#include "ICamera.hpp"
#include "IDrawable2D.hpp"
#include "ObjectManager.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/native_exceptions.hpp"

namespace helengine::wii {
    /// Creates the Wii 2D render bridge.
    WiiRenderManager2D::WiiRenderManager2D()
        : RenderManager2D() {
    }

    /// Fails because raw texture creation is outside the generated-core boot slice.
    RuntimeTexture* WiiRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        }

        throw new InvalidOperationException("Wii generated-core boot does not support raw texture creation yet.");
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

            camera->get_RenderQueue2D()->VisitOrdered(this);
            return;
        }
    }

    /// Visits one ordered 2D drawable from the active camera queue.
    void WiiRenderManager2D::Visit(IDrawable2D* drawable) {
        if (drawable == nullptr || drawable->get_Parent() == nullptr || !drawable->get_Parent()->get_IsHierarchyEnabled()) {
            return;
        }

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
    }

    /// Returns whether the current frame captured any 2D draw requests.
    bool WiiRenderManager2D::HasCapturedDrawables() const {
        return !SpriteQueue.empty() || !TextQueue.empty() || !RoundedRectQueue.empty();
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
}
