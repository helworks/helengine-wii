#include "platform/wii/WiiRenderManager3D.hpp"

#include <algorithm>
#include <cstdlib>

#include <ogc/system.h>

#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "CameraClearSettings.hpp"
#include "MaterialBlendMode.hpp"
#include "MaterialCullMode.hpp"
#include "MaterialRenderState.hpp"
#include "ModelAsset.hpp"
#include "ModelAssetIndexData.hpp"
#include "ModelSubmeshAsset.hpp"
#include "ModelSubmeshResolver.hpp"
#include "PlatformMaterialAsset.hpp"
#include "RendererBackendCapabilityProfile.hpp"
#include "RuntimeMaterial.hpp"
#include "RuntimeMaterialLightingModel.hpp"
#include "RuntimeSubmesh.hpp"
#include "float2.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "platform/wii/WiiFramePlan.hpp"
#include "platform/wii/WiiMeshCache.hpp"
#include "platform/wii/WiiRasterRenderer.hpp"
#include "platform/wii/WiiRenderManager2D.hpp"
#include "platform/wii/WiiRuntimeModel.hpp"
#include "platform/wii/WiiSceneRenderBridge.hpp"
#include "runtime/array.hpp"
#include "runtime/finally.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/native_exceptions.hpp"
#include "system/io/file.hpp"

namespace helengine::wii {
    /// Creates the Wii 3D backend and its owned bridge/cache/raster collaborators.
    WiiRenderManager3D::WiiRenderManager3D()
        : RenderManager3D()
        , CapabilityProfile(new RendererBackendCapabilityProfile(true, false, false, false, 0, 0))
        , SceneRenderBridge(new WiiSceneRenderBridge())
        , MeshCache(new WiiMeshCache())
        , RasterRenderer(new WiiRasterRenderer(MeshCache))
        , OverlayRenderManager2D(nullptr)
        , PresentedClearColorValid(false)
        , PresentedClearColor { 0x00, 0x00, 0x00, 0xFF }
        , HasRenderedSceneValue(false)
        , PresentedFrameWidth(0U)
        , PresentedFrameHeight(0U)
        , ExtractedFrameCount(0U) {
    }

    /// Releases owned Wii renderer collaborators.
    WiiRenderManager3D::~WiiRenderManager3D() {
        delete RasterRenderer;
        delete MeshCache;
        delete SceneRenderBridge;
        delete CapabilityProfile;
    }

    /// Rebuilds one legacy raw material asset path through the cooked platform-owned Wii material contract.
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

        return BuildMaterialFromCooked(materialAssetPath);
    }

    /// Builds a Wii runtime model that keeps authored submesh and geometry arrays alive.
    RuntimeModel* WiiRenderManager3D::BuildModelFromRaw(ModelAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        }

        ModelAssetIndexData* indexData = ModelAssetIndexData::Resolve(data);
        WiiRuntimeModel* runtimeModel = new WiiRuntimeModel();
        runtimeModel->set_Id(data->get_Id());
        runtimeModel->SetBounds(data->BoundsMin, data->BoundsMax);
        runtimeModel->SetSubmeshes(ModelSubmeshResolver::BuildRuntimeSubmeshes(data));
        runtimeModel->Positions = data->Positions;
        runtimeModel->Normals = data->Normals;
        runtimeModel->TexCoords = data->TexCoords;
        runtimeModel->Indices16 = indexData->get_Indices16();
        runtimeModel->Indices32 = indexData->get_Indices32();
        runtimeModel->Uses32BitIndices = indexData->get_Uses32BitIndices();
        SYS_Report(
            "[Wii] RM3D build raw model id=%s positions=%u submeshes=%u ptr=%p\n",
            data->get_Id().c_str(),
            static_cast<unsigned>(runtimeModel->Positions != nullptr ? runtimeModel->Positions->get_Length() : 0),
            static_cast<unsigned>(runtimeModel->get_Submeshes() != nullptr ? runtimeModel->get_Submeshes()->get_Length() : 0),
            runtimeModel);
        delete indexData;
        return runtimeModel;
    }

    /// Builds a Wii runtime model from one serialized cooked model asset path.
    RuntimeModel* WiiRenderManager3D::BuildModelFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Wii cooked model path is required.", "cookedAssetPath");
        }

        FileStream* stream = File::OpenRead(cookedAssetPath.c_str());
        try {
            Asset* asset = AssetSerializer::Deserialize(stream);
            ModelAsset* cookedModelAsset = he_cpp_try_cast<ModelAsset>(asset);
            if (cookedModelAsset == nullptr) {
                throw new ArgumentException("Wii cooked model payload did not deserialize as ModelAsset.", "cookedAssetPath");
            }

            stream->Dispose();
            WiiRuntimeModel* runtimeModel = static_cast<WiiRuntimeModel*>(BuildModelFromRaw(cookedModelAsset));
            runtimeModel->OwnedSourceModelAsset = cookedModelAsset;
            SYS_Report(
                "[Wii] RM3D build cooked model path=%s id=%s ptr=%p\n",
                cookedAssetPath.c_str(),
                cookedModelAsset->get_Id().c_str(),
                runtimeModel);
            return runtimeModel;
        } catch (...) {
            if (stream != nullptr) {
                stream->Dispose();
            }

            throw;
        }
    }

    /// Rebuilds one cooked platform-owned material payload path into the shared runtime material contract used by generated scenes.
    RuntimeMaterial* WiiRenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Wii cooked material path is required.", "cookedAssetPath");
        }

        FileStream* stream = File::OpenRead(cookedAssetPath.c_str());
        auto streamGuard = he_cpp_make_scope_exit([&]() {
            if (stream != nullptr) {
                stream->Dispose();
                delete stream;
            }
        });
        Asset* asset = AssetSerializer::Deserialize(stream);
        PlatformMaterialAsset* materialAsset = he_cpp_try_cast<PlatformMaterialAsset>(asset);
        if (materialAsset == nullptr) {
            delete asset;
            throw new InvalidOperationException("Wii cooked material payload did not deserialize into a PlatformMaterialAsset.");
        }

        auto materialAssetGuard = he_cpp_make_scope_exit([&]() {
            delete materialAsset;
        });
        return BuildMaterialFromCooked(materialAsset);
    }

    /// Rebuilds one cooked platform-owned material payload into the minimal runtime contract currently consumed by generated scenes.
    RuntimeMaterial* WiiRenderManager3D::BuildMaterialFromCooked(PlatformMaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw new ArgumentNullException("materialAsset");
        }

        RuntimeMaterial* runtimeMaterial = new RuntimeMaterial();
        runtimeMaterial->set_Id(materialAsset->get_Id());
        runtimeMaterial->SetRenderState(BuildMaterialRenderState(materialAsset));
        runtimeMaterial->set_LightingModel(materialAsset->Lit
            ? RuntimeMaterialLightingModel::MetalRoughPbr
            : RuntimeMaterialLightingModel::Unlit);
        runtimeMaterial->set_SupportsNormalMapping(false);
        runtimeMaterial->set_SupportsEmissive(false);
        runtimeMaterial->set_CastsShadows(materialAsset->Lit);
        runtimeMaterial->set_ReceivesShadows(materialAsset->Lit);
        return runtimeMaterial;
    }

    /// Releases one Wii runtime material after the final scene reference is removed.
    void WiiRenderManager3D::ReleaseMaterial(RuntimeMaterial* material) {
        if (material == nullptr) {
            throw new ArgumentNullException("material");
        }

        ReleasedMaterials.push_back(material);
    }

    /// Releases one Wii runtime model after the final scene reference is removed.
    void WiiRenderManager3D::ReleaseModel(RuntimeModel* model) {
        if (model == nullptr) {
            throw new ArgumentNullException("model");
        }

        ReleasedModels.push_back(model);
    }

    /// Releases any deferred runtime-material and runtime-model deletions after the scene manager reaches a safe transition boundary.
    void WiiRenderManager3D::FlushReleasedAssets() {
        for (RuntimeMaterial* material : ReleasedMaterials) {
            if (material == nullptr) {
                continue;
            }

            material->Dispose();
            delete material;
        }

        ReleasedMaterials.clear();

        for (RuntimeModel* model : ReleasedModels) {
            if (model == nullptr) {
                continue;
            }

            WiiRuntimeModel* runtimeModel = static_cast<WiiRuntimeModel*>(model);
            ReleaseOwnedSourceModelAsset(runtimeModel);

            Array<RuntimeSubmesh*>* submeshes = runtimeModel->get_Submeshes();
            if (submeshes != nullptr && submeshes != Array<RuntimeSubmesh*>::Empty()) {
                for (int32_t submeshIndex = 0; submeshIndex < submeshes->get_Length(); submeshIndex++) {
                    delete (*submeshes)[submeshIndex];
                }

                delete submeshes;
            }

            if (runtimeModel->CachedMeshData != nullptr) {
                if (runtimeModel->CachedMeshData->PackedPositions != nullptr && runtimeModel->CachedMeshData->PackedPositions != Array<WiiPackedPosition3>::Empty()) {
                    delete runtimeModel->CachedMeshData->PackedPositions;
                }

                if (runtimeModel->CachedMeshData->PackedPositionBuffer != nullptr) {
                    free(runtimeModel->CachedMeshData->PackedPositionBuffer);
                }

                if (runtimeModel->CachedMeshData->PackedNormals != nullptr && runtimeModel->CachedMeshData->PackedNormals != Array<WiiPackedNormal3>::Empty()) {
                    delete runtimeModel->CachedMeshData->PackedNormals;
                }

                if (runtimeModel->CachedMeshData->PackedNormalBuffer != nullptr) {
                    free(runtimeModel->CachedMeshData->PackedNormalBuffer);
                }

                if (runtimeModel->CachedMeshData->PackedTexCoords != nullptr && runtimeModel->CachedMeshData->PackedTexCoords != Array<WiiPackedTexCoord2>::Empty()) {
                    delete runtimeModel->CachedMeshData->PackedTexCoords;
                }

                if (runtimeModel->CachedMeshData->PackedTexCoordBuffer != nullptr) {
                    free(runtimeModel->CachedMeshData->PackedTexCoordBuffer);
                }

                if (runtimeModel->CachedMeshData->Indices16 != nullptr && runtimeModel->CachedMeshData->Indices16 != Array<uint16_t>::Empty()) {
                    delete runtimeModel->CachedMeshData->Indices16;
                }

                if (runtimeModel->CachedMeshData->SubmeshIndexStarts != nullptr && runtimeModel->CachedMeshData->SubmeshIndexStarts != Array<int32_t>::Empty()) {
                    delete runtimeModel->CachedMeshData->SubmeshIndexStarts;
                }

                if (runtimeModel->CachedMeshData->SubmeshIndexCounts != nullptr && runtimeModel->CachedMeshData->SubmeshIndexCounts != Array<int32_t>::Empty()) {
                    delete runtimeModel->CachedMeshData->SubmeshIndexCounts;
                }

                delete runtimeModel->CachedMeshData;
            }

            if (runtimeModel->Positions != nullptr && runtimeModel->Positions != Array<float3>::Empty()) {
                delete runtimeModel->Positions;
            }

            if (runtimeModel->Normals != nullptr && runtimeModel->Normals != Array<float3>::Empty()) {
                delete runtimeModel->Normals;
            }

            if (runtimeModel->TexCoords != nullptr && runtimeModel->TexCoords != Array<float2>::Empty()) {
                delete runtimeModel->TexCoords;
            }

            if (runtimeModel->Indices16 != nullptr && runtimeModel->Indices16 != Array<uint16_t>::Empty()) {
                delete runtimeModel->Indices16;
            }

            if (runtimeModel->Indices32 != nullptr && runtimeModel->Indices32 != Array<uint32_t>::Empty()) {
                delete runtimeModel->Indices32;
            }

            delete runtimeModel;
        }

        ReleasedModels.clear();
    }

    /// Extracts the current frame and renders it through GX.
    void WiiRenderManager3D::Draw() {
        if (OverlayRenderManager2D == nullptr) {
            throw new InvalidOperationException("WiiRenderManager3D requires an overlay WiiRenderManager2D before Draw().");
        } else if (PresentedFrameWidth == 0U) {
            throw new InvalidOperationException("WiiRenderManager3D requires one presented framebuffer width before Draw().");
        } else if (PresentedFrameHeight == 0U) {
            throw new InvalidOperationException("WiiRenderManager3D requires one presented framebuffer height before Draw().");
        }

        OverlayRenderManager2D->Draw();
        WiiFramePlan* framePlan = SceneRenderBridge->BuildFramePlan(CapabilityProfile, MainWindowSize.X, MainWindowSize.Y, PresentedFrameWidth, PresentedFrameHeight);
        PresentedClearColorValid = false;
        if (framePlan == nullptr) {
            HasRenderedSceneValue = false;
            return;
        }

        UpdatePresentedClearColor(framePlan);
        if (framePlan->DrawableSubmissions->get_Count() <= 0) {
            HasRenderedSceneValue = false;
            delete framePlan;
            return;
        }

        ExtractedFrameCount++;
        HasRenderedSceneValue = RasterRenderer->DrawFrame(framePlan);
        delete framePlan;
    }

    /// Registers the 2D overlay render manager used by the generated draw path.
    void WiiRenderManager3D::SetOverlayRenderManager2D(WiiRenderManager2D* renderManager2D) {
        if (renderManager2D == nullptr) {
            throw new ArgumentNullException("renderManager2D");
        }

        OverlayRenderManager2D = renderManager2D;
    }

    /// Registers the physical presented framebuffer size used for GX viewport and scissor setup.
    void WiiRenderManager3D::SetPresentedFrameSize(uint16_t width, uint16_t height) {
        if (width == 0U) {
            throw new ArgumentOutOfRangeException("width");
        } else if (height == 0U) {
            throw new ArgumentOutOfRangeException("height");
        }

        PresentedFrameWidth = width;
        PresentedFrameHeight = height;
    }

    /// Returns the strict backend capability surface exposed by the first Wii tier.
    RendererBackendCapabilityProfile* WiiRenderManager3D::GetCapabilityProfile() {
        return CapabilityProfile;
    }

    /// Reports whether this backend has emitted a native scene frame.
    bool WiiRenderManager3D::HasRenderedScene() const {
        return HasRenderedSceneValue;
    }

    /// Returns whether the current frame resolved one authored camera clear color for presentation.
    bool WiiRenderManager3D::HasPresentedClearColor() const {
        return PresentedClearColorValid;
    }

    /// Returns the authored camera clear color resolved for the current presented frame.
    GXColor WiiRenderManager3D::GetPresentedClearColor() const {
        return PresentedClearColor;
    }

    /// Updates the presented clear color from the active frame-plan camera.
    void WiiRenderManager3D::UpdatePresentedClearColor(WiiFramePlan* framePlan) {
        if (framePlan == nullptr || framePlan->Camera == nullptr) {
            return;
        }

        CameraClearSettings clearSettings = framePlan->Camera->get_ClearSettings();
        if (!clearSettings.get_ClearColorEnabled()) {
            return;
        }

        PresentedClearColor = ToGxColor(clearSettings.get_ClearColor());
        PresentedClearColorValid = true;
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

    /// Rebuilds one material render-state instance from the cooked Wii material payload flags.
    MaterialRenderState* WiiRenderManager3D::BuildMaterialRenderState(PlatformMaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw new ArgumentNullException("materialAsset");
        }

        MaterialRenderState* renderState = new MaterialRenderState();
        renderState->set_CullMode(materialAsset->DoubleSided
            ? MaterialCullMode::None
            : MaterialCullMode::Back);
        renderState->set_BlendMode(materialAsset->BaseColorA < 0xFF
            ? MaterialBlendMode::AlphaBlend
            : MaterialBlendMode::Opaque);
        renderState->set_DepthTestEnabled(true);
        renderState->set_DepthWriteEnabled(materialAsset->BaseColorA >= 0xFF);
        return renderState;
    }

    /// Releases one transient cooked/raw model asset after the shared runtime model has been rebuilt.
    void WiiRenderManager3D::ReleaseTransientModelAsset(ModelAsset* asset) {
        if (asset == nullptr) {
            return;
        }

        Array<float3>* positions = asset->Positions;
        Array<float3>* normals = asset->Normals;
        Array<float2>* texCoords = asset->TexCoords;
        Array<uint16_t>* indices16 = asset->Indices16;
        Array<uint32_t>* indices32 = asset->Indices32;
        Array<ModelSubmeshAsset*>* submeshes = asset->Submeshes;
        asset->Positions = nullptr;
        asset->Normals = nullptr;
        asset->TexCoords = nullptr;
        asset->Indices16 = nullptr;
        asset->Indices32 = nullptr;
        asset->Submeshes = nullptr;

        if (submeshes != nullptr) {
            for (int32_t index = 0; index < submeshes->get_Length(); index++) {
                delete (*submeshes)[index];
            }
        }

        if (positions != nullptr && positions != Array<float3>::Empty()) {
            delete positions;
        }

        if (normals != nullptr && normals != Array<float3>::Empty()) {
            delete normals;
        }

        if (texCoords != nullptr && texCoords != Array<float2>::Empty()) {
            delete texCoords;
        }

        if (indices16 != nullptr && indices16 != Array<uint16_t>::Empty()) {
            delete indices16;
        }

        if (indices32 != nullptr && indices32 != Array<uint32_t>::Empty()) {
            delete indices32;
        }

        if (submeshes != nullptr && submeshes != Array<ModelSubmeshAsset*>::Empty()) {
            delete submeshes;
        }

        delete asset;
    }

    /// Releases one owned deserialized cooked model payload attached to a Wii runtime model.
    void WiiRenderManager3D::ReleaseOwnedSourceModelAsset(WiiRuntimeModel* runtimeModel) {
        if (runtimeModel == nullptr || runtimeModel->OwnedSourceModelAsset == nullptr) {
            return;
        }

        ModelAsset* ownedSourceModelAsset = runtimeModel->OwnedSourceModelAsset;
        Array<ModelSubmeshAsset*>* submeshes = ownedSourceModelAsset->Submeshes;
        ownedSourceModelAsset->Positions = nullptr;
        ownedSourceModelAsset->Normals = nullptr;
        ownedSourceModelAsset->TexCoords = nullptr;
        ownedSourceModelAsset->Indices16 = nullptr;
        ownedSourceModelAsset->Indices32 = nullptr;
        ownedSourceModelAsset->Submeshes = nullptr;
        runtimeModel->OwnedSourceModelAsset = nullptr;

        if (submeshes != nullptr) {
            for (int32_t submeshIndex = 0; submeshIndex < submeshes->get_Length(); submeshIndex++) {
                delete (*submeshes)[submeshIndex];
            }

            delete submeshes;
        }

        delete ownedSourceModelAsset;
    }
}
