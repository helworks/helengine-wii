#include "platform/wii/WiiRenderManager3D.hpp"

#include "RendererBackendCapabilityProfile.hpp"
#include "platform/wii/WiiRenderManager2D.hpp"
#include "runtime/native_exceptions.hpp"

namespace helengine::wii {
    /// Creates the Wii 3D render bridge.
    WiiRenderManager3D::WiiRenderManager3D()
        : RenderManager3D()
        , CapabilityProfile(new RendererBackendCapabilityProfile())
        , OverlayRenderManager2D(nullptr) {
    }

    /// Releases the Wii 3D render bridge.
    WiiRenderManager3D::~WiiRenderManager3D() {
        delete CapabilityProfile;
    }

    /// Fails because material creation is outside the generated-core boot slice.
    RuntimeMaterial* WiiRenderManager3D::BuildMaterialFromCooked(PlatformMaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw new ArgumentNullException("materialAsset");
        }

        throw new InvalidOperationException("Wii generated-core boot does not support material creation yet.");
    }

    /// Fails because cooked material loading is outside the generated-core boot slice.
    RuntimeMaterial* WiiRenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Wii cooked material path is required.", "cookedAssetPath");
        }

        throw new InvalidOperationException("Wii generated-core boot does not support cooked material loading yet.");
    }

    /// Fails because raw model creation is outside the generated-core boot slice.
    RuntimeModel* WiiRenderManager3D::BuildModelFromRaw(ModelAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        }

        throw new InvalidOperationException("Wii generated-core boot does not support raw model creation yet.");
    }

    /// Fails because cooked model loading is outside the generated-core boot slice.
    RuntimeModel* WiiRenderManager3D::BuildModelFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Wii cooked model path is required.", "cookedAssetPath");
        }

        throw new InvalidOperationException("Wii generated-core boot does not support cooked model loading yet.");
    }

    /// Releases one runtime material.
    void WiiRenderManager3D::ReleaseMaterial(RuntimeMaterial* material) {
        if (material == nullptr) {
            throw new ArgumentNullException("material");
        }
    }

    /// Releases one runtime model.
    void WiiRenderManager3D::ReleaseModel(RuntimeModel* model) {
        if (model == nullptr) {
            throw new ArgumentNullException("model");
        }
    }

    /// Clears deferred release state after an engine frame.
    void WiiRenderManager3D::FlushReleasedAssets() {
    }

    /// Runs the generated draw path without native scene rasterization.
    void WiiRenderManager3D::Draw() {
        if (OverlayRenderManager2D == nullptr) {
            throw new InvalidOperationException("WiiRenderManager3D requires an overlay WiiRenderManager2D before Draw().");
        }

        OverlayRenderManager2D->Draw();
    }

    /// Registers the 2D overlay render manager used by the generated draw path.
    void WiiRenderManager3D::SetOverlayRenderManager2D(WiiRenderManager2D* renderManager2D) {
        if (renderManager2D == nullptr) {
            throw new ArgumentNullException("renderManager2D");
        }

        OverlayRenderManager2D = renderManager2D;
    }

    /// Returns the strict backend capability surface exposed by the first Wii tier.
    RendererBackendCapabilityProfile* WiiRenderManager3D::GetCapabilityProfile() {
        return CapabilityProfile;
    }

    /// Reports whether this backend has emitted a native scene frame.
    bool WiiRenderManager3D::HasRenderedScene() const {
        return false;
    }
}
