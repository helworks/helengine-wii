#pragma once

#include "RenderManager3D.hpp"

#include <gccore.h>

class ContentManager;
class ModelAsset;
class RendererBackendCapabilityProfile;
class RuntimeMaterial;
class RuntimeModel;
class float4;

namespace helengine::wii {
    class WiiRenderManager2D;

    /// Provides the minimal Wii 3D render bridge required for generated core update/draw boot validation.
    class WiiRenderManager3D : public RenderManager3D {
    public:
        /// Creates the Wii 3D render bridge.
        WiiRenderManager3D();

        /// Releases the Wii 3D render bridge.
        ~WiiRenderManager3D() override;

        /// Fails because material creation is outside the generated-core boot slice.
        RuntimeMaterial* BuildMaterialFromRawAsset(ContentManager* assetContentManager, std::string contentRootPath, std::string materialAssetPath) override;

        /// Fails because raw model creation is outside the generated-core boot slice.
        RuntimeModel* BuildModelFromRaw(ModelAsset* data) override;

        /// Fails because cooked model loading is outside the generated-core boot slice.
        RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath) override;

        /// Releases one runtime material.
        void ReleaseMaterial(RuntimeMaterial* material) override;

        /// Releases one runtime model.
        void ReleaseModel(RuntimeModel* model) override;

        /// Clears deferred release state after an engine frame.
        void FlushReleasedAssets() override;

        /// Runs the generated draw path without native scene rasterization.
        void Draw() override;

        /// Registers the 2D overlay render manager used by the generated draw path.
        void SetOverlayRenderManager2D(WiiRenderManager2D* renderManager2D);

        /// Returns the strict backend capability surface exposed by the first Wii tier.
        RendererBackendCapabilityProfile* GetCapabilityProfile() override;

        /// Reports whether this backend has emitted a native scene frame.
        bool HasRenderedScene() const;

        /// Returns whether the current frame resolved one authored camera clear color for presentation.
        bool HasPresentedClearColor() const;

        /// Returns the authored camera clear color resolved for the current presented frame.
        GXColor GetPresentedClearColor() const;

    private:
        /// Refreshes the current presented clear color from the active authored camera set.
        void UpdatePresentedClearColorFromActiveCameras();

        /// Converts one normalized engine color into the byte GX color contract used by the copy clear path.
        static GXColor ToGxColor(float4 color);

        /// Converts one normalized engine color channel into the byte GX range expected by the Wii renderer.
        static uint8_t ConvertNormalizedColorChannel(float value);

        /// Shared backend capability object reused across frame calls.
        RendererBackendCapabilityProfile* CapabilityProfile;

        /// Stores the 2D render manager whose captured overlay drawables are part of the draw boundary.
        WiiRenderManager2D* OverlayRenderManager2D;

        /// Stores the authored clear color that should be presented for the current frame when a camera enables it.
        GXColor PresentedClearColor;

        /// Tracks whether the current frame resolved one authored camera clear color for presentation.
        bool PresentedClearColorValid;
    };
}
