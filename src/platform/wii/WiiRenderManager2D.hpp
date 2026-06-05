#pragma once

#include <vector>

#include "IRenderVisitor2D.hpp"
#include "RenderManager2D.hpp"

class RuntimeTexture;
class IDrawable2D;

namespace helengine::wii {
    /// Stores one captured sprite draw request for the current Wii 2D frame.
    struct WiiSpriteDrawCommand {
        /// Pointer to the shared-engine sprite drawable submitted during the current frame.
        ISpriteDrawable2D* Drawable;
    };

    /// Stores one captured text draw request for the current Wii 2D frame.
    struct WiiTextDrawCommand {
        /// Pointer to the shared-engine text drawable submitted during the current frame.
        ITextDrawable2D* Drawable;
    };

    /// Stores one captured rounded-rectangle draw request for the current Wii 2D frame.
    struct WiiRoundedRectDrawCommand {
        /// Pointer to the shared-engine rounded-rectangle drawable submitted during the current frame.
        IRoundedRectDrawable2D* Drawable;
    };

    /// Implements the Wii 2D render bridge by capturing shared-engine draw requests for a later GX overlay pass.
    class WiiRenderManager2D : public RenderManager2D, public IRenderVisitor2D {
    public:
        /// Creates the Wii 2D render bridge.
        WiiRenderManager2D();

        /// Fails because raw texture creation is outside the generated-core boot slice.
        RuntimeTexture* BuildTextureFromRaw(TextureAsset* data) override;

        /// Fails because cooked texture loading is outside the generated-core boot slice.
        RuntimeTexture* BuildTextureFromCooked(std::string cookedAssetPath) override;

        /// Releases one Wii runtime texture.
        void ReleaseTexture(RuntimeTexture* texture) override;

        /// Releases one font asset.
        void ReleaseFont(FontAsset* font) override;

        /// Accepts a sprite draw request without issuing native rendering yet.
        void DrawSprite(ISpriteDrawable2D* sprite) override;

        /// Accepts a text draw request without issuing native rendering yet.
        void DrawText(ITextDrawable2D* text) override;

        /// Accepts a rounded-rectangle draw request without issuing native rendering yet.
        void DrawRoundedRect(IRoundedRectDrawable2D* shape) override;

        /// Walks the active camera 2D queue and lets each drawable submit itself into this frame capture.
        void Draw() override;

        /// Visits one ordered 2D drawable from the active camera queue.
        void Visit(IDrawable2D* drawable) override;

        /// Clears deferred release state after an engine frame.
        void FlushReleasedTextures() override;

        /// Clears previously captured 2D draw requests before the next engine frame begins.
        void BeginFrame();

        /// Returns whether the current frame captured any 2D draw requests.
        bool HasCapturedDrawables() const;

    private:
        /// Captured sprite draw requests in shared-engine render order.
        std::vector<WiiSpriteDrawCommand> SpriteQueue;

        /// Captured text draw requests in shared-engine render order.
        std::vector<WiiTextDrawCommand> TextQueue;

        /// Captured rounded-rectangle draw requests in shared-engine render order.
        std::vector<WiiRoundedRectDrawCommand> RoundedRectQueue;
    };
}
