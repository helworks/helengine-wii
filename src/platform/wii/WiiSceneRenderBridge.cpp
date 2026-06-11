#include "platform/wii/WiiSceneRenderBridge.hpp"

#include <cmath>

#include "AmbientLightComponent.hpp"
#include "CameraComponent.hpp"
#include "CameraViewportResolver.hpp"
#include "Core.hpp"
#include "DirectionalLightComponent.hpp"
#include "Entity.hpp"
#include "ICamera.hpp"
#include "IDrawable3D.hpp"
#include "IRenderQueue3D.hpp"
#include "LightComponent.hpp"
#include "ObjectManager.hpp"
#include "RenderFrame.hpp"
#include "RenderFrameDrawableSubmission.hpp"
#include "RenderFrameExtractionResult.hpp"
#include "RenderFrameExtractionService.hpp"
#include "RendererBackendCapabilityProfile.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "float4x4.hpp"
#include "platform/wii/WiiFramePlan.hpp"
#include "platform/wii/WiiRenderQueueSnapshotVisitor.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/native_exceptions.hpp"

namespace helengine::wii {
    /// Builds one strict frame plan for the active camera and visible opaque drawables, or returns null when no active camera is available yet.
    WiiFramePlan* WiiSceneRenderBridge::BuildFramePlan(RendererBackendCapabilityProfile* capabilities, int32_t logicalWidth, int32_t logicalHeight, int32_t physicalWidth, int32_t physicalHeight) {
        if (capabilities == nullptr) {
            throw new ArgumentNullException("capabilities");
        } else if (logicalWidth < 1) {
            throw new ArgumentOutOfRangeException("logicalWidth");
        } else if (logicalHeight < 1) {
            throw new ArgumentOutOfRangeException("logicalHeight");
        } else if (physicalWidth < 1) {
            throw new ArgumentOutOfRangeException("physicalWidth");
        } else if (physicalHeight < 1) {
            throw new ArgumentOutOfRangeException("physicalHeight");
        }

        if (!HasActiveCamera()) {
            return nullptr;
        }

        CameraComponent* camera = ResolveActiveCamera();
        List<IDrawable3D*>* drawables = SnapshotVisibleDrawables(camera);
        List<CameraComponent*>* cameras = new List<CameraComponent*>(1);
        cameras->Add(camera);
        List<LightComponent*>* lights = new List<LightComponent*>();
        ObjectManager* objectManager = Core::get_Instance()->get_ObjectManager();
        List<AmbientLightComponent*>* ambientLights = objectManager->get_AmbientLights();
        for (int32_t lightIndex = 0; lightIndex < ambientLights->get_Count(); lightIndex++) {
            lights->Add((*ambientLights)[lightIndex]);
        }

        List<DirectionalLightComponent*>* directionalLights = objectManager->get_DirectionalLights();
        for (int32_t lightIndex = 0; lightIndex < directionalLights->get_Count(); lightIndex++) {
            lights->Add((*directionalLights)[lightIndex]);
        }

        RenderFrameExtractionService extractor;
        RenderFrameExtractionResult* extraction = extractor.Extract(cameras, drawables, lights, capabilities);
        RenderFrame* frame = (*extraction->get_Frames())[0];
        if (frame->get_HasTransparentDrawables()) {
            throw new NotSupportedException("Transparent 3D submissions are not supported in the first Wii renderer tier.");
        }

        float4 logicalViewport = CameraViewportResolver::ResolveViewport(camera->get_Viewport(), logicalWidth, logicalHeight);
        float4 physicalViewport = ResolvePhysicalViewport(logicalViewport, logicalWidth, logicalHeight, physicalWidth, physicalHeight);
        float4x4 view = BuildViewMatrix(camera);
        float4x4 projection = BuildProjectionMatrix(camera, logicalViewport.Z / logicalViewport.W);
        float4x4 viewProjection;
        MultiplyMatrices(view, projection, viewProjection);
        return new WiiFramePlan(
            camera,
            frame->get_DrawableSubmissions(),
            frame->get_LightSubmissions(),
            logicalViewport,
            physicalViewport,
            view,
            projection,
            viewProjection);
    }

    /// Returns whether the current runtime state exposes at least one enabled camera the Wii backend can render.
    bool WiiSceneRenderBridge::HasActiveCamera() {
        List<ICamera*>* cameras = Core::get_Instance()->get_ObjectManager()->get_Cameras();
        for (int32_t index = 0; index < cameras->get_Count(); index++) {
            CameraComponent* camera = he_cpp_try_cast<CameraComponent>((*cameras)[index]);
            if (camera == nullptr || camera->get_Parent() == nullptr || !camera->get_Parent()->get_IsHierarchyEnabled()) {
                continue;
            }

            return true;
        }

        return false;
    }

    /// Resolves the first enabled runtime camera the Wii backend is willing to render.
    CameraComponent* WiiSceneRenderBridge::ResolveActiveCamera() {
        List<ICamera*>* cameras = Core::get_Instance()->get_ObjectManager()->get_Cameras();
        for (int32_t index = 0; index < cameras->get_Count(); index++) {
            CameraComponent* camera = he_cpp_try_cast<CameraComponent>((*cameras)[index]);
            if (camera == nullptr || camera->get_Parent() == nullptr || !camera->get_Parent()->get_IsHierarchyEnabled()) {
                continue;
            }

            return camera;
        }

        throw new InvalidOperationException("No active runtime camera is available for the Wii frame plan.");
    }

    /// Copies the ordered visible 3D queue for one camera into a backend-local list.
    List<IDrawable3D*>* WiiSceneRenderBridge::SnapshotVisibleDrawables(CameraComponent* camera) {
        if (camera == nullptr) {
            throw new ArgumentNullException("camera");
        }

        WiiRenderQueueSnapshotVisitor visitor;
        camera->get_RenderQueue3D()->VisitOrdered(&visitor);
        return visitor.Items;
    }

    /// Resolves the physical GX viewport from the logical viewport and current presented framebuffer dimensions.
    float4 WiiSceneRenderBridge::ResolvePhysicalViewport(const float4& logicalViewport, int32_t logicalWidth, int32_t logicalHeight, int32_t physicalWidth, int32_t physicalHeight) {
        const float horizontalScale = static_cast<float>(physicalWidth) / static_cast<float>(logicalWidth);
        const float verticalScale = static_cast<float>(physicalHeight) / static_cast<float>(logicalHeight);
        return float4(
            logicalViewport.X * horizontalScale,
            logicalViewport.Y * verticalScale,
            logicalViewport.Z * horizontalScale,
            logicalViewport.W * verticalScale);
    }

    /// Builds the authored view matrix from the active camera transform using the same handwritten row-vector path as the GameCube backend.
    float4x4 WiiSceneRenderBridge::BuildViewMatrix(CameraComponent* camera) {
        if (camera == nullptr) {
            throw new ArgumentNullException("camera");
        } else if (camera->get_Parent() == nullptr) {
            throw new InvalidOperationException("Wii frame-plan construction requires a camera parent entity.");
        }

        float3 cameraPosition = camera->get_Parent()->get_Position();
        float4 cameraOrientation = camera->get_Parent()->get_Orientation();
        float3 cameraForward = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), cameraOrientation);
        float3 cameraUp = float4::RotateVector(float3(0.0f, 1.0f, 0.0f), cameraOrientation);
        float3 cameraTarget = cameraPosition + cameraForward;
        float3 backward = float3::Normalize(cameraPosition - cameraTarget);
        float3 right = float3::Normalize(float3::Cross(cameraUp, backward));
        float3 up = float3::Cross(backward, right);
        float4x4 view;
        view.M11 = right.X;
        view.M12 = up.X;
        view.M13 = backward.X;
        view.M14 = 0.0f;
        view.M21 = right.Y;
        view.M22 = up.Y;
        view.M23 = backward.Y;
        view.M24 = 0.0f;
        view.M31 = right.Z;
        view.M32 = up.Z;
        view.M33 = backward.Z;
        view.M34 = 0.0f;
        view.M41 = -float3::Dot(right, cameraPosition);
        view.M42 = -float3::Dot(up, cameraPosition);
        view.M43 = -float3::Dot(backward, cameraPosition);
        view.M44 = 1.0f;
        return view;
    }

    /// Builds the authored perspective projection matrix using the same handwritten row-vector path as the GameCube backend.
    float4x4 WiiSceneRenderBridge::BuildProjectionMatrix(CameraComponent* camera, float aspectRatio) {
        if (camera == nullptr) {
            throw new ArgumentNullException("camera");
        } else if (aspectRatio <= 0.0f) {
            throw new ArgumentOutOfRangeException("aspectRatio");
        }

        const float fieldOfView = static_cast<float>(3.14159265358979323846 / 4.0);
        const float nearPlaneDistance = camera->get_NearPlaneDistance();
        const float farPlaneDistance = camera->get_FarPlaneDistance();
        const float yScale = 1.0f / static_cast<float>(std::tan(static_cast<double>(fieldOfView) * 0.5));
        const float xScale = yScale / aspectRatio;
        const float negFarRange = farPlaneDistance / (nearPlaneDistance - farPlaneDistance);

        float4x4 projection;
        projection.M11 = xScale;
        projection.M12 = 0.0f;
        projection.M13 = 0.0f;
        projection.M14 = 0.0f;
        projection.M21 = 0.0f;
        projection.M22 = yScale;
        projection.M23 = 0.0f;
        projection.M24 = 0.0f;
        projection.M31 = 0.0f;
        projection.M32 = 0.0f;
        projection.M33 = negFarRange;
        projection.M34 = -1.0f;
        projection.M41 = 0.0f;
        projection.M42 = 0.0f;
        projection.M43 = nearPlaneDistance * negFarRange;
        projection.M44 = 0.0f;
        return projection;
    }

    /// Multiplies two row-vector matrices using the shared engine convention expected by the Wii frame plan.
    void WiiSceneRenderBridge::MultiplyMatrices(const float4x4& left, const float4x4& right, float4x4& result) {
        result.M11 = (((left.M11 * right.M11) + (left.M12 * right.M21)) + (left.M13 * right.M31)) + (left.M14 * right.M41);
        result.M12 = (((left.M11 * right.M12) + (left.M12 * right.M22)) + (left.M13 * right.M32)) + (left.M14 * right.M42);
        result.M13 = (((left.M11 * right.M13) + (left.M12 * right.M23)) + (left.M13 * right.M33)) + (left.M14 * right.M43);
        result.M14 = (((left.M11 * right.M14) + (left.M12 * right.M24)) + (left.M13 * right.M34)) + (left.M14 * right.M44);
        result.M21 = (((left.M21 * right.M11) + (left.M22 * right.M21)) + (left.M23 * right.M31)) + (left.M24 * right.M41);
        result.M22 = (((left.M21 * right.M12) + (left.M22 * right.M22)) + (left.M23 * right.M32)) + (left.M24 * right.M42);
        result.M23 = (((left.M21 * right.M13) + (left.M22 * right.M23)) + (left.M23 * right.M33)) + (left.M24 * right.M43);
        result.M24 = (((left.M21 * right.M14) + (left.M22 * right.M24)) + (left.M23 * right.M34)) + (left.M24 * right.M44);
        result.M31 = (((left.M31 * right.M11) + (left.M32 * right.M21)) + (left.M33 * right.M31)) + (left.M34 * right.M41);
        result.M32 = (((left.M31 * right.M12) + (left.M32 * right.M22)) + (left.M33 * right.M32)) + (left.M34 * right.M42);
        result.M33 = (((left.M31 * right.M13) + (left.M32 * right.M23)) + (left.M33 * right.M33)) + (left.M34 * right.M43);
        result.M34 = (((left.M31 * right.M14) + (left.M32 * right.M24)) + (left.M33 * right.M34)) + (left.M34 * right.M44);
        result.M41 = (((left.M41 * right.M11) + (left.M42 * right.M21)) + (left.M43 * right.M31)) + (left.M44 * right.M41);
        result.M42 = (((left.M41 * right.M12) + (left.M42 * right.M22)) + (left.M43 * right.M32)) + (left.M44 * right.M42);
        result.M43 = (((left.M41 * right.M13) + (left.M42 * right.M23)) + (left.M43 * right.M33)) + (left.M44 * right.M43);
        result.M44 = (((left.M41 * right.M14) + (left.M42 * right.M24)) + (left.M43 * right.M34)) + (left.M44 * right.M44);
    }
}
