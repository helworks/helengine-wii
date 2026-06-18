#pragma once

#include <cstdint>

#include <gccore.h>

#include "MaterialCullMode.hpp"

class CameraClearSettings;
class CameraComponent;
class Entity;
class RenderFrameDrawableSubmission;
class RuntimeMaterial;
class RuntimeSubmesh;
class float3;
class float4x4;

namespace helengine::wii {
    class WiiCachedMeshData;
    class WiiFramePlan;
    class WiiMeshCache;
    class WiiRuntimeModel;

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
        void ConfigurePipeline(bool useIndexedGeometry);

        /// Binds the cached mesh arrays used by the indexed Wii draw path.
        void BindCachedMeshArrays(WiiCachedMeshData* cachedMeshData);

        /// Converts the authored runtime clear settings into the presented GX clear color.
        GXColor ResolveClearColor(CameraClearSettings clearSettings);

        /// Converts the authored runtime clear depth into GX packed depth.
        uint32_t ResolveClearDepth(CameraClearSettings clearSettings);

        /// Copies one generated affine matrix directly into a GX position matrix without runtime reinterpretation.
        void CopyAffineMatrixToGx(const float4x4& source, Mtx& destination);

        /// Copies one generated full projection matrix into a GX projection matrix without runtime reinterpretation.
        void CopyProjectionMatrixToGx(const float4x4& source, Mtx44& destination);

        /// Emits a one-shot comparison between generated and native GX matrix paths for the first 3D draw.
        void ReportMatrixProbe(WiiFramePlan* framePlan, Entity* entity);

        /// Builds one native GX view matrix directly from the active camera transform.
        void BuildNativeViewMatrix(CameraComponent* camera, Mtx& viewMatrix);

        /// Builds one native GX model matrix directly from the active entity transform.
        void BuildNativeModelMatrix(Entity* entity, Mtx& modelMatrix);

        /// Builds one native GX model-view matrix through libogc matrix concatenation.
        void BuildNativeModelViewMatrix(CameraComponent* camera, Entity* entity, Mtx& modelViewMatrix);

        /// Maps the shared material cull-mode contract onto the reversed GX face-culling convention.
        u8 ResolveGxCullMode(MaterialCullMode cullMode);

        /// Builds one authored world matrix through the generated platform-adapted float4x4 runtime.
        void BuildWorldMatrix(Entity* entity, float4x4& worldMatrix);

        /// Multiplies two row-vector matrices using the shared engine convention expected by the Wii raster path.
        void MultiplyMatrices(const float4x4& left, const float4x4& right, float4x4& result);

        /// Builds one authored model-view matrix through the generated platform-adapted float4x4 runtime.
        void BuildModelViewMatrix(WiiFramePlan* framePlan, Entity* entity, float4x4& modelViewMatrix);

        /// Draws one unlit cached submesh through indexed GX array submission.
        void DrawCachedSubmesh(WiiCachedMeshData* cachedMeshData, RuntimeSubmesh* runtimeSubmesh);

        /// Draws one authored runtime submesh through indexed GX triangle submission and the active entity transform.
        void DrawSubmesh(WiiFramePlan* framePlan, RenderFrameDrawableSubmission* submission, WiiRuntimeModel* runtimeModel, RuntimeSubmesh* runtimeSubmesh, Entity* entity);
    };
}
