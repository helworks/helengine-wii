#pragma once

#include <cstdint>

#include <gccore.h>

#include "MaterialCullMode.hpp"

class CameraClearSettings;
class CameraComponent;
class Entity;
class RenderFrameDrawableSubmission;
class RuntimeMaterial;
class RuntimeTexture;
class RuntimeSubmesh;
class float3;
class float4;
class float4x4;

namespace helengine::wii {
    class WiiCachedMeshData;
    class WiiFramePlan;
    class WiiMeshCache;
    class WiiRuntimeMaterial;
    class WiiRuntimeModel;
    class WiiRuntimeTexture;

    /// Owns the first narrow GX execution path for authored opaque 3D meshes on Wii.
    class WiiRasterRenderer {
    public:
        /// Creates the raster renderer with a shared runtime-model cache.
        explicit WiiRasterRenderer(WiiMeshCache* meshCache);

        /// Draws one extracted camera frame through GX and reports whether this frame claimed scene presentation ownership.
        bool DrawFrame(WiiFramePlan* framePlan);

    private:
        /// Shared runtime-model cache used by the draw path.
        WiiMeshCache* MeshCache;

        /// Configures the GX state used by the current opaque mesh path.
        void ConfigurePipeline(bool useTexturedBranch, bool useIndexedGeometry, bool transparentMaterial);

        /// Configures the GX state used by the indexed lit mesh path with GX fixed-function lighting enabled.
        void ConfigureLitPipeline(bool useTexturedBranch, bool useIndexedGeometry, bool transparentMaterial);

        /// Binds the cached mesh arrays used by the indexed Wii draw path.
        void BindCachedMeshArrays(WiiCachedMeshData* cachedMeshData, bool useTexturedBranch);

        /// Converts the authored runtime clear settings into the presented GX clear color.
        GXColor ResolveClearColor(CameraClearSettings clearSettings);

        /// Converts the authored runtime clear depth into GX packed depth.
        uint32_t ResolveClearDepth(CameraClearSettings clearSettings);

        /// Copies one generated affine matrix directly into a GX position matrix without runtime reinterpretation.
        void CopyAffineMatrixToGx(const float4x4& source, Mtx& destination);

        /// Copies one generated projection matrix into the GX projection upload layout.
        void CopyProjectionMatrixToGx(const float4x4& source, Mtx44& destination);

        /// Configures one GX directional-light state block from the extracted render-frame lighting inputs.
        void ConfigureDirectionalLight(WiiFramePlan* framePlan, GXLightObj& lightObject, GXColor& ambientColor, bool& hasDirectionalLight);

        /// Emits a one-shot comparison between generated and native GX matrix paths for the first 3D draw.
        void ReportMatrixProbe(WiiFramePlan* framePlan, Entity* entity);

        /// Builds one native GX view matrix directly from the active camera transform.
        void BuildNativeViewMatrix(CameraComponent* camera, Mtx& viewMatrix);

        /// Builds one native GX model matrix directly from the active entity transform.
        void BuildNativeModelMatrix(Entity* entity, Mtx& modelMatrix);

        /// Builds one native GX model-view matrix through libogc matrix concatenation.
        void BuildNativeModelViewMatrix(CameraComponent* camera, Entity* entity, Mtx& modelViewMatrix);

        /// Loads one GX normal matrix derived from the current authored model-view transform so fixed-function lighting stays in view space.
        void LoadNormalMatrix(const Mtx& modelViewMatrix);

        /// Resolves whether one submission should use the lit branch for the current checkpoint.
        bool UsesLitBranch(RenderFrameDrawableSubmission* submission);

        /// Resolves one Wii-native runtime texture from the current material graph when present.
        WiiRuntimeTexture* ResolveBoundTexture(WiiRuntimeMaterial* material);

        /// Maps the shared material cull-mode contract onto the reversed GX face-culling convention.
        u8 ResolveGxCullMode(MaterialCullMode cullMode);

        /// Converts a normalized RGB lighting value into a GX color with full alpha.
        GXColor ConvertLightingColorToGx(float3 color);

        /// Transforms one world-space direction through the frame-plan view rotation so GX lighting receives a view-space light direction.
        float3 TransformDirectionToViewSpace(const float3& direction, const float4x4& viewMatrix);

        /// Builds one authored world matrix through the generated platform-adapted float4x4 runtime.
        void BuildWorldMatrix(Entity* entity, float4x4& worldMatrix);

        /// Multiplies two row-vector matrices using the shared engine convention expected by the Wii raster path.
        void MultiplyMatrices(const float4x4& left, const float4x4& right, float4x4& result);

        /// Builds one authored model-view matrix through the generated platform-adapted float4x4 runtime.
        void BuildModelViewMatrix(WiiFramePlan* framePlan, Entity* entity, float4x4& modelViewMatrix);

        /// Draws one unlit or textured cached submesh through indexed GX array submission.
        void DrawCachedSubmesh(WiiRuntimeMaterial* material, WiiCachedMeshData* cachedMeshData, RuntimeSubmesh* runtimeSubmesh, bool useTexturedBranch);

        /// Draws one lit cached submesh through the indexed GX lighting path.
        void DrawCachedLitSubmesh(WiiFramePlan* framePlan, Entity* entity, WiiRuntimeMaterial* material, WiiCachedMeshData* cachedMeshData, RuntimeSubmesh* runtimeSubmesh, bool useTexturedBranch);

        /// Draws one authored runtime submesh through indexed GX triangle submission and the active entity transform.
        void DrawSubmesh(WiiFramePlan* framePlan, RenderFrameDrawableSubmission* submission, WiiRuntimeModel* runtimeModel, RuntimeSubmesh* runtimeSubmesh, Entity* entity);
    };
}
