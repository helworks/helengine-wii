#include "platform/wii/WiiRasterRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <ogc/system.h>

#include "CameraClearSettings.hpp"
#include "Entity.hpp"
#include "IDrawable3D.hpp"
#include "MaterialRenderState.hpp"
#include "RenderFrameDrawableSubmission.hpp"
#include "RuntimeMaterial.hpp"
#include "RuntimeSubmesh.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "float4x4.hpp"
#include "platform/wii/WiiCachedMeshData.hpp"
#include "platform/wii/WiiFramePlan.hpp"
#include "platform/wii/WiiMeshCache.hpp"
#include "platform/wii/WiiRuntimeModel.hpp"
#include "runtime/array.hpp"
#include "runtime/native_exceptions.hpp"

namespace {
    constexpr u8 OpaqueMeshColorRed = 255;
    constexpr u8 OpaqueMeshColorGreen = 255;
    constexpr u8 OpaqueMeshColorBlue = 255;
    constexpr u8 OpaqueMeshColorAlpha = 255;
    bool MatrixProbeReported = false;
    Mtx44 UploadedProjectionMatrix {};
    bool UploadedProjectionMatrixCaptured = false;

    void ReportFloat4x4(const char* label, const float4x4& matrix) {
        SYS_Report(
            "[Wii][MatrixProbe] %s [%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f]\n",
            label,
            matrix.M11, matrix.M12, matrix.M13, matrix.M14,
            matrix.M21, matrix.M22, matrix.M23, matrix.M24,
            matrix.M31, matrix.M32, matrix.M33, matrix.M34,
            matrix.M41, matrix.M42, matrix.M43, matrix.M44);
    }

    void ReportMtx(const char* label, const Mtx& matrix) {
        SYS_Report(
            "[Wii][MatrixProbe] %s [%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f]\n",
            label,
            matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3],
            matrix[1][0], matrix[1][1], matrix[1][2], matrix[1][3],
            matrix[2][0], matrix[2][1], matrix[2][2], matrix[2][3]);
    }

    void ReportMtx44(const char* label, const Mtx44& matrix) {
        SYS_Report(
            "[Wii][MatrixProbe] %s [%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f]\n",
            label,
            matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3],
            matrix[1][0], matrix[1][1], matrix[1][2], matrix[1][3],
            matrix[2][0], matrix[2][1], matrix[2][2], matrix[2][3],
            matrix[3][0], matrix[3][1], matrix[3][2], matrix[3][3]);
    }
}

namespace helengine::wii {
    /// Creates the raster renderer with a shared runtime-model cache.
    WiiRasterRenderer::WiiRasterRenderer(WiiMeshCache* meshCache)
        : MeshCache(meshCache) {
        if (MeshCache == nullptr) {
            throw new ArgumentNullException("meshCache");
        }
    }

    /// Draws one extracted camera frame through GX and reports whether this frame claimed scene presentation ownership.
    bool WiiRasterRenderer::DrawFrame(WiiFramePlan* framePlan) {
        if (framePlan == nullptr) {
            throw new ArgumentNullException("framePlan");
        }

        CameraClearSettings clearSettings = framePlan->Camera->get_ClearSettings();
        GX_SetCopyClear(ResolveClearColor(clearSettings), ResolveClearDepth(clearSettings));
        GX_SetViewport(framePlan->PhysicalViewport.X, framePlan->PhysicalViewport.Y, framePlan->PhysicalViewport.Z, framePlan->PhysicalViewport.W, 0.0f, 1.0f);
        GX_SetScissor(
            static_cast<u32>(framePlan->PhysicalViewport.X),
            static_cast<u32>(framePlan->PhysicalViewport.Y),
            static_cast<u32>(framePlan->PhysicalViewport.Z),
            static_cast<u32>(framePlan->PhysicalViewport.W));
        GX_InvVtxCache();

        Mtx44 projectionMatrix;
        CopyProjectionMatrixToGx(framePlan->Projection, projectionMatrix);
        std::memcpy(UploadedProjectionMatrix, projectionMatrix, sizeof(Mtx44));
        UploadedProjectionMatrixCaptured = true;
        GX_LoadProjectionMtx(projectionMatrix, GX_PERSPECTIVE);

        if (framePlan->DrawableSubmissions->get_Count() <= 0) {
            return true;
        }

        for (int32_t submissionIndex = 0; submissionIndex < framePlan->DrawableSubmissions->get_Count(); submissionIndex++) {
            RenderFrameDrawableSubmission* submission = (*framePlan->DrawableSubmissions)[submissionIndex];
            if (submission == nullptr || submission->get_Drawable() == nullptr) {
                continue;
            }

            WiiRuntimeModel* runtimeModel = MeshCache->Resolve(submission->get_Drawable()->get_Model());
            if (runtimeModel == nullptr) {
                throw new InvalidOperationException("Wii mesh cache must resolve runtime models for extracted drawable submissions.");
            }

            Array<RuntimeSubmesh*>* submeshes = runtimeModel->get_Submeshes();
            if (submeshes == nullptr || submeshes == Array<RuntimeSubmesh*>::Empty()) {
                throw new InvalidOperationException("Wii runtime models must provide runtime submesh metadata.");
            }

            const int32_t submeshIndex = submission->get_SubmeshIndex();
            if (submeshIndex < 0 || submeshIndex >= submeshes->get_Length()) {
                throw new InvalidOperationException("Wii drawable submission submesh index is outside the runtime model submesh range.");
            }

            Entity* entity = submission->get_Drawable()->get_Parent();
            if (entity == nullptr) {
                throw new InvalidOperationException("Wii drawable submissions require a parent entity.");
            }

            DrawSubmesh(framePlan, submission, runtimeModel, (*submeshes)[submeshIndex], entity);
        }

        return true;
    }

    /// Configures the GX state used by the current opaque mesh path.
    void WiiRasterRenderer::ConfigurePipeline(bool useIndexedGeometry) {
        GX_ClearVtxDesc();
        GX_SetVtxDesc(GX_VA_POS, useIndexedGeometry ? GX_INDEX16 : GX_DIRECT);
        GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
        GX_SetNumChans(1);
        GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
        GX_SetNumTexGens(0);
        GX_SetNumTevStages(1);
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
        GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
        GX_SetCullMode(GX_CULL_FRONT);
        GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        GX_SetZCompLoc(GX_TRUE);
        GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
        GX_SetColorUpdate(GX_TRUE);
        GX_SetAlphaUpdate(GX_FALSE);
    }

    /// Binds the cached mesh arrays used by the indexed Wii draw path.
    void WiiRasterRenderer::BindCachedMeshArrays(WiiCachedMeshData* cachedMeshData) {
        if (cachedMeshData == nullptr) {
            throw new ArgumentNullException("cachedMeshData");
        } else if (cachedMeshData->PackedPositions == nullptr || cachedMeshData->PackedPositions == Array<WiiPackedPosition3>::Empty() || cachedMeshData->PackedPositions->Length == 0) {
            throw new InvalidOperationException("Wii cached mesh arrays must contain cached positions.");
        }

        if (cachedMeshData->PackedPositionBuffer == nullptr) {
            throw new InvalidOperationException("Wii cached mesh arrays must contain one aligned packed position buffer.");
        }

        GX_SetArray(GX_VA_POS, cachedMeshData->PackedPositionBuffer, sizeof(WiiPackedPosition3));
    }

    /// Converts the authored runtime clear settings into the presented GX clear color.
    GXColor WiiRasterRenderer::ResolveClearColor(CameraClearSettings clearSettings) {
        if (!clearSettings.get_ClearColorEnabled()) {
            return GXColor { 0x00, 0x00, 0x00, 0xFF };
        }

        float4 color = clearSettings.get_ClearColor();
        return GXColor {
            static_cast<u8>(std::clamp(color.X, 0.0f, 1.0f) * 255.0f),
            static_cast<u8>(std::clamp(color.Y, 0.0f, 1.0f) * 255.0f),
            static_cast<u8>(std::clamp(color.Z, 0.0f, 1.0f) * 255.0f),
            static_cast<u8>(std::clamp(color.W, 0.0f, 1.0f) * 255.0f)
        };
    }

    /// Converts the authored runtime clear depth into GX packed depth.
    uint32_t WiiRasterRenderer::ResolveClearDepth(CameraClearSettings clearSettings) {
        if (!clearSettings.get_ClearDepthEnabled()) {
            return 0x00FFFFFF;
        }

        return static_cast<uint32_t>(clearSettings.get_ClearDepth() * 16777215.0f);
    }

    /// Copies one generated affine matrix directly into a GX position matrix without runtime reinterpretation.
    void WiiRasterRenderer::CopyAffineMatrixToGx(const float4x4& source, Mtx& destination) {
        destination[0][0] = source.M11;
        destination[0][1] = source.M21;
        destination[0][2] = source.M31;
        destination[0][3] = source.M41;
        destination[1][0] = source.M12;
        destination[1][1] = source.M22;
        destination[1][2] = source.M32;
        destination[1][3] = source.M42;
        destination[2][0] = source.M13;
        destination[2][1] = source.M23;
        destination[2][2] = source.M33;
        destination[2][3] = source.M43;
    }

    /// Copies one generated projection matrix into the GX projection upload layout.
    void WiiRasterRenderer::CopyProjectionMatrixToGx(const float4x4& source, Mtx44& destination) {
        destination[0][0] = source.M11;
        destination[0][1] = source.M21;
        destination[0][2] = source.M31;
        destination[0][3] = source.M41;
        destination[1][0] = source.M12;
        destination[1][1] = source.M22;
        destination[1][2] = source.M32;
        destination[1][3] = source.M42;
        destination[2][0] = source.M13;
        destination[2][1] = source.M23;
        destination[2][2] = source.M33 + 1.0f;
        destination[2][3] = source.M43;
        destination[3][0] = source.M14;
        destination[3][1] = source.M24;
        destination[3][2] = source.M34;
        destination[3][3] = source.M44;
    }

    /// Emits one first-draw matrix comparison between generated float4x4 output and the native libogc GX path.
    void WiiRasterRenderer::ReportMatrixProbe(WiiFramePlan* framePlan, Entity* entity) {
        if (MatrixProbeReported) {
            return;
        } else if (framePlan == nullptr) {
            throw new ArgumentNullException("framePlan");
        } else if (entity == nullptr) {
            throw new ArgumentNullException("entity");
        }

        const float4 entityOrientation = entity->get_Orientation();
        const bool isIdentityOrientation =
            std::fabs(entityOrientation.X) < 0.0001f &&
            std::fabs(entityOrientation.Y) < 0.0001f &&
            std::fabs(entityOrientation.Z) < 0.0001f &&
            std::fabs(entityOrientation.W - 1.0f) < 0.0001f;
        if (isIdentityOrientation) {
            return;
        }

        float4x4 generatedWorldMatrix;
        BuildWorldMatrix(entity, generatedWorldMatrix);
        float4x4 generatedModelViewMatrix;
        BuildModelViewMatrix(framePlan, entity, generatedModelViewMatrix);

        Mtx nativeViewMatrix;
        BuildNativeViewMatrix(framePlan->Camera, nativeViewMatrix);
        Mtx nativeModelMatrix;
        BuildNativeModelMatrix(entity, nativeModelMatrix);
        Mtx nativeModelViewMatrix;
        BuildNativeModelViewMatrix(framePlan->Camera, entity, nativeModelViewMatrix);
        Mtx44 nativeProjectionMatrix;
        const float viewportHeight = framePlan->LogicalViewport.W > 0.0f ? framePlan->LogicalViewport.W : 1.0f;
        const float aspectRatio = framePlan->LogicalViewport.Z / viewportHeight;
        guPerspective(
            nativeProjectionMatrix,
            45.0f,
            aspectRatio,
            framePlan->Camera->get_NearPlaneDistance(),
            framePlan->Camera->get_FarPlaneDistance());

        SYS_Report(
            "[Wii][MatrixProbe] entityPos=(%.6f, %.6f, %.6f) entityScale=(%.6f, %.6f, %.6f) entityRot=(%.6f, %.6f, %.6f, %.6f)\n",
            entity->get_Position().X,
            entity->get_Position().Y,
            entity->get_Position().Z,
            entity->get_Scale().X,
            entity->get_Scale().Y,
            entity->get_Scale().Z,
            entityOrientation.X,
            entityOrientation.Y,
            entityOrientation.Z,
            entityOrientation.W);
        ReportFloat4x4("generated.view", framePlan->View);
        ReportMtx("native.view", nativeViewMatrix);
        ReportFloat4x4("generated.projection", framePlan->Projection);
        ReportMtx44("native.projection", nativeProjectionMatrix);
        if (UploadedProjectionMatrixCaptured) {
            ReportMtx44("uploaded.projection", UploadedProjectionMatrix);
        }
        ReportFloat4x4("generated.world", generatedWorldMatrix);
        ReportMtx("native.model", nativeModelMatrix);
        ReportFloat4x4("generated.modelView", generatedModelViewMatrix);
        ReportMtx("native.modelView", nativeModelViewMatrix);
        MatrixProbeReported = true;
    }

    /// Builds one native GX view matrix directly from the active camera transform through libogc.
    void WiiRasterRenderer::BuildNativeViewMatrix(CameraComponent* camera, Mtx& viewMatrix) {
        if (camera == nullptr) {
            throw new ArgumentNullException("camera");
        } else if (camera->get_Parent() == nullptr) {
            throw new InvalidOperationException("Wii native model-view construction requires a camera parent entity.");
        }

        float3 cameraPosition = camera->get_Parent()->get_Position();
        float4 cameraOrientation = camera->get_Parent()->get_Orientation();
        float3 cameraForward = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), cameraOrientation);
        float3 cameraUp = float4::RotateVector(float3(0.0f, 1.0f, 0.0f), cameraOrientation);
        float3 cameraTarget = cameraPosition + cameraForward;
        guVector nativeCameraPosition = { cameraPosition.X, cameraPosition.Y, cameraPosition.Z };
        guVector nativeCameraUp = { cameraUp.X, cameraUp.Y, cameraUp.Z };
        guVector nativeCameraTarget = { cameraTarget.X, cameraTarget.Y, cameraTarget.Z };
        guLookAt(viewMatrix, &nativeCameraPosition, &nativeCameraUp, &nativeCameraTarget);
    }

    /// Builds one native GX model matrix directly from the active entity transform.
    void WiiRasterRenderer::BuildNativeModelMatrix(Entity* entity, Mtx& modelMatrix) {
        if (entity == nullptr) {
            throw new ArgumentNullException("entity");
        }

        float3 entityScale = entity->get_Scale();
        float4 entityOrientation = entity->get_Orientation();
        float3 entityPosition = entity->get_Position();
        entityOrientation.Normalize();
        const float xx = entityOrientation.X * entityOrientation.X;
        const float yy = entityOrientation.Y * entityOrientation.Y;
        const float zz = entityOrientation.Z * entityOrientation.Z;
        const float xy = entityOrientation.X * entityOrientation.Y;
        const float xz = entityOrientation.X * entityOrientation.Z;
        const float yz = entityOrientation.Y * entityOrientation.Z;
        const float xw = entityOrientation.X * entityOrientation.W;
        const float yw = entityOrientation.Y * entityOrientation.W;
        const float zw = entityOrientation.Z * entityOrientation.W;

        modelMatrix[0][0] = (1.0f - (2.0f * (yy + zz))) * entityScale.X;
        modelMatrix[0][1] = (2.0f * (xy + zw)) * entityScale.X;
        modelMatrix[0][2] = (2.0f * (xz - yw)) * entityScale.X;
        modelMatrix[0][3] = entityPosition.X;
        modelMatrix[1][0] = (2.0f * (xy - zw)) * entityScale.Y;
        modelMatrix[1][1] = (1.0f - (2.0f * (zz + xx))) * entityScale.Y;
        modelMatrix[1][2] = (2.0f * (yz + xw)) * entityScale.Y;
        modelMatrix[1][3] = entityPosition.Y;
        modelMatrix[2][0] = (2.0f * (xz + yw)) * entityScale.Z;
        modelMatrix[2][1] = (2.0f * (yz - xw)) * entityScale.Z;
        modelMatrix[2][2] = (1.0f - (2.0f * (yy + xx))) * entityScale.Z;
        modelMatrix[2][3] = entityPosition.Z;
    }

    /// Builds one native GX model-view matrix through libogc matrix concatenation.
    void WiiRasterRenderer::BuildNativeModelViewMatrix(CameraComponent* camera, Entity* entity, Mtx& modelViewMatrix) {
        if (camera == nullptr) {
            throw new ArgumentNullException("camera");
        } else if (entity == nullptr) {
            throw new ArgumentNullException("entity");
        }

        Mtx viewMatrix;
        BuildNativeViewMatrix(camera, viewMatrix);
        Mtx modelMatrix;
        BuildNativeModelMatrix(entity, modelMatrix);
        guMtxConcat(viewMatrix, modelMatrix, modelViewMatrix);
    }

    /// Maps the shared material cull-mode contract onto the GX cull state used by the Wii runtime path.
    u8 WiiRasterRenderer::ResolveGxCullMode(MaterialCullMode cullMode) {
        switch (cullMode) {
            case MaterialCullMode::None:
                return GX_CULL_NONE;

            case MaterialCullMode::Back:
                return GX_CULL_BACK;

            case MaterialCullMode::Front:
                return GX_CULL_FRONT;
        }

        throw new InvalidOperationException("Unsupported material cull mode for Wii GX submission.");
    }

    /// Builds one authored world matrix using the same handwritten row-vector path as the GameCube backend.
    void WiiRasterRenderer::BuildWorldMatrix(Entity* entity, float4x4& worldMatrix) {
        if (entity == nullptr) {
            throw new ArgumentNullException("entity");
        }

        float3 entityScale = entity->get_Scale();
        float4 entityOrientation = entity->get_Orientation();
        float3 entityPosition = entity->get_Position();
        entityOrientation.Normalize();
        const float xx = entityOrientation.X * entityOrientation.X;
        const float yy = entityOrientation.Y * entityOrientation.Y;
        const float zz = entityOrientation.Z * entityOrientation.Z;
        const float xy = entityOrientation.X * entityOrientation.Y;
        const float xz = entityOrientation.X * entityOrientation.Z;
        const float yz = entityOrientation.Y * entityOrientation.Z;
        const float xw = entityOrientation.X * entityOrientation.W;
        const float yw = entityOrientation.Y * entityOrientation.W;
        const float zw = entityOrientation.Z * entityOrientation.W;

        worldMatrix.M11 = (1.0f - (2.0f * (yy + zz))) * entityScale.X;
        worldMatrix.M12 = (2.0f * (xy + zw)) * entityScale.X;
        worldMatrix.M13 = (2.0f * (xz - yw)) * entityScale.X;
        worldMatrix.M14 = 0.0f;
        worldMatrix.M21 = (2.0f * (xy - zw)) * entityScale.Y;
        worldMatrix.M22 = (1.0f - (2.0f * (zz + xx))) * entityScale.Y;
        worldMatrix.M23 = (2.0f * (yz + xw)) * entityScale.Y;
        worldMatrix.M24 = 0.0f;
        worldMatrix.M31 = (2.0f * (xz + yw)) * entityScale.Z;
        worldMatrix.M32 = (2.0f * (yz - xw)) * entityScale.Z;
        worldMatrix.M33 = (1.0f - (2.0f * (yy + xx))) * entityScale.Z;
        worldMatrix.M34 = 0.0f;
        worldMatrix.M41 = entityPosition.X;
        worldMatrix.M42 = entityPosition.Y;
        worldMatrix.M43 = entityPosition.Z;
        worldMatrix.M44 = 1.0f;
    }

    /// Multiplies two row-vector matrices using the shared engine convention expected by the Wii raster path.
    void WiiRasterRenderer::MultiplyMatrices(const float4x4& left, const float4x4& right, float4x4& result) {
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

    /// Builds one authored model-view matrix using the same handwritten row-vector path as the GameCube backend.
    void WiiRasterRenderer::BuildModelViewMatrix(WiiFramePlan* framePlan, Entity* entity, float4x4& modelViewMatrix) {
        if (framePlan == nullptr) {
            throw new ArgumentNullException("framePlan");
        } else if (entity == nullptr) {
            throw new ArgumentNullException("entity");
        }

        float4x4 worldMatrix;
        BuildWorldMatrix(entity, worldMatrix);
        MultiplyMatrices(worldMatrix, framePlan->View, modelViewMatrix);
    }

    /// Draws one unlit cached submesh through immediate GX triangle submission sourced from cached authored positions.
    void WiiRasterRenderer::DrawCachedSubmesh(WiiCachedMeshData* cachedMeshData, RuntimeSubmesh* runtimeSubmesh) {
        if (cachedMeshData == nullptr) {
            throw new ArgumentNullException("cachedMeshData");
        } else if (runtimeSubmesh == nullptr) {
            throw new ArgumentNullException("runtimeSubmesh");
        } else if (cachedMeshData->Indices16 == nullptr || cachedMeshData->Indices16 == Array<uint16_t>::Empty() || cachedMeshData->Indices16->Length == 0) {
            throw new InvalidOperationException("Wii cached mesh arrays must contain cached 16-bit indices.");
        } else if (cachedMeshData->PackedPositions == nullptr || cachedMeshData->PackedPositions == Array<WiiPackedPosition3>::Empty() || cachedMeshData->PackedPositions->Length == 0) {
            throw new InvalidOperationException("Wii cached mesh arrays must contain cached positions for immediate GX submission.");
        }

        const int32_t indexStart = runtimeSubmesh->get_IndexStart();
        const int32_t indexCount = runtimeSubmesh->get_IndexCount();
        if (indexStart < 0 || indexCount <= 0 || indexStart + indexCount > cachedMeshData->Indices16->Length) {
            throw new InvalidOperationException("Wii cached submesh ranges must stay within the cached index buffer.");
        }

        GX_Begin(GX_TRIANGLES, GX_VTXFMT0, indexCount);
        for (int32_t indexOffset = 0; indexOffset < indexCount; indexOffset++) {
            const uint16_t cachedIndex = (*cachedMeshData->Indices16)[indexStart + indexOffset];
            if (cachedIndex >= cachedMeshData->PackedPositions->Length) {
                throw new InvalidOperationException("Wii cached mesh indices must stay within the cached position buffer.");
            }

            const WiiPackedPosition3 cachedPosition = (*cachedMeshData->PackedPositions)[cachedIndex];
            GX_Position3f32(cachedPosition.X, cachedPosition.Y, cachedPosition.Z);
            GX_Color4u8(OpaqueMeshColorRed, OpaqueMeshColorGreen, OpaqueMeshColorBlue, OpaqueMeshColorAlpha);
        }
        GX_End();
    }

    /// Draws one authored runtime submesh through indexed GX triangle submission and the active entity transform.
    void WiiRasterRenderer::DrawSubmesh(WiiFramePlan* framePlan, RenderFrameDrawableSubmission* submission, WiiRuntimeModel* runtimeModel, RuntimeSubmesh* runtimeSubmesh, Entity* entity) {
        if (framePlan == nullptr) {
            throw new ArgumentNullException("framePlan");
        } else if (submission == nullptr) {
            throw new ArgumentNullException("submission");
        } else if (runtimeModel == nullptr) {
            throw new ArgumentNullException("runtimeModel");
        } else if (runtimeSubmesh == nullptr) {
            throw new ArgumentNullException("runtimeSubmesh");
        } else if (entity == nullptr) {
            throw new ArgumentNullException("entity");
        }

        Mtx nativeModelViewMatrix;
        BuildNativeModelViewMatrix(framePlan->Camera, entity, nativeModelViewMatrix);
        ReportMatrixProbe(framePlan, entity);
        GX_LoadPosMtxImm(nativeModelViewMatrix, GX_PNMTX0);
        GX_SetCurrentMtx(GX_PNMTX0);

        RuntimeMaterial* material = submission->get_Material();
        if (material == nullptr) {
            throw new InvalidOperationException("Wii drawable submission requires a runtime material.");
        }

        WiiCachedMeshData* cachedMeshData = runtimeModel->CachedMeshData;
        if (cachedMeshData == nullptr) {
            throw new InvalidOperationException("Wii drawable submission requires cached mesh data.");
        }

        ConfigurePipeline(false);
        GX_SetCullMode(ResolveGxCullMode(material->get_RenderState()->get_CullMode()));
        DrawCachedSubmesh(cachedMeshData, runtimeSubmesh);
    }
}
