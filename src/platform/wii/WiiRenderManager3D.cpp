#include "platform/wii/WiiRenderManager3D.hpp"

#include <algorithm>

#include "CameraClearSettings.hpp"
#include "CameraComponent.hpp"
#include "Core.hpp"
#include "Entity.hpp"
#include "ICamera.hpp"
#include "ObjectManager.hpp"
#include "RendererBackendCapabilityProfile.hpp"
#include "platform/wii/WiiRenderManager2D.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/native_exceptions.hpp"

namespace helengine::wii {
    /// Creates the Wii 3D render bridge.
    WiiRenderManager3D::WiiRenderManager3D()
        : RenderManager3D()
        , CapabilityProfile(new RendererBackendCapabilityProfile(true, false, false, false, 0, 0))
        , OverlayRenderManager2D(nullptr)
        , PresentedClearColor { 0x00, 0x00, 0x00, 0xFF }
        , PresentedClearColorValid(false) {
    }

    /// Releases the Wii 3D render bridge.
    WiiRenderManager3D::~WiiRenderManager3D() {
        delete CapabilityProfile;
    }

    /// Fails because material creation is outside the generated-core boot slice.
    RuntimeMaterial* WiiRenderManager3D::BuildMaterialFromRawAsset(ContentManager* assetContentManager, std::string contentRootPath, std::string materialAssetPath) {
        if (assetContentManager == nullptr) {
            throw new ArgumentNullException("assetContentManager");
        }

        if (contentRootPath.empty()) {
            throw new ArgumentException("Wii content root path is required.", "contentRootPath");
        }

        if (materialAssetPath.empty()) {
            throw new ArgumentException("Wii material asset path is required.", "materialAssetPath");
        }

        throw new InvalidOperationException("Wii generated-core boot does not support material creation yet.");
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

        UpdatePresentedClearColorFromActiveCameras();
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

    /// Returns whether the current frame resolved one authored camera clear color for presentation.
    bool WiiRenderManager3D::HasPresentedClearColor() const {
        return PresentedClearColorValid;
    }

    /// Returns the authored camera clear color resolved for the current presented frame.
    GXColor WiiRenderManager3D::GetPresentedClearColor() const {
        return PresentedClearColor;
    }

    /// Refreshes the current presented clear color from the active authored camera set.
    void WiiRenderManager3D::UpdatePresentedClearColorFromActiveCameras() {
        PresentedClearColorValid = false;

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

            CameraClearSettings clearSettings = camera->get_ClearSettings();
            if (!clearSettings.get_ClearColorEnabled()) {
                continue;
            }

            PresentedClearColor = ToGxColor(clearSettings.get_ClearColor());
            PresentedClearColorValid = true;
        }
    }

    /// Converts one normalized engine color into the byte GX color contract used by the copy clear path.
    GXColor WiiRenderManager3D::ToGxColor(float4 color) {
        return GXColor {
            ConvertNormalizedColorChannel(color.X),
            ConvertNormalizedColorChannel(color.Y),
            ConvertNormalizedColorChannel(color.Z),
            ConvertNormalizedColorChannel(color.W)
        };
    }

    /// Converts one normalized engine color channel into the byte GX range expected by the Wii renderer.
    uint8_t WiiRenderManager3D::ConvertNormalizedColorChannel(float value) {
        const double clampedValue = std::clamp(static_cast<double>(value), 0.0, 1.0);
        return static_cast<uint8_t>((clampedValue * 255.0) + 0.5);
    }
}
