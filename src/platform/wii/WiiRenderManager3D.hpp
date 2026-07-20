#pragma once

#include <cstdint>
#include <vector>

#include <gccore.h>

#include "RenderManager3D.hpp"

class ContentManager;
class MaterialLayout;
class MaterialRenderState;
class ModelAsset;
class PlatformMaterialAsset;
class RendererBackendCapabilityProfile;
class RuntimeMaterial;
class RuntimeModel;
class float4;

namespace helengine::wii {
    class WiiFramePlan;
    class WiiMeshCache;
    class WiiRasterRenderer;
    class WiiRenderManager2D;
    class WiiRuntimeMaterial;
    class WiiRuntimeModel;
    class WiiSceneRenderBridge;

    /// Orchestrates authored runtime model creation, frame extraction, and GX execution for the Wii backend.
    class WiiRenderManager3D : public RenderManager3D {
    public:
        /// Creates the Wii 3D backend and its owned bridge/cache/raster collaborators.
        WiiRenderManager3D();

        /// Releases owned Wii renderer collaborators.
        ~WiiRenderManager3D() override;

        /// Rebuilds one legacy raw material asset path through the cooked platform-owned Wii material contract.
        RuntimeMaterial* BuildMaterialFromRawAsset(ContentManager* assetContentManager, std::string materialAssetPath) override;

        /// Builds a Wii runtime model that keeps authored submesh and geometry arrays alive.
        RuntimeModel* BuildModelFromRaw(ModelAsset* data) override;

        /// Builds a Wii runtime model from one serialized cooked model asset path.
        RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override;

        /// Rebuilds one cooked platform-owned material payload path into the shared runtime material contract used by generated scenes.
        RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource);

        /// Rebuilds one cooked platform-owned material payload into the minimal runtime contract currently consumed by generated scenes.
        RuntimeMaterial* BuildMaterialFromCooked(PlatformMaterialAsset* materialAsset);

        /// Releases one Wii runtime material after the final scene reference is removed.
        void ReleaseMaterial(RuntimeMaterial* material) override;

        /// Releases one Wii runtime model after the final scene reference is removed.
        void ReleaseModel(RuntimeModel* model) override;

        /// Releases any deferred runtime-material and runtime-model deletions after the scene manager reaches a safe transition boundary.
        void FlushReleasedAssets() override;

        /// Extracts the current frame and renders it through GX.
        void Draw() override;

        /// Registers the 2D overlay render manager used by the generated draw path.
        void SetOverlayRenderManager2D(WiiRenderManager2D* renderManager2D);

        /// Registers the physical presented framebuffer size used for GX viewport and scissor setup.
        void SetPresentedFrameSize(uint16_t width, uint16_t height);

        /// Returns the strict backend capability surface exposed by the first Wii tier.
        RendererBackendCapabilityProfile* GetCapabilityProfile() override;

        /// Reports whether this backend has emitted a native scene frame.
        bool HasRenderedScene() const;

        /// Returns whether the current frame resolved one authored camera clear color for presentation.
        bool HasPresentedClearColor() const;

        /// Returns the authored camera clear color resolved for the current presented frame.
        GXColor GetPresentedClearColor() const;

    private:
        /// Shared backend capability object reused across frame extraction calls.
        RendererBackendCapabilityProfile* CapabilityProfile;

        /// Converts generated runtime state into a Wii-local frame plan.
        WiiSceneRenderBridge* SceneRenderBridge;

        /// Validates and reuses authored mesh geometry across frames.
        WiiMeshCache* MeshCache;

        /// Owns the narrow GX raster path for the first Wii 3D milestone.
        WiiRasterRenderer* RasterRenderer;

        /// Stores the 2D render manager whose captured overlay drawables must be completed before frame-boundary scene commits run.
        WiiRenderManager2D* OverlayRenderManager2D;

        /// Tracks whether the current frame resolved one authored camera clear color for presentation.
        bool PresentedClearColorValid;

        /// Stores the authored clear color that should be presented for the current frame when a camera enables it.
        GXColor PresentedClearColor;

        /// Tracks whether the backend has already drawn a real scene frame.
        bool HasRenderedSceneValue;

        /// Tracks the physical framebuffer width used by the raster path.
        uint16_t PresentedFrameWidth;

        /// Tracks the physical framebuffer height used by the raster path.
        uint16_t PresentedFrameHeight;

        /// Counts extracted Wii scene frames for throttled diagnostics.
        uint32_t ExtractedFrameCount;

        /// Runtime materials deferred until the renderer reaches a safe destruction boundary.
        std::vector<RuntimeMaterial*> ReleasedMaterials;

        /// Runtime models deferred until the renderer reaches a safe destruction boundary.
        std::vector<RuntimeModel*> ReleasedModels;

        /// Updates the presented clear color from the active frame-plan camera.
        void UpdatePresentedClearColor(WiiFramePlan* framePlan);

        /// Converts one normalized engine color into the byte GX color contract used by the copy clear path.
        static GXColor ToGxColor(float4 color);

        /// Converts one normalized engine color channel into the byte GX range expected by the Wii renderer.
        static uint8_t ConvertNormalizedColorChannel(float value);

        /// Rebuilds one material render-state instance from the cooked Wii material payload flags.
        static MaterialRenderState* BuildMaterialRenderState(PlatformMaterialAsset* materialAsset);

        /// Resolves one packaged content-relative asset path against the absolute cooked material path that referenced it.
        std::string ResolvePackagedContentAssetPath(const std::string& cookedMaterialAssetPath, const std::string& contentRelativePath);

        /// Loads and attaches one cooked diffuse texture when the path-based Wii cooked-material contract references one.
        void AttachCookedDiffuseTexture(WiiRuntimeMaterial* runtimeMaterial, PlatformMaterialAsset* materialAsset, const std::string& cookedMaterialAssetPath, IContentStreamSource* contentStreamSource);

        /// Releases one transient cooked/raw model asset after the shared runtime model has been rebuilt.
        static void ReleaseTransientModelAsset(ModelAsset* asset);

        /// Releases one owned deserialized cooked model payload attached to a Wii runtime model.
        void ReleaseOwnedSourceModelAsset(WiiRuntimeModel* runtimeModel);
    };
}
