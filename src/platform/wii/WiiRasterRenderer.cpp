#include "platform/wii/WiiRasterRenderer.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <cstdio>

#include <ogc/dvd.h>
#include <ogc/isfs.h>
#include <ogc/system.h>

#include "CameraClearSettings.hpp"
#include "Entity.hpp"
#include "IDrawable3D.hpp"
#include "LightComponent.hpp"
#include "LightType.hpp"
#include "MaterialRenderState.hpp"
#include "RenderFrameDrawableSubmission.hpp"
#include "RenderFrameLightSubmission.hpp"
#include "RuntimeMaterial.hpp"
#include "RuntimeMaterialLightingModel.hpp"
#include "RuntimeTexture.hpp"
#include "RuntimeSubmesh.hpp"
#include "float2.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "float4x4.hpp"
#include "platform/wii/WiiCachedMeshData.hpp"
#include "platform/wii/WiiFramePlan.hpp"
#include "platform/wii/WiiMeshCache.hpp"
#include "platform/wii/WiiRuntimeMaterial.hpp"
#include "platform/wii/WiiRuntimeModel.hpp"
#include "platform/wii/WiiRuntimeTexture.hpp"
#include "runtime/array.hpp"
#include "runtime/native_exceptions.hpp"

namespace {
    bool MatrixProbeReported = false;
    bool FirstDrawStateReported = false;
    bool HasLoggedFirstLightingState = false;
    bool HasLoggedFirstLitDraw = false;
    bool RasterTraceSessionStarted = false;
    bool RasterTraceIsfsInitializationAttempted = false;
    bool RasterTraceIsfsAvailable = false;
    char RasterTraceIsfsPath[144] {};
    Mtx44 UploadedProjectionMatrix {};
    bool UploadedProjectionMatrixCaptured = false;

    /// <summary>
    /// Resolves one host-readable per-title trace file path inside the emulated Wii save-data directory.
    /// </summary>
    /// <param name="fileName">Trace file name to place under the title data directory.</param>
    /// <param name="pathBuffer">Destination buffer that receives the resolved ISFS path.</param>
    /// <param name="pathBufferLength">Capacity of <paramref name="pathBuffer"/> in bytes.</param>
    /// <returns><see langword="true"/> when the current disc id was available and the trace path fit in the supplied buffer.</returns>
    bool TryResolveTitleDataTracePath(const char* fileName, char* pathBuffer, std::size_t pathBufferLength) {
        dvddiskid* diskId = DVD_GetCurrentDiskID();
        if (diskId == nullptr) {
            return false;
        }

        int writtenCharacterCount = std::snprintf(
            pathBuffer,
            pathBufferLength,
            "/title/00010000/%02X%02X%02X%02X/data/%s",
            static_cast<unsigned char>(diskId->gamename[0]),
            static_cast<unsigned char>(diskId->gamename[1]),
            static_cast<unsigned char>(diskId->gamename[2]),
            static_cast<unsigned char>(diskId->gamename[3]),
            fileName);
        return writtenCharacterCount > 0 && static_cast<std::size_t>(writtenCharacterCount) < pathBufferLength;
    }

    /// <summary>
    /// Creates the per-title save-data directory used for packaged-disc host-readable raster trace files.
    /// </summary>
    /// <param name="directoryPathBuffer">Destination buffer that receives the resolved <c>data</c> directory path.</param>
    /// <param name="directoryPathBufferLength">Capacity of <paramref name="directoryPathBuffer"/> in bytes.</param>
    /// <returns><see langword="true"/> when the per-title data directory path was resolved and created or already existed.</returns>
    bool TryEnsureTitleDataTraceDirectory(char* directoryPathBuffer, std::size_t directoryPathBufferLength) {
        dvddiskid* diskId = DVD_GetCurrentDiskID();
        if (diskId == nullptr) {
            return false;
        }

        char titleDirectoryPath[96];
        int titleDirectoryCharacterCount = std::snprintf(
            titleDirectoryPath,
            sizeof(titleDirectoryPath),
            "/title/00010000/%02X%02X%02X%02X",
            static_cast<unsigned char>(diskId->gamename[0]),
            static_cast<unsigned char>(diskId->gamename[1]),
            static_cast<unsigned char>(diskId->gamename[2]),
            static_cast<unsigned char>(diskId->gamename[3]));
        if (titleDirectoryCharacterCount <= 0 || static_cast<std::size_t>(titleDirectoryCharacterCount) >= sizeof(titleDirectoryPath)) {
            return false;
        }

        int dataDirectoryCharacterCount = std::snprintf(
            directoryPathBuffer,
            directoryPathBufferLength,
            "%s/data",
            titleDirectoryPath);
        if (dataDirectoryCharacterCount <= 0 || static_cast<std::size_t>(dataDirectoryCharacterCount) >= directoryPathBufferLength) {
            return false;
        }

        ISFS_CreateDir(titleDirectoryPath, 0, 3, 3, 3);
        ISFS_CreateDir(directoryPathBuffer, 0, 3, 3, 3);
        return true;
    }

    /// <summary>
    /// Resolves and creates the packaged-disc Wii raster trace file under the emulated title data directory.
    /// </summary>
    void InitializeRasterTraceIsfsPath() {
        if (RasterTraceIsfsInitializationAttempted) {
            return;
        }

        RasterTraceIsfsInitializationAttempted = true;
        if (ISFS_Initialize() != ISFS_OK) {
            return;
        }

        char traceDirectoryPath[112];
        if (!TryEnsureTitleDataTraceDirectory(traceDirectoryPath, sizeof(traceDirectoryPath))
            || !TryResolveTitleDataTracePath("wii_raster_trace.txt", RasterTraceIsfsPath, sizeof(RasterTraceIsfsPath))) {
            return;
        }

        s32 fileDescriptor = ISFS_Open(RasterTraceIsfsPath, ISFS_OPEN_RW);
        if (fileDescriptor < 0) {
            if (ISFS_CreateFile(RasterTraceIsfsPath, 0, 3, 3, 3) != ISFS_OK) {
                return;
            }

            fileDescriptor = ISFS_Open(RasterTraceIsfsPath, ISFS_OPEN_RW);
            if (fileDescriptor < 0) {
                return;
            }
        }

        ISFS_Close(fileDescriptor);
        RasterTraceIsfsAvailable = true;
    }

    /// <summary>
    /// Appends one raster trace payload into the emulated title data directory that Dolphin mirrors back into the isolated user profile.
    /// </summary>
    /// <param name="text">Formatted trace payload to append.</param>
    void AppendRasterTraceToIsfs(const char* text) {
        InitializeRasterTraceIsfsPath();
        if (!RasterTraceIsfsAvailable) {
            return;
        }

        s32 fileDescriptor = ISFS_Open(RasterTraceIsfsPath, ISFS_OPEN_RW);
        if (fileDescriptor < 0) {
            RasterTraceIsfsAvailable = false;
            return;
        }

        const std::size_t textLength = std::strlen(text);
        if (textLength > 0) {
            ISFS_Seek(fileDescriptor, 0, SEEK_END);
            ISFS_Write(fileDescriptor, text, static_cast<u32>(textLength));
        }

        ISFS_Close(fileDescriptor);
    }

    /// <summary>
    /// Appends one raster trace payload to the packaged-disc host-readable trace targets.
    /// </summary>
    /// <param name="format">Printf-style format string that describes the payload.</param>
    void AppendRasterTrace(const char* format, ...) {
        char buffer[2048];
        va_list arguments;
        va_start(arguments, format);
        std::vsnprintf(buffer, sizeof(buffer), format, arguments);
        va_end(arguments);

        std::FILE* file = std::fopen("wii_raster_trace.txt", "ab");
        if (file != nullptr) {
            std::fputs(buffer, file);
            std::fflush(file);
            std::fclose(file);
        }

        AppendRasterTraceToIsfs(buffer);
    }

    void ReportFloat4x4(const char* label, const float4x4& matrix) {
        SYS_Report(
            "[Wii][MatrixProbe] %s [%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f] [%.6f %.6f %.6f %.6f]\n",
            label,
            matrix.M11, matrix.M12, matrix.M13, matrix.M14,
            matrix.M21, matrix.M22, matrix.M23, matrix.M24,
            matrix.M31, matrix.M32, matrix.M33, matrix.M34,
            matrix.M41, matrix.M42, matrix.M43, matrix.M44);
        AppendRasterTrace(
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
        AppendRasterTrace(
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
        AppendRasterTrace(
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

        if (!RasterTraceSessionStarted) {
            RasterTraceSessionStarted = true;
            AppendRasterTrace("\n=== Wii raster session %s ===\n", __DATE__ " " __TIME__);
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
    void WiiRasterRenderer::ConfigurePipeline(bool useTexturedBranch, bool useIndexedGeometry, bool transparentMaterial) {
        GX_ClearVtxDesc();
        GX_SetVtxDesc(GX_VA_POS, useIndexedGeometry ? GX_INDEX16 : GX_DIRECT);
        GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
        GX_SetVtxDesc(GX_VA_TEX0, useTexturedBranch ? GX_DIRECT : GX_NONE);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
        GX_SetNumChans(1);
        GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
        GX_SetNumTexGens(useTexturedBranch ? 1 : 0);
        if (useTexturedBranch) {
            GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
        }
        GX_SetNumTevStages(1);
        GX_SetTevOrder(GX_TEVSTAGE0, useTexturedBranch ? GX_TEXCOORD0 : GX_TEXCOORDNULL, useTexturedBranch ? GX_TEXMAP0 : GX_TEXMAP_NULL, GX_COLOR0A0);
        GX_SetTevOp(GX_TEVSTAGE0, useTexturedBranch ? GX_MODULATE : GX_PASSCLR);
        GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
        GX_SetZMode(GX_TRUE, GX_LEQUAL, transparentMaterial ? GX_FALSE : GX_TRUE);
        GX_SetZCompLoc(GX_TRUE);
        GX_SetBlendMode(transparentMaterial ? GX_BM_BLEND : GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
        GX_SetColorUpdate(GX_TRUE);
        GX_SetAlphaUpdate(GX_FALSE);
    }

    /// Configures the GX state used by the indexed lit mesh path with GX fixed-function lighting enabled.
    void WiiRasterRenderer::ConfigureLitPipeline(bool useTexturedBranch, bool useIndexedGeometry, bool transparentMaterial) {
        GX_ClearVtxDesc();
        GX_SetVtxDesc(GX_VA_POS, useIndexedGeometry ? GX_INDEX16 : GX_DIRECT);
        GX_SetVtxDesc(GX_VA_NRM, useIndexedGeometry ? GX_INDEX16 : GX_DIRECT);
        GX_SetVtxDesc(GX_VA_CLR0, GX_NONE);
        GX_SetVtxDesc(GX_VA_TEX0, useTexturedBranch ? GX_DIRECT : GX_NONE);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
        GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
        GX_SetNumChans(1);
        GX_SetChanCtrl(GX_COLOR0A0, GX_ENABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0, GX_DF_CLAMP, GX_AF_NONE);
        GX_SetNumTexGens(useTexturedBranch ? 1 : 0);
        if (useTexturedBranch) {
            GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
        }
        GX_SetNumTevStages(1);
        GX_SetTevOrder(GX_TEVSTAGE0, useTexturedBranch ? GX_TEXCOORD0 : GX_TEXCOORDNULL, useTexturedBranch ? GX_TEXMAP0 : GX_TEXMAP_NULL, GX_COLOR0A0);
        GX_SetTevOp(GX_TEVSTAGE0, useTexturedBranch ? GX_MODULATE : GX_PASSCLR);
        GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
        GX_SetZMode(GX_TRUE, GX_LEQUAL, transparentMaterial ? GX_FALSE : GX_TRUE);
        GX_SetZCompLoc(GX_TRUE);
        GX_SetBlendMode(transparentMaterial ? GX_BM_BLEND : GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
        GX_SetColorUpdate(GX_TRUE);
        GX_SetAlphaUpdate(GX_FALSE);
    }

    /// Binds the cached mesh arrays used by the indexed Wii draw path.
    void WiiRasterRenderer::BindCachedMeshArrays(WiiCachedMeshData* cachedMeshData, bool useTexturedBranch) {
        if (cachedMeshData == nullptr) {
            throw new ArgumentNullException("cachedMeshData");
        } else if (cachedMeshData->PackedPositions == nullptr || cachedMeshData->PackedPositions == Array<WiiPackedPosition3>::Empty() || cachedMeshData->PackedPositions->Length == 0) {
            throw new InvalidOperationException("Wii cached mesh arrays must contain cached positions.");
        }

        GX_SetArray(GX_VA_POS, &(*cachedMeshData->PackedPositions)[0], sizeof(WiiPackedPosition3));
        if (cachedMeshData->HasNormals) {
            if (cachedMeshData->PackedNormals == nullptr || cachedMeshData->PackedNormals == Array<WiiPackedNormal3>::Empty() || cachedMeshData->PackedNormals->Length == 0) {
                throw new InvalidOperationException("Wii lit cached mesh arrays require packed normals.");
            }

            GX_SetArray(GX_VA_NRM, &(*cachedMeshData->PackedNormals)[0], sizeof(WiiPackedNormal3));
        }

        if (useTexturedBranch) {
            if (!cachedMeshData->HasTexCoords || cachedMeshData->PackedTexCoords == nullptr || cachedMeshData->PackedTexCoords == Array<WiiPackedTexCoord2>::Empty() || cachedMeshData->PackedTexCoords->Length == 0) {
                throw new InvalidOperationException("Wii textured cached mesh arrays require cached texture coordinates.");
            }

            GX_SetArray(GX_VA_TEX0, &(*cachedMeshData->PackedTexCoords)[0], sizeof(WiiPackedTexCoord2));
        }
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

    /// Configures one GX directional-light state block from the extracted render-frame lighting inputs.
    void WiiRasterRenderer::ConfigureDirectionalLight(WiiFramePlan* framePlan, GXLightObj& lightObject, GXColor& ambientColor, bool& hasDirectionalLight) {
        if (framePlan == nullptr) {
            throw new ArgumentNullException("framePlan");
        }

        float3 ambientRgb(0.0f, 0.0f, 0.0f);
        float3 directionalRgb(0.0f, 0.0f, 0.0f);
        float3 directionalDirection(0.0f, 0.0f, -1.0f);
        float3 directionalPosition(0.0f, 0.0f, 1024.0f);
        hasDirectionalLight = false;

        for (int32_t lightIndex = 0; lightIndex < framePlan->LightSubmissions->get_Count(); lightIndex++) {
            RenderFrameLightSubmission* submission = (*framePlan->LightSubmissions)[lightIndex];
            if (submission == nullptr) {
                continue;
            }

            LightComponent* light = submission->get_Light();
            if (light == nullptr) {
                continue;
            }

            float4 lightColor = light->get_Color();
            float3 rgb = float3(lightColor.X, lightColor.Y, lightColor.Z) * light->get_Intensity();
            if (submission->get_LightType() == LightType::Ambient) {
                ambientRgb = ambientRgb + rgb;
                continue;
            } else if (submission->get_LightType() != LightType::Directional || hasDirectionalLight) {
                continue;
            }

            Entity* lightEntity = light->get_Parent();
            if (lightEntity == nullptr) {
                continue;
            }

            float4 lightOrientation = lightEntity->get_Orientation();
            lightOrientation.Normalize();
            directionalDirection = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), lightOrientation);
            directionalDirection = TransformDirectionToViewSpace(float3::Normalize(directionalDirection), framePlan->View);
            directionalPosition = float3::Normalize(directionalDirection) * -1024.0f;
            directionalRgb = rgb;
            hasDirectionalLight = true;
        }

        ambientColor = ConvertLightingColorToGx(ambientRgb);
        GX_SetChanAmbColor(GX_COLOR0A0, ambientColor);

        if (!HasLoggedFirstLightingState) {
            HasLoggedFirstLightingState = true;
            SYS_Report(
                "[Wii] Lighting state: frameLights=%ld ambientRgb=(%.3f, %.3f, %.3f) ambientGx=(%u, %u, %u) hasDirectional=%d directionalRgb=(%.3f, %.3f, %.3f) directionalView=(%.3f, %.3f, %.3f) directionalPos=(%.3f, %.3f, %.3f)\n",
                static_cast<long>(framePlan->LightSubmissions->get_Count()),
                ambientRgb.X,
                ambientRgb.Y,
                ambientRgb.Z,
                static_cast<unsigned int>(ambientColor.r),
                static_cast<unsigned int>(ambientColor.g),
                static_cast<unsigned int>(ambientColor.b),
                hasDirectionalLight ? 1 : 0,
                directionalRgb.X,
                directionalRgb.Y,
                directionalRgb.Z,
                directionalDirection.X,
                directionalDirection.Y,
                directionalDirection.Z,
                directionalPosition.X,
                directionalPosition.Y,
                directionalPosition.Z);
            AppendRasterTrace(
                "[Wii] Lighting state: frameLights=%ld ambientRgb=(%.3f, %.3f, %.3f) ambientGx=(%u, %u, %u) hasDirectional=%d directionalRgb=(%.3f, %.3f, %.3f) directionalView=(%.3f, %.3f, %.3f) directionalPos=(%.3f, %.3f, %.3f)\n",
                static_cast<long>(framePlan->LightSubmissions->get_Count()),
                ambientRgb.X,
                ambientRgb.Y,
                ambientRgb.Z,
                static_cast<unsigned int>(ambientColor.r),
                static_cast<unsigned int>(ambientColor.g),
                static_cast<unsigned int>(ambientColor.b),
                hasDirectionalLight ? 1 : 0,
                directionalRgb.X,
                directionalRgb.Y,
                directionalRgb.Z,
                directionalDirection.X,
                directionalDirection.Y,
                directionalDirection.Z,
                directionalPosition.X,
                directionalPosition.Y,
                directionalPosition.Z);
        }

        if (!hasDirectionalLight) {
            GX_InitLightPos(&lightObject, 0.0f, 0.0f, 1024.0f);
            GX_InitLightColor(&lightObject, GXColor { 0x00, 0x00, 0x00, 0xFF });
            GX_LoadLightObj(&lightObject, GX_LIGHT0);
            return;
        }

        GX_InitLightPos(&lightObject, directionalPosition.X, directionalPosition.Y, directionalPosition.Z);
        GX_InitLightColor(&lightObject, ConvertLightingColorToGx(directionalRgb));
        GX_LoadLightObj(&lightObject, GX_LIGHT0);
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
        float4x4 entityLocalTransformMatrix = entity->get_LocalTransformMatrix();
        float4x4 entityWorldTransformMatrix = entity->get_WorldTransformMatrix();

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
        AppendRasterTrace(
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
        ReportFloat4x4("entity.localTransform", entityLocalTransformMatrix);
        ReportFloat4x4("entity.worldTransform", entityWorldTransformMatrix);
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

    /// Loads one GX normal matrix derived from the current authored model-view transform so fixed-function lighting stays in view space.
    void WiiRasterRenderer::LoadNormalMatrix(const Mtx& modelViewMatrix) {
        Mtx sourceModelViewMatrix;
        std::memcpy(sourceModelViewMatrix, modelViewMatrix, sizeof(Mtx));
        Mtx inverseModelViewMatrix;
        if (guMtxInverse(sourceModelViewMatrix, inverseModelViewMatrix) == 0U) {
            throw new InvalidOperationException("Wii lit rendering requires an invertible model-view matrix.");
        }

        Mtx normalMatrix;
        guMtxTranspose(inverseModelViewMatrix, normalMatrix);
        GX_LoadNrmMtxImm(normalMatrix, GX_PNMTX0);
    }

    /// Resolves whether one submission should use the lit branch for the current checkpoint.
    bool WiiRasterRenderer::UsesLitBranch(RenderFrameDrawableSubmission* submission) {
        if (submission == nullptr) {
            throw new ArgumentNullException("submission");
        }

        RuntimeMaterial* material = submission->get_Material();
        if (material == nullptr) {
            throw new InvalidOperationException("Wii drawable submission requires a runtime material.");
        }

        if (material->get_LightingModel() == RuntimeMaterialLightingModel::Unlit) {
            return false;
        }

        return material->get_LightingModel() == RuntimeMaterialLightingModel::MetalRoughPbr;
    }

    /// Resolves one Wii-native runtime texture from the current material graph when present.
    WiiRuntimeTexture* WiiRasterRenderer::ResolveBoundTexture(WiiRuntimeMaterial* material) {
        if (material == nullptr) {
            throw new ArgumentNullException("material");
        }

        RuntimeTexture* runtimeTexture = material->GetOwnedDiffuseTexture();
        if (runtimeTexture == nullptr) {
            return nullptr;
        }

        WiiRuntimeTexture* wiiRuntimeTexture = static_cast<WiiRuntimeTexture*>(runtimeTexture);
        if (wiiRuntimeTexture == nullptr || !wiiRuntimeTexture->HasNativeTextureObject()) {
            return nullptr;
        }

        return wiiRuntimeTexture;
    }

    /// Maps the shared material cull-mode contract onto the GX cull state used by the Wii runtime path.
    u8 WiiRasterRenderer::ResolveGxCullMode(MaterialCullMode cullMode) {
        switch (cullMode) {
            case MaterialCullMode::None:
                return GX_CULL_NONE;

            case MaterialCullMode::Back:
                return GX_CULL_FRONT;

            case MaterialCullMode::Front:
                return GX_CULL_BACK;
        }

        throw new InvalidOperationException("Unsupported material cull mode for Wii GX submission.");
    }

    /// Converts a normalized RGB lighting value into a GX color with full alpha.
    GXColor WiiRasterRenderer::ConvertLightingColorToGx(float3 color) {
        return GXColor {
            static_cast<u8>(std::clamp(color.X, 0.0f, 1.0f) * 255.0f),
            static_cast<u8>(std::clamp(color.Y, 0.0f, 1.0f) * 255.0f),
            static_cast<u8>(std::clamp(color.Z, 0.0f, 1.0f) * 255.0f),
            0xFF
        };
    }

    /// Transforms one world-space direction through the frame-plan view rotation so GX lighting receives a view-space light direction.
    float3 WiiRasterRenderer::TransformDirectionToViewSpace(const float3& direction, const float4x4& viewMatrix) {
        float3 transformedDirection;
        transformedDirection.X = (direction.X * viewMatrix.M11) + (direction.Y * viewMatrix.M21) + (direction.Z * viewMatrix.M31);
        transformedDirection.Y = (direction.X * viewMatrix.M12) + (direction.Y * viewMatrix.M22) + (direction.Z * viewMatrix.M32);
        transformedDirection.Z = (direction.X * viewMatrix.M13) + (direction.Y * viewMatrix.M23) + (direction.Z * viewMatrix.M33);
        return float3::Normalize(transformedDirection);
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

    /// Draws one unlit or textured cached submesh through indexed GX array submission.
    void WiiRasterRenderer::DrawCachedSubmesh(WiiRuntimeMaterial* material, WiiCachedMeshData* cachedMeshData, RuntimeSubmesh* runtimeSubmesh, bool useTexturedBranch) {
        if (material == nullptr) {
            throw new ArgumentNullException("material");
        } else if (cachedMeshData == nullptr) {
            throw new ArgumentNullException("cachedMeshData");
        } else if (runtimeSubmesh == nullptr) {
            throw new ArgumentNullException("runtimeSubmesh");
        } else if (cachedMeshData->Indices16 == nullptr || cachedMeshData->Indices16 == Array<uint16_t>::Empty() || cachedMeshData->Indices16->Length == 0) {
            throw new InvalidOperationException("Wii cached mesh arrays must contain cached 16-bit indices.");
        }

        const int32_t indexStart = runtimeSubmesh->get_IndexStart();
        const int32_t indexCount = runtimeSubmesh->get_IndexCount();
        if (indexStart < 0 || indexCount <= 0 || indexStart + indexCount > cachedMeshData->Indices16->Length) {
            throw new InvalidOperationException("Wii cached submesh ranges must stay within the cached index buffer.");
        }

        const GXColor baseColor = ConvertLightingColorToGx(material->GetBaseColor());
        GX_Begin(GX_TRIANGLES, GX_VTXFMT0, indexCount);
        for (int32_t indexOffset = 0; indexOffset < indexCount; indexOffset++) {
            const uint16_t cachedIndex = (*cachedMeshData->Indices16)[indexStart + indexOffset];
            GX_Position1x16(cachedIndex);
            GX_Color4u8(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
            if (useTexturedBranch) {
                const WiiPackedTexCoord2 packedTextureCoordinate = (*cachedMeshData->PackedTexCoords)[cachedIndex];
                const float2 textureCoordinate(packedTextureCoordinate.U, packedTextureCoordinate.V);
                GX_TexCoord2f32(textureCoordinate.X, textureCoordinate.Y);
            }
        }
        GX_End();
    }

    /// Draws one lit cached submesh through the indexed GX lighting path.
    void WiiRasterRenderer::DrawCachedLitSubmesh(WiiFramePlan* framePlan, Entity* entity, WiiRuntimeMaterial* material, WiiCachedMeshData* cachedMeshData, RuntimeSubmesh* runtimeSubmesh, bool useTexturedBranch) {
        if (framePlan == nullptr) {
            throw new ArgumentNullException("framePlan");
        } else if (entity == nullptr) {
            throw new ArgumentNullException("entity");
        } else if (material == nullptr) {
            throw new ArgumentNullException("material");
        } else if (cachedMeshData == nullptr) {
            throw new ArgumentNullException("cachedMeshData");
        } else if (runtimeSubmesh == nullptr) {
            throw new ArgumentNullException("runtimeSubmesh");
        } else if (!cachedMeshData->HasNormals || cachedMeshData->PackedNormals == nullptr || cachedMeshData->PackedNormals == Array<WiiPackedNormal3>::Empty() || cachedMeshData->PackedNormals->Length == 0) {
            throw new InvalidOperationException("Wii lit rendering requires cached packed mesh normals.");
        } else if (cachedMeshData->Indices16 == nullptr || cachedMeshData->Indices16 == Array<uint16_t>::Empty() || cachedMeshData->Indices16->Length == 0) {
            throw new InvalidOperationException("Wii cached lit meshes must contain cached 16-bit indices.");
        }

        const int32_t indexStart = runtimeSubmesh->get_IndexStart();
        const int32_t indexCount = runtimeSubmesh->get_IndexCount();
        if (indexStart < 0 || indexCount <= 0 || indexStart + indexCount > cachedMeshData->Indices16->Length) {
            throw new InvalidOperationException("Wii cached lit submesh ranges must stay within the cached index buffer.");
        }

        BindCachedMeshArrays(cachedMeshData, useTexturedBranch);
        GXLightObj lightObject;
        GXColor ambientColor;
        bool hasDirectionalLight = false;
        ConfigureDirectionalLight(framePlan, lightObject, ambientColor, hasDirectionalLight);
        const float3 baseColor = material->GetBaseColor();
        GX_SetChanMatColor(GX_COLOR0A0, ConvertLightingColorToGx(baseColor));

        if (!HasLoggedFirstLitDraw) {
            HasLoggedFirstLitDraw = true;
            SYS_Report(
                "[Wii] First lit draw: baseColor=(%.3f, %.3f, %.3f) textured=%d hasDirectional=%d ambientGx=(%u, %u, %u) indices=%ld\n",
                baseColor.X,
                baseColor.Y,
                baseColor.Z,
                useTexturedBranch ? 1 : 0,
                hasDirectionalLight ? 1 : 0,
                static_cast<unsigned int>(ambientColor.r),
                static_cast<unsigned int>(ambientColor.g),
                static_cast<unsigned int>(ambientColor.b),
                static_cast<long>(indexCount));
            AppendRasterTrace(
                "[Wii] First lit draw: baseColor=(%.3f, %.3f, %.3f) textured=%d hasDirectional=%d ambientGx=(%u, %u, %u) indices=%ld\n",
                baseColor.X,
                baseColor.Y,
                baseColor.Z,
                useTexturedBranch ? 1 : 0,
                hasDirectionalLight ? 1 : 0,
                static_cast<unsigned int>(ambientColor.r),
                static_cast<unsigned int>(ambientColor.g),
                static_cast<unsigned int>(ambientColor.b),
                static_cast<long>(indexCount));
        }

        GX_Begin(GX_TRIANGLES, GX_VTXFMT0, indexCount);
        for (int32_t indexOffset = 0; indexOffset < indexCount; indexOffset++) {
            const uint16_t cachedIndex = (*cachedMeshData->Indices16)[indexStart + indexOffset];
            GX_Position1x16(cachedIndex);
            GX_Normal1x16(cachedIndex);
            if (useTexturedBranch) {
                const WiiPackedTexCoord2 packedTextureCoordinate = (*cachedMeshData->PackedTexCoords)[cachedIndex];
                const float2 textureCoordinate(packedTextureCoordinate.U, packedTextureCoordinate.V);
                GX_TexCoord2f32(textureCoordinate.X, textureCoordinate.Y);
            }
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

        float4x4 modelViewMatrix;
        BuildModelViewMatrix(framePlan, entity, modelViewMatrix);

        Mtx nativeModelViewMatrix;
        CopyAffineMatrixToGx(modelViewMatrix, nativeModelViewMatrix);
        ReportMatrixProbe(framePlan, entity);
        GX_LoadPosMtxImm(nativeModelViewMatrix, GX_PNMTX0);
        GX_SetCurrentMtx(GX_PNMTX0);

        RuntimeMaterial* material = submission->get_Material();
        if (material == nullptr) {
            throw new InvalidOperationException("Wii drawable submission requires a runtime material.");
        }

        WiiRuntimeMaterial* wiiRuntimeMaterial = static_cast<WiiRuntimeMaterial*>(material);
        if (wiiRuntimeMaterial == nullptr) {
            throw new InvalidOperationException("Wii drawable submission requires a WiiRuntimeMaterial.");
        }

        const bool expectsTexture = !wiiRuntimeMaterial->GetTextureRelativePath().empty();
        WiiRuntimeTexture* boundTexture = expectsTexture
            ? ResolveBoundTexture(wiiRuntimeMaterial)
            : nullptr;
        if (expectsTexture && boundTexture == nullptr) {
            throw new InvalidOperationException("Wii textured material requires one resolved runtime texture.");
        }

        WiiCachedMeshData* cachedMeshData = runtimeModel->CachedMeshData;
        if (cachedMeshData == nullptr) {
            throw new InvalidOperationException("Wii drawable submission requires cached mesh data.");
        }

        const bool useTexturedBranch = boundTexture != nullptr;
        if (useTexturedBranch) {
            GX_LoadTexObj(boundTexture->GetNativeTextureObject(), GX_TEXMAP0);
        }

        const bool useLitBranch = UsesLitBranch(submission);
        const bool transparentMaterial = material->get_RenderState()->get_BlendMode() == MaterialBlendMode::AlphaBlend;
        const MaterialCullMode cullMode = material->get_RenderState()->get_CullMode();
        const u8 gxCullMode = ResolveGxCullMode(cullMode);
        if (!FirstDrawStateReported) {
            FirstDrawStateReported = true;
            const float3 baseColor = wiiRuntimeMaterial->GetBaseColor();
            const uint16_t firstIndex = cachedMeshData->Indices16 == nullptr || cachedMeshData->Indices16->Length <= runtimeSubmesh->get_IndexStart()
                ? 0
                : (*cachedMeshData->Indices16)[runtimeSubmesh->get_IndexStart()];
            const uint16_t secondIndex = cachedMeshData->Indices16 == nullptr || cachedMeshData->Indices16->Length <= runtimeSubmesh->get_IndexStart() + 1
                ? 0
                : (*cachedMeshData->Indices16)[runtimeSubmesh->get_IndexStart() + 1];
            const uint16_t thirdIndex = cachedMeshData->Indices16 == nullptr || cachedMeshData->Indices16->Length <= runtimeSubmesh->get_IndexStart() + 2
                ? 0
                : (*cachedMeshData->Indices16)[runtimeSubmesh->get_IndexStart() + 2];
            const WiiPackedPosition3 firstPosition = cachedMeshData->PackedPositions == nullptr || cachedMeshData->PackedPositions->Length <= firstIndex
                ? WiiPackedPosition3 { 0.0f, 0.0f, 0.0f }
                : (*cachedMeshData->PackedPositions)[firstIndex];
            const WiiPackedPosition3 secondPosition = cachedMeshData->PackedPositions == nullptr || cachedMeshData->PackedPositions->Length <= secondIndex
                ? WiiPackedPosition3 { 0.0f, 0.0f, 0.0f }
                : (*cachedMeshData->PackedPositions)[secondIndex];
            const WiiPackedPosition3 thirdPosition = cachedMeshData->PackedPositions == nullptr || cachedMeshData->PackedPositions->Length <= thirdIndex
                ? WiiPackedPosition3 { 0.0f, 0.0f, 0.0f }
                : (*cachedMeshData->PackedPositions)[thirdIndex];
            const WiiPackedNormal3 firstNormal = cachedMeshData->PackedNormals == nullptr || cachedMeshData->PackedNormals->Length <= firstIndex
                ? WiiPackedNormal3 { 0.0f, 0.0f, 0.0f }
                : (*cachedMeshData->PackedNormals)[firstIndex];
            SYS_Report(
                "[Wii][DrawProbe] lightingModel=%d useLit=%d expectsTexture=%d textured=%d cull=%d gxCull=%u hasNormals=%d hasTexCoords=%d positions=%ld indices=%ld indexStart=%ld indexCount=%ld baseColor=(%.3f, %.3f, %.3f) texturePath=%s firstTri=(%u,%u,%u) p0=(%.3f,%.3f,%.3f) p1=(%.3f,%.3f,%.3f) p2=(%.3f,%.3f,%.3f) n0=(%.3f,%.3f,%.3f)\n",
                static_cast<int32_t>(material->get_LightingModel()),
                useLitBranch ? 1 : 0,
                expectsTexture ? 1 : 0,
                useTexturedBranch ? 1 : 0,
                static_cast<int32_t>(cullMode),
                static_cast<uint32_t>(gxCullMode),
                cachedMeshData->HasNormals ? 1 : 0,
                cachedMeshData->HasTexCoords ? 1 : 0,
                cachedMeshData->PackedPositions == nullptr ? 0L : static_cast<long>(cachedMeshData->PackedPositions->Length),
                cachedMeshData->Indices16 == nullptr ? 0L : static_cast<long>(cachedMeshData->Indices16->Length),
                static_cast<long>(runtimeSubmesh->get_IndexStart()),
                static_cast<long>(runtimeSubmesh->get_IndexCount()),
                baseColor.X,
                baseColor.Y,
                baseColor.Z,
                wiiRuntimeMaterial->GetTextureRelativePath().c_str(),
                static_cast<unsigned int>(firstIndex),
                static_cast<unsigned int>(secondIndex),
                static_cast<unsigned int>(thirdIndex),
                firstPosition.X,
                firstPosition.Y,
                firstPosition.Z,
                secondPosition.X,
                secondPosition.Y,
                secondPosition.Z,
                thirdPosition.X,
                thirdPosition.Y,
                thirdPosition.Z,
                firstNormal.X,
                firstNormal.Y,
                firstNormal.Z);
            AppendRasterTrace(
                "[Wii][DrawProbe] lightingModel=%d useLit=%d expectsTexture=%d textured=%d cull=%d gxCull=%u hasNormals=%d hasTexCoords=%d positions=%ld indices=%ld indexStart=%ld indexCount=%ld baseColor=(%.3f, %.3f, %.3f) texturePath=%s firstTri=(%u,%u,%u) p0=(%.3f,%.3f,%.3f) p1=(%.3f,%.3f,%.3f) p2=(%.3f,%.3f,%.3f) n0=(%.3f,%.3f,%.3f)\n",
                static_cast<int32_t>(material->get_LightingModel()),
                useLitBranch ? 1 : 0,
                expectsTexture ? 1 : 0,
                useTexturedBranch ? 1 : 0,
                static_cast<int32_t>(cullMode),
                static_cast<uint32_t>(gxCullMode),
                cachedMeshData->HasNormals ? 1 : 0,
                cachedMeshData->HasTexCoords ? 1 : 0,
                cachedMeshData->PackedPositions == nullptr ? 0L : static_cast<long>(cachedMeshData->PackedPositions->Length),
                cachedMeshData->Indices16 == nullptr ? 0L : static_cast<long>(cachedMeshData->Indices16->Length),
                static_cast<long>(runtimeSubmesh->get_IndexStart()),
                static_cast<long>(runtimeSubmesh->get_IndexCount()),
                baseColor.X,
                baseColor.Y,
                baseColor.Z,
                wiiRuntimeMaterial->GetTextureRelativePath().c_str(),
                static_cast<unsigned int>(firstIndex),
                static_cast<unsigned int>(secondIndex),
                static_cast<unsigned int>(thirdIndex),
                firstPosition.X,
                firstPosition.Y,
                firstPosition.Z,
                secondPosition.X,
                secondPosition.Y,
                secondPosition.Z,
                thirdPosition.X,
                thirdPosition.Y,
                thirdPosition.Z,
                firstNormal.X,
                firstNormal.Y,
                firstNormal.Z);
        }

        GX_SetCullMode(gxCullMode);
        if (useLitBranch) {
            LoadNormalMatrix(nativeModelViewMatrix);
            ConfigureLitPipeline(useTexturedBranch, true, transparentMaterial);
            DrawCachedLitSubmesh(framePlan, entity, wiiRuntimeMaterial, cachedMeshData, runtimeSubmesh, useTexturedBranch);
        } else {
            ConfigurePipeline(useTexturedBranch, true, transparentMaterial);
            BindCachedMeshArrays(cachedMeshData, useTexturedBranch);
            DrawCachedSubmesh(wiiRuntimeMaterial, cachedMeshData, runtimeSubmesh, useTexturedBranch);
        }
    }
}
