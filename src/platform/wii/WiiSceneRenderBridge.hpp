#pragma once

#include "runtime/native_list.hpp"

class CameraComponent;
class IDrawable3D;
class RendererBackendCapabilityProfile;
class float4;
class float4x4;

namespace helengine::wii {
    class WiiFramePlan;

    /// Resolves generated runtime state into the first backend-local Wii frame plan.
    class WiiSceneRenderBridge {
    public:
        /// Builds one strict frame plan for the active camera and visible opaque drawables, or returns null when no active camera is available yet.
        WiiFramePlan* BuildFramePlan(RendererBackendCapabilityProfile* capabilities, int32_t logicalWidth, int32_t logicalHeight, int32_t physicalWidth, int32_t physicalHeight);

        /// Returns whether the current runtime state exposes at least one enabled camera the Wii backend can render.
        bool HasActiveCamera();

    private:
        /// Resolves the first enabled runtime camera the Wii backend is willing to render.
        CameraComponent* ResolveActiveCamera();

        /// Copies the ordered visible 3D queue for one camera into a backend-local list.
        List<IDrawable3D*>* SnapshotVisibleDrawables(CameraComponent* camera);

        /// Resolves the physical GX viewport from the logical viewport and current presented framebuffer dimensions.
        float4 ResolvePhysicalViewport(const float4& logicalViewport, int32_t logicalWidth, int32_t logicalHeight, int32_t physicalWidth, int32_t physicalHeight);

        /// Builds the authored view matrix from the generated entity transform.
        float4x4 BuildViewMatrix(CameraComponent* camera);

        /// Builds the authored perspective projection matrix using the shared row-vector convention expected by the Wii raster path.
        float4x4 BuildProjectionMatrix(CameraComponent* camera, float aspectRatio);

        /// Multiplies two row-vector matrices using the shared engine convention expected by the Wii frame plan.
        void MultiplyMatrices(const float4x4& left, const float4x4& right, float4x4& result);
    };
}
