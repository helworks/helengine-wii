namespace helengine.wii.builder.tests;

/// <summary>
/// Guards the packaged-disc Wii runtime source contract.
/// </summary>
public sealed class WiiRuntimeSourceTests {
    /// <summary>
    /// Ensures the Wii runtime keeps a direct-DOL developer boot path instead of forcing packaged-disc startup for every emulator launch.
    /// </summary>
    [Fact]
    public void BootModeSplit_KeepsDirectDolDeveloperBootAlongsidePackagedDiscBoot() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string makefileSource = File.ReadAllText(Path.Combine(repositoryRootPath, "Makefile"));
        string bootstrapHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiSceneBootstrap.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));

        Assert.Contains("HELENGINE_WII_BOOT_MODE", makefileSource, StringComparison.Ordinal);
        Assert.Contains("HELENGINE_WII_PACKAGED_DISC_BOOT", makefileSource, StringComparison.Ordinal);
        Assert.Contains("static std::string GetValidatedContentRootPath();", bootstrapHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static RuntimeSceneCatalog* CreateSceneCatalog();", bootstrapHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static std::string GetStartupSceneId();", bootstrapHeaderSource, StringComparison.Ordinal);
        Assert.Contains("#if HELENGINE_WII_PACKAGED_DISC_BOOT", applicationSource, StringComparison.Ordinal);
        Assert.Contains("WiiSceneBootstrap::GetValidatedContentRootPath()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("WiiSceneBootstrap::CreateSceneCatalog()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("WiiSceneBootstrap::GetStartupSceneId()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("WiiSceneBootstrap::InitializePackagedStorage()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("WiiSceneBootstrap::GetPackagedContentRootPath()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("WiiSceneBootstrap::CreatePackagedSceneCatalog()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("WiiSceneBootstrap::GetPackagedStartupSceneId()", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii scene bootstrap mirrors the GameCube packaged-disc contract instead of the earlier loose direct-DOL path.
    /// </summary>
    [Fact]
    public void PackagedBootstrap_DeclaresPackagedDiscHelpers() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string bootstrapHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiSceneBootstrap.hpp"));
        string bootstrapSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiSceneBootstrap.cpp"));

        Assert.Contains("static std::string StartupSceneId;", bootstrapHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static bool InitializePackagedStorage();", bootstrapHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static std::string GetPackagedContentRootPath();", bootstrapHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static RuntimeSceneCatalog* CreatePackagedSceneCatalog();", bootstrapHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static std::string GetPackagedStartupSceneId();", bootstrapHeaderSource, StringComparison.Ordinal);
        Assert.Contains("std::string WiiSceneBootstrap::StartupSceneId = \"Scenes/DemoDiscMainMenu.helen\";", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("bool WiiSceneBootstrap::InitializePackagedStorage()", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("DVD_Init();", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("DVD_MountAsync(", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("DVD_GetDriveStatus()", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("DI_Init()", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("DI_OpenPartition", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("DI_OpenPartition(partitionOffset >> 2U)", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("WiiDiscFileSystem::ConfigurePartitionDataOffset", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("DVD_ReadAbsAsyncPrio", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("0x40000U", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("0x2B8U", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("std::string WiiSceneBootstrap::GetPackagedContentRootPath()", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("return \"dvd:/\";", bootstrapSource, StringComparison.Ordinal);
        Assert.DoesNotContain("return \".\";", bootstrapSource, StringComparison.Ordinal);

        string discFileSystemHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiDiscFileSystem.hpp"));
        string discFileSystemSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiDiscFileSystem.cpp"));
        Assert.Contains("static void ConfigurePartitionDataOffset(uint32_t partitionDataOffset);", discFileSystemHeaderSource, StringComparison.Ordinal);
        Assert.Contains("PartitionDataOffset = partitionDataOffset;", discFileSystemSource, StringComparison.Ordinal);
        Assert.Contains("DI_Read(", discFileSystemSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii packaged runtime keeps the builder-emitted manifest path and startup-scene queue.
    /// </summary>
    [Fact]
    public void PackagedBootstrap_UsesManifestCatalogAndStartupSceneQueue() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string bootstrapSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiSceneBootstrap.cpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));

        Assert.Contains("he_get_runtime_wii_scene_entries(&entryCount)", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] Runtime manifest entry count:", bootstrapSource, StringComparison.Ordinal);
        Assert.Contains("WiiSceneBootstrap::InitializePackagedStorage()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] Runtime content root initialization failed.", applicationSource, StringComparison.Ordinal);
        Assert.Contains("WiiSceneBootstrap::GetPackagedContentRootPath()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("WiiSceneBootstrap::CreatePackagedSceneCatalog()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("WiiSceneBootstrap::GetPackagedStartupSceneId()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneManager()->LoadScene(packagedStartupSceneId, SceneLoadMode::Single);", applicationSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] Runtime startup scene queued.", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the direct-DOL Wii runtime reports the queued startup scene and the first generated scene/content trace state needed to prove authored scene loading.
    /// </summary>
    [Fact]
    public void DirectDolBootstrap_ReportsStartupSceneAndGeneratedSceneLoadTrace() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));

        Assert.Contains("SYS_Report(\"[Wii] Startup scene id: %s\\n\", startupSceneId.c_str());", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneManager()->get_LastTraceStage()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneManager()->get_LastTraceSceneId()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneManager()->get_LastTraceLoadedSceneCount()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneManager()->get_LastTracePendingOperationCount()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneLoadService()->get_LastTraceStage()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneLoadService()->get_LastTextLoadStage()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneLoadService()->get_LastTextFontRelativePath()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneLoadService()->get_LastTextureRelativePath()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] SceneManager trace stage=", applicationSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] SceneLoad trace stage=", applicationSource, StringComparison.Ordinal);
        Assert.Contains("[WiiFile] SceneManager trace stage=", applicationSource, StringComparison.Ordinal);
        Assert.Contains("[WiiFile] SceneLoad trace stage=", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii 2D bridge accepts already cooked GX RGB5A3 textures instead of requiring runtime RGBA32 transcoding.
    /// </summary>
    [Fact]
    public void PackagedGpuText_DeclaresWiiRuntimeTextureAndDirectGxRgb5A3UploadPath() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string runtimeTextureHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRuntimeTexture.hpp");
        string runtimeTextureSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRuntimeTexture.cpp");
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.cpp"));

        Assert.True(File.Exists(runtimeTextureHeaderPath), "Expected WiiRuntimeTexture.hpp to exist.");
        Assert.True(File.Exists(runtimeTextureSourcePath), "Expected WiiRuntimeTexture.cpp to exist.");

        string runtimeTextureHeaderSource = File.ReadAllText(runtimeTextureHeaderPath);
        string runtimeTextureSource = File.ReadAllText(runtimeTextureSourcePath);

        Assert.Contains("class WiiRuntimeTexture final : public RuntimeTexture", runtimeTextureHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void LoadFromRaw(TextureAsset* data);", runtimeTextureHeaderSource, StringComparison.Ordinal);
        Assert.Contains("GXTexObj* GetNativeTextureObject();", runtimeTextureHeaderSource, StringComparison.Ordinal);
        Assert.Contains("TextureAssetColorFormat::GxRgb5A3", runtimeTextureSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Wii text proof-of-life currently supports only RGBA32 packaged font atlas textures.", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("GX_InitTexObj(", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("GX_InitTexObjFilterMode(", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("DCFlushRange(", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] WiiRuntimeTexture::LoadFromRaw width=%u height=%u format=%d colors=%p palette=%p\\n", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] WiiRuntimeTexture::LoadFromRaw using prepacked GxRgb5A3 path.\\n", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] LoadPrepackedRgb5A3 padded=%ux%u expectedBytes=%u actualBytes=%d\\n", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] LoadPrepackedRgb5A3 upload completed native=%ux%u\\n", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("RuntimeTexture* WiiRenderManager2D::BuildTextureFromRaw(TextureAsset* data)", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("new WiiRuntimeTexture()", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeTexture->LoadFromRaw(data);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] BuildTextureFromRaw width=%u height=%u format=%d colors=%p palette=%p\\n", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] BuildTextureFromRaw completed.\\n", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("texture->Dispose();", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("delete texture;", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("RuntimeTexture* WiiRenderManager2D::BuildTextureFromCooked(std::string cookedAssetPath)", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("File::OpenRead(cookedAssetPath.c_str())", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("AssetSerializer::Deserialize(stream)", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("he_cpp_try_cast<TextureAsset>(asset)", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("ReleaseTransientTextureAsset(textureAsset);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] BuildTextureFromCooked open path=%s\\n", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] BuildTextureFromCooked deserialize begin.\\n", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("[Wii] BuildTextureFromCooked deserialize completed asset=%p\\n", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("colors != Array<uint8_t>::Empty()", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("paletteColors != Array<uint8_t>::Empty()", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Wii generated-core boot does not support cooked texture loading yet.", renderManagerSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii packaged-disc text proof renders captured glyphs through a native GX text pass.
    /// </summary>
    [Fact]
    public void PackagedGpuText_RendersQueuedGlyphsThroughNativeGxPass() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.hpp"));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.cpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));

        Assert.Contains("void RenderCapturedCommands(uint16_t logicalFrameWidth, uint16_t logicalFrameHeight, uint16_t physicalFrameWidth, uint16_t physicalFrameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void ExecuteGlyphQuadCommand(RenderCommandList2D* commandList, int32_t payloadIndex, uint16_t logicalFrameWidth, uint16_t logicalFrameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("GetGlyphQuadTexture(", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("GetGlyphQuadBounds(", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("GX_LoadTexObj(", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("GX_Begin(GX_QUADS, GX_VTXFMT0, 4);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("DidSubmitGlyph = true;", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager2D->RenderCapturedCommands(", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->Draw();", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii 2D renderer now uses the shared command-list path for non-text UI, including clip transitions.
    /// </summary>
    [Fact]
    public void PackagedGpuText_UsesSharedCommandListAndClipAwareNonTextUiPath() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));
        string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.hpp"));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.cpp"));

        Assert.Contains("RenderCapturedCommands(", applicationSource, StringComparison.Ordinal);
        Assert.Contains("RenderCommandList2D", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("RenderCommandListBuilder2D", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void RenderCapturedCommands(uint16_t logicalFrameWidth, uint16_t logicalFrameHeight, uint16_t physicalFrameWidth, uint16_t physicalFrameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void ExecuteCommandList(RenderCommandList2D* commandList, uint16_t logicalFrameWidth, uint16_t logicalFrameHeight, uint16_t physicalFrameWidth, uint16_t physicalFrameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void ExecuteRoundedRectCommand(RenderCommandList2D* commandList, int32_t payloadIndex, uint16_t logicalFrameWidth, uint16_t logicalFrameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void ApplyClipRect(const float4& clipRect, uint16_t logicalFrameWidth, uint16_t logicalFrameHeight, uint16_t physicalFrameWidth, uint16_t physicalFrameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("#include \"RenderCommand2DType.hpp\"", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("#include \"RenderCommandList2D.hpp\"", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("#include \"RenderCommandListBuilder2D.hpp\"", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("RenderCommandListBuilder2D commandListBuilder {};", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("ExecuteCommandList(commandList, logicalFrameWidth, logicalFrameHeight, physicalFrameWidth, physicalFrameHeight);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("case RenderCommand2DType::ClipPush:", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("case RenderCommand2DType::ClipPop:", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("case RenderCommand2DType::TexturedQuad:", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("case RenderCommand2DType::GlyphQuad:", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("case RenderCommand2DType::RoundedRect:", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("GX_SetScissor(", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("void RenderCapturedText(uint16_t frameWidth, uint16_t frameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii 3D bridge can rebuild shared runtime models, retain authored mesh data, and draw indexed GX geometry instead of throwing generated-boot stub errors.
    /// </summary>
    [Fact]
    public void Packaged3D_BuildsRuntimeModelsAndCookedPlatformOwnedMaterials() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));
        string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager3D.hpp"));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager3D.cpp"));
        string runtimeModelHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRuntimeModel.hpp"));
        string runtimeModelSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRuntimeModel.cpp"));

        Assert.Contains("class WiiRenderManager3D : public RenderManager3D {", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("RuntimeModel* BuildModelFromRaw(ModelAsset* data) override;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath) override;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("RuntimeMaterial* BuildMaterialFromCooked(PlatformMaterialAsset* materialAsset);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void SetPresentedFrameSize(uint16_t width, uint16_t height);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("WiiSceneRenderBridge* SceneRenderBridge;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("WiiMeshCache* MeshCache;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("WiiRasterRenderer* RasterRenderer;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("std::vector<RuntimeMaterial*> ReleasedMaterials;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("std::vector<RuntimeModel*> ReleasedModels;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void UpdatePresentedClearColor(WiiFramePlan* framePlan);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static MaterialRenderState* BuildMaterialRenderState(PlatformMaterialAsset* materialAsset);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static void ReleaseTransientModelAsset(ModelAsset* asset);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void ReleaseOwnedSourceModelAsset(WiiRuntimeModel* runtimeModel);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager3D->SetPresentedFrameSize(static_cast<uint16_t>(RenderMode->fbWidth), static_cast<uint16_t>(RenderMode->efbHeight));", applicationSource, StringComparison.Ordinal);
        Assert.Contains("ModelAssetIndexData* indexData = ModelAssetIndexData::Resolve(data);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("WiiRuntimeModel* runtimeModel = new WiiRuntimeModel();", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeModel->set_Id(data->get_Id());", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeModel->SetBounds(data->BoundsMin, data->BoundsMax);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeModel->SetSubmeshes(ModelSubmeshResolver::BuildRuntimeSubmeshes(data));", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeModel->Positions = data->Positions;", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeModel->Normals = data->Normals;", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeModel->TexCoords = data->TexCoords;", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeModel->Indices16 = indexData->get_Indices16();", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeModel->Indices32 = indexData->get_Indices32();", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeModel->Uses32BitIndices = indexData->get_Uses32BitIndices();", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("WiiFramePlan* framePlan = SceneRenderBridge->BuildFramePlan(", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("HasRenderedSceneValue = RasterRenderer->DrawFrame(framePlan);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("ReleasedMaterials.push_back(material);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("ReleasedModels.push_back(model);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("delete RasterRenderer;", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("delete MeshCache;", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("delete SceneRenderBridge;", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("File::OpenRead(cookedAssetPath.c_str())", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("AssetSerializer::Deserialize(stream)", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("he_cpp_try_cast<ModelAsset>(asset)", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeModel->OwnedSourceModelAsset = cookedModelAsset;", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("PlatformMaterialAsset* materialAsset = he_cpp_try_cast<PlatformMaterialAsset>(asset);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("return BuildMaterialFromCooked(materialAsset);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeMaterial->SetRenderState(BuildMaterialRenderState(materialAsset));", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("renderState->set_CullMode(materialAsset->DoubleSided", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("renderState->set_BlendMode(materialAsset->BaseColorA < 0xFF", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("GXColor WiiRenderManager3D::ToGxColor(float4 color)", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("RenderFrameExtractionService extractor;", File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiSceneRenderBridge.cpp")), StringComparison.Ordinal);
        Assert.Contains("WiiRuntimeModel* typedRuntimeModel = he_cpp_try_cast<WiiRuntimeModel>(runtimeModel);", File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiMeshCache.cpp")), StringComparison.Ordinal);
        Assert.Contains("GX_SetViewport(framePlan->PhysicalViewport.X", File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRasterRenderer.cpp")), StringComparison.Ordinal);
        Assert.Contains("GX_LoadProjectionMtx(projectionMatrix, GX_PERSPECTIVE);", File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRasterRenderer.cpp")), StringComparison.Ordinal);
        Assert.Contains("GX_Position3f32(cachedPosition.X, cachedPosition.Y, cachedPosition.Z);", File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRasterRenderer.cpp")), StringComparison.Ordinal);
        Assert.Contains("GX_Color4u8(OpaqueMeshColorRed, OpaqueMeshColorGreen, OpaqueMeshColorBlue, OpaqueMeshColorAlpha);", File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRasterRenderer.cpp")), StringComparison.Ordinal);
        Assert.Contains("GX_SetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);", File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp")), StringComparison.Ordinal);
        Assert.Contains("class WiiRuntimeModel final : public RuntimeModel {", runtimeModelHeaderSource, StringComparison.Ordinal);
        Assert.Contains("Array<float3>* Positions;", runtimeModelHeaderSource, StringComparison.Ordinal);
        Assert.Contains("Array<float3>* Normals;", runtimeModelHeaderSource, StringComparison.Ordinal);
        Assert.Contains("Array<float2>* TexCoords;", runtimeModelHeaderSource, StringComparison.Ordinal);
        Assert.Contains("Array<uint16_t>* Indices16;", runtimeModelHeaderSource, StringComparison.Ordinal);
        Assert.Contains("Array<uint32_t>* Indices32;", runtimeModelHeaderSource, StringComparison.Ordinal);
        Assert.Contains("bool Uses32BitIndices;", runtimeModelHeaderSource, StringComparison.Ordinal);
        Assert.Contains("WiiCachedMeshData* CachedMeshData;", runtimeModelHeaderSource, StringComparison.Ordinal);
        Assert.Contains("ModelAsset* OwnedSourceModelAsset;", runtimeModelHeaderSource, StringComparison.Ordinal);
        Assert.Contains("WiiRuntimeModel::WiiRuntimeModel()", runtimeModelSource, StringComparison.Ordinal);
        Assert.Contains("Positions(nullptr)", runtimeModelSource, StringComparison.Ordinal);
        Assert.Contains("CachedMeshData(nullptr)", runtimeModelSource, StringComparison.Ordinal);
        Assert.Contains("OwnedSourceModelAsset(nullptr)", runtimeModelSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Wii generated-core boot does not support raw model creation yet.", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Wii generated-core boot does not support cooked model loading yet.", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Wii generated-core boot does not support material creation yet.", renderManagerSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures presented Wii frames expose the authored platform version string and honor the active camera clear color instead of the old diagnostic steady-state clears.
    /// </summary>
    [Fact]
    public void PackagedGpuText_UsesAuthoredPlatformVersionAndCameraClearColorForPresentedFrames() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));
        string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager3D.hpp"));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager3D.cpp"));

        Assert.Contains("new PlatformInfo(\"wii\", \"1.0\")", applicationSource, StringComparison.Ordinal);
        Assert.Contains("bool HasPresentedClearColor() const;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("GXColor GetPresentedClearColor() const;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void UpdatePresentedClearColor(WiiFramePlan* framePlan);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static GXColor ToGxColor(float4 color);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static uint8_t ConvertNormalizedColorChannel(float value);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("UpdatePresentedClearColor(framePlan);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("CameraClearSettings clearSettings = framePlan->Camera->get_ClearSettings();", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("if (!clearSettings.get_ClearColorEnabled())", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("PresentedClearColor = ToGxColor(clearSettings.get_ClearColor());", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("PresentedClearColorValid = true;", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("if (EngineRenderManager3D != nullptr && EngineRenderManager3D->HasPresentedClearColor())", applicationSource, StringComparison.Ordinal);
        Assert.Contains("return EngineRenderManager3D->GetPresentedClearColor();", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("return GXColor { 0x00, 0x80, 0x80, 0xFF };", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("return GXColor { 0xC0, 0xC0, 0x00, 0xFF };", applicationSource, StringComparison.Ordinal);
        Assert.DoesNotContain("return GXColor { 0x00, 0x60, 0xA0, 0xFF };", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii host exposes a widescreen logical window to the shared UI layout system while keeping GX presentation and scissor bounds on the physical framebuffer.
    /// </summary>
    [Fact]
    public void PackagedGpuText_UsesAspectAwareLogicalWindowSizeAndPhysicalFramebufferScissor() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.hpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));
        string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.hpp"));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.cpp"));

        Assert.Contains("#include <ogc/conf.h>", applicationSource, StringComparison.Ordinal);
        Assert.Contains("bool IsWidescreenAspectEnabled() const;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("uint16_t ResolveLogicalFrameWidth() const;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("uint16_t ResolveLogicalFrameHeight() const;", applicationHeaderSource, StringComparison.Ordinal);
        Assert.Contains("return CONF_GetAspectRatio() == CONF_ASPECT_16_9;", applicationSource, StringComparison.Ordinal);
        Assert.Contains("return static_cast<uint16_t>(((static_cast<uint32_t>(RenderMode->efbHeight) * 16U) + 8U) / 9U);", applicationSource, StringComparison.Ordinal);
        Assert.Contains("const uint16_t logicalFrameWidth = ResolveLogicalFrameWidth();", applicationSource, StringComparison.Ordinal);
        Assert.Contains("const uint16_t logicalFrameHeight = ResolveLogicalFrameHeight();", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager3D->AddWindow(0, logicalFrameWidth, logicalFrameHeight);", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager2D->RenderCapturedCommands(", applicationSource, StringComparison.Ordinal);
        Assert.Contains("logicalFrameWidth", applicationSource, StringComparison.Ordinal);
        Assert.Contains("logicalFrameHeight", applicationSource, StringComparison.Ordinal);
        Assert.Contains("void RenderCapturedCommands(uint16_t logicalFrameWidth, uint16_t logicalFrameHeight, uint16_t physicalFrameWidth, uint16_t physicalFrameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("const float horizontalScale = static_cast<float>(physicalFrameWidth) / static_cast<float>(logicalFrameWidth);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("GX_SetScissor(0U, 0U, physicalFrameWidth, physicalFrameHeight);", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("PresentedFrameHeight", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii input backend polls both Wii Remote and GameCube controller paths and maps sideways Wii Remote buttons into the shared logical gamepad contract.
    /// </summary>
    [Fact]
    public void PackagedInput_UsesWiiRemoteSidewaysMappingWithGameCubeFallback() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string inputHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiInputManager.hpp"));
        string inputSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiInputManager.cpp"));

        Assert.Contains("#include <wiiuse/wpad.h>", inputSource, StringComparison.Ordinal);
        Assert.Contains("WPAD_Init();", inputSource, StringComparison.Ordinal);
        Assert.Contains("WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS);", inputSource, StringComparison.Ordinal);
        Assert.Contains("WPAD_ScanPads();", inputSource, StringComparison.Ordinal);
        Assert.Contains("const u32 wiiButtonsHeld = WPAD_ButtonsHeld(WPAD_CHAN_0);", inputSource, StringComparison.Ordinal);
        Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::DPadUp, (wiiButtonsHeld & WPAD_BUTTON_UP) != 0);", inputSource, StringComparison.Ordinal);
        Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::DPadDown, (wiiButtonsHeld & WPAD_BUTTON_DOWN) != 0);", inputSource, StringComparison.Ordinal);
        Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::DPadLeft, (wiiButtonsHeld & WPAD_BUTTON_LEFT) != 0);", inputSource, StringComparison.Ordinal);
        Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::DPadRight, (wiiButtonsHeld & WPAD_BUTTON_RIGHT) != 0);", inputSource, StringComparison.Ordinal);
        Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::South, (wiiButtonsHeld & WPAD_BUTTON_2) != 0);", inputSource, StringComparison.Ordinal);
        Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::East, (wiiButtonsHeld & WPAD_BUTTON_1) != 0);", inputSource, StringComparison.Ordinal);
        Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::Start, (wiiButtonsHeld & WPAD_BUTTON_PLUS) != 0);", inputSource, StringComparison.Ordinal);
        Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::Select, (wiiButtonsHeld & WPAD_BUTTON_MINUS) != 0);", inputSource, StringComparison.Ordinal);
        Assert.Contains("const u16 heldButtons = PAD_ButtonsHeld(0);", inputSource, StringComparison.Ordinal);
        Assert.Contains("PAD_ScanPads();", inputSource, StringComparison.Ordinal);
        Assert.Contains("InputFrameState CaptureFrame() override;", inputHeaderSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures Wii startup installs standard-platform Accept and Return bindings so the authored menu can confirm and navigate back through the shared action layer.
    /// </summary>
    [Fact]
    public void PackagedInput_ConfiguresStandardAcceptAndReturnBindingsForMenuActions() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));

        Assert.Contains("#include \"StandardPlatformInputConfiguration.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"StandardPlatformActionBinding.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"StandardPlatformAction.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"InputControlId.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"InputDeviceKind.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include \"InputControlKind.hpp\"", applicationSource, StringComparison.Ordinal);
        Assert.Contains("options->StandardPlatformInputConfiguration = new StandardPlatformInputConfiguration(", applicationSource, StringComparison.Ordinal);
        Assert.Contains("new StandardPlatformActionBinding(StandardPlatformAction::Accept, InputControlId(InputDeviceKind::Gamepad, InputControlKind::Button, 0, static_cast<int32_t>(InputGamepadButton::South)))", applicationSource, StringComparison.Ordinal);
        Assert.Contains("new StandardPlatformActionBinding(StandardPlatformAction::Return, InputControlId(InputDeviceKind::Gamepad, InputControlKind::Button, 0, static_cast<int32_t>(InputGamepadButton::East)))", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures Wii textured UI quads remap logical UVs onto GC-style 4-pixel padded GX texture dimensions instead of power-of-two expansion.
    /// </summary>
    [Fact]
    public void PackagedGpuText_UsesFourPixelPaddedNativeTextureDimensionsForSpriteQuads() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.cpp"));
        string runtimeTextureHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRuntimeTexture.hpp"));
        string runtimeTextureSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRuntimeTexture.cpp"));

        Assert.Contains("uint32_t GetNativeTextureWidth() const;", runtimeTextureHeaderSource, StringComparison.Ordinal);
        Assert.Contains("uint32_t GetNativeTextureHeight() const;", runtimeTextureHeaderSource, StringComparison.Ordinal);
        Assert.Contains("const uint32_t nativeWidth = (width + 3U) & ~3U;", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("const uint32_t nativeHeight = (height + 3U) & ~3U;", runtimeTextureSource, StringComparison.Ordinal);
        Assert.DoesNotContain("ResolveTextureDimension(uint32_t logicalDimension)", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("texture->GetNativeTextureWidth()", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("texture->GetNativeTextureHeight()", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("sourceRect.X * logicalWidth / nativeWidth", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("sourceRect.W * logicalHeight / nativeHeight", renderManagerSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii text pass visits every enabled camera queue instead of stopping after the first enabled camera.
    /// </summary>
    [Fact]
    public void PackagedGpuText_VisitsAllEnabledCameras() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.cpp"));

        Assert.DoesNotContain("camera->get_RenderQueue2D()->VisitOrdered(this);\r\n            return;", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("camera->get_RenderQueue2D()->VisitOrdered(this);\n            return;", renderManagerSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii packaged disc file bridge exists and the generated Wii file layer routes rooted <c>dvd:/</c> reads through it.
    /// </summary>
    [Fact]
    public void PackagedBootstrap_UsesWiiDiscFileSystemBridge() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string discFileSystemHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiDiscFileSystem.hpp");
        string discFileSystemSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiDiscFileSystem.cpp");
        string platformDefinitionFactoryPath = Path.Combine(repositoryRootPath, "builder", "WiiPlatformDefinitionFactory.cs");

        Assert.True(File.Exists(discFileSystemHeaderPath), "Expected WiiDiscFileSystem.hpp to exist.");
        Assert.True(File.Exists(discFileSystemSourcePath), "Expected WiiDiscFileSystem.cpp to exist.");
        Assert.True(File.Exists(platformDefinitionFactoryPath), "Expected WiiPlatformDefinitionFactory.cs to exist.");

        string discFileSystemHeaderSource = File.ReadAllText(discFileSystemHeaderPath);
        string discFileSystemSource = File.ReadAllText(discFileSystemSourcePath);
        string platformDefinitionFactorySource = File.ReadAllText(platformDefinitionFactoryPath);

        Assert.Contains("class WiiDiscFileSystem", discFileSystemHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static bool CanHandlePath(const char* path);", discFileSystemHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static bool Exists(const char* path);", discFileSystemHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static FileStream* OpenRead(const char* path);", discFileSystemHeaderSource, StringComparison.Ordinal);
        Assert.Contains("DI_Read(", discFileSystemSource, StringComparison.Ordinal);
        Assert.Contains("<< 2U", discFileSystemSource, StringComparison.Ordinal);
        Assert.Contains("native-file-system-header", platformDefinitionFactorySource, StringComparison.Ordinal);
        Assert.Contains("WiiNativeFileSystemHeader = \"\\\"platform/wii/WiiDiscFileSystem.hpp\\\"\"", platformDefinitionFactorySource, StringComparison.Ordinal);
        Assert.Contains("native-file-system-type", platformDefinitionFactorySource, StringComparison.Ordinal);
        Assert.Contains("helengine::wii::WiiDiscFileSystem", platformDefinitionFactorySource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures Wii packaged-disc debugging uses one explicit launcher that targets an existing ISO and a seeded Dolphin logging profile.
    /// </summary>
    [Fact]
    public void PackagedDebugLauncher_UsesExplicitIsoAndSeededLoggingProfile() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string launcherPath = Path.Combine(repositoryRootPath, "tmp", "launch_wii_packaged_debug_session.ps1");

        Assert.True(File.Exists(launcherPath), "Expected tmp/launch_wii_packaged_debug_session.ps1 to exist.");

        string launcherSource = File.ReadAllText(launcherPath);

        Assert.Contains("[Parameter(Mandatory = $true)]", launcherSource, StringComparison.Ordinal);
        Assert.Contains("[string]$ArtifactPath", launcherSource, StringComparison.Ordinal);
        Assert.Contains("Get-Process -Name 'Dolphin'", launcherSource, StringComparison.Ordinal);
        Assert.Contains("Logger.ini", launcherSource, StringComparison.Ordinal);
        Assert.Contains("Wii", launcherSource, StringComparison.Ordinal);
        Assert.Contains("Backup", launcherSource, StringComparison.Ordinal);
        Assert.Contains("ResourcePacks", launcherSource, StringComparison.Ordinal);
        Assert.Contains("$globalLoggerPath = Join-Path $globalProfileRoot 'Config\\Logger.ini'", launcherSource, StringComparison.Ordinal);
        Assert.Contains("Get-Content -LiteralPath $globalLoggerPath -Raw", launcherSource, StringComparison.Ordinal);
        Assert.Contains("WriteToConsole = True", launcherSource, StringComparison.Ordinal);
        Assert.Contains("WriteToFile = True", launcherSource, StringComparison.Ordinal);
        Assert.Contains("WriteToWindow = True", launcherSource, StringComparison.Ordinal);
        Assert.Contains("logvisible=true", launcherSource, StringComparison.Ordinal);
        Assert.Contains("logconfigvisible=true", launcherSource, StringComparison.Ordinal);
        Assert.Contains("Start-Process", launcherSource, StringComparison.Ordinal);
        Assert.Contains("'-u', $userDir, '-e', $resolvedArtifactPath", launcherSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the packaged-disc Wii runtime exposes temporary host-visible diagnostics for the menu text path while debugging.
    /// </summary>
    [Fact]
    public void PackagedGpuText_ExposesHostVisibleDiagnosticsForMenuTextPath() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));
        string renderManagerHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.hpp"));
        string renderManagerSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiRenderManager2D.cpp"));

        Assert.Contains("get_LastTextLoadStage()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("get_LastTextFontRelativePath()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("get_LastTextureRelativePath()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("get_VisitedCameraCount()", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("get_VisitedDrawableCount()", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("get_QueuedTextCount()", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("get_DidSubmitGlyph()", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("DrawSolidQuad2D(8.0f, 8.0f, 24.0f, 24.0f,", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("DrawSolidQuad2D(40.0f, 8.0f, 24.0f, 24.0f,", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("DrawSolidQuad2D(0.0f, 0.0f, static_cast<float>(frameWidth), 40.0f,", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("DrawSolidQuad2D(0.0f, static_cast<float>(frameHeight) - 32.0f, static_cast<float>(frameWidth), 32.0f,", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("DidSubmitGlyph = true;", renderManagerSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Wii runtime enables Dolphin OSReport stdio redirection before the packaged-disc diagnostics begin.
    /// </summary>
    [Fact]
    public void PackagedDebugLogging_EnablesDolphinOsReportStdioBridge() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string mainSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "main.cpp"));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));

        Assert.Contains("static struct __argv NullSafeSystemArgv = {};", mainSource, StringComparison.Ordinal);
        Assert.Contains("extern \"C\" void __CheckARGV()", mainSource, StringComparison.Ordinal);
        Assert.Contains("__system_argv = &NullSafeSystemArgv;", mainSource, StringComparison.Ordinal);
        Assert.Contains("SYS_STDIO_Report(true);", mainSource, StringComparison.Ordinal);
        Assert.Contains("std::fprintf(stderr, \"[Wii] stderr bridge armed.\\n\");", mainSource, StringComparison.Ordinal);
        Assert.Contains("std::fflush(stderr);", mainSource, StringComparison.Ordinal);
        Assert.Contains("SYS_Report(\"[Wii] main() entered.\\n\");", mainSource, StringComparison.Ordinal);
        Assert.True(
            mainSource.IndexOf("SYS_STDIO_Report(true);", StringComparison.Ordinal) < mainSource.IndexOf("ISFS_Initialize()", StringComparison.Ordinal),
            "Expected SYS_STDIO_Report(true) to run before the title-data ISFS startup probe.");
        Assert.True(
            mainSource.IndexOf("SYS_Report(\"[Wii] main() entered.\\n\");", StringComparison.Ordinal) < mainSource.IndexOf("ISFS_Initialize()", StringComparison.Ordinal),
            "Expected the earliest main() OSReport to run before the title-data ISFS startup probe.");
        Assert.Contains("SYS_Report(\"[Wii] InitializeEngineCore begin.\\n\");", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the packaged-disc Wii runtime also writes its trace stream into an emulated NAND path that Dolphin mirrors back into the isolated user directory.
    /// </summary>
    [Fact]
    public void PackagedDebugLogging_WritesRuntimeTraceIntoHostReadableIsfsPath() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string applicationSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiApplication.cpp"));

        Assert.Contains("#include <ogc/dvd.h>", applicationSource, StringComparison.Ordinal);
        Assert.Contains("#include <ogc/isfs.h>", applicationSource, StringComparison.Ordinal);
        Assert.Contains("/title/00010000/%02X%02X%02X%02X/data/%s", applicationSource, StringComparison.Ordinal);
        Assert.Contains("DVD_GetCurrentDiskID()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("ISFS_Initialize()", applicationSource, StringComparison.Ordinal);
        Assert.Contains("ISFS_CreateDir(", applicationSource, StringComparison.Ordinal);
        Assert.Contains("ISFS_CreateFile(", applicationSource, StringComparison.Ordinal);
        Assert.Contains("ISFS_Open(", applicationSource, StringComparison.Ordinal);
        Assert.Contains("ISFS_Write(", applicationSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the packaged-disc Wii entrypoint emits one earliest-possible ISFS startup probe in the title save-data directory so packaged boot can be distinguished from later runtime logging failures.
    /// </summary>
    [Fact]
    public void PackagedDebugLogging_WritesMainEntryProbeIntoTitleDataIsfsPath() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string mainSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "main.cpp"));

        Assert.Contains("#include <ogc/dvd.h>", mainSource, StringComparison.Ordinal);
        Assert.Contains("#include <ogc/isfs.h>", mainSource, StringComparison.Ordinal);
        Assert.Contains("/title/00010000/%02X%02X%02X%02X/data/%s", mainSource, StringComparison.Ordinal);
        Assert.Contains("DVD_GetCurrentDiskID()", mainSource, StringComparison.Ordinal);
        Assert.Contains("ISFS_Initialize()", mainSource, StringComparison.Ordinal);
        Assert.Contains("ISFS_CreateDir(", mainSource, StringComparison.Ordinal);
        Assert.Contains("ISFS_CreateFile(", mainSource, StringComparison.Ordinal);
        Assert.Contains("ISFS_Open(", mainSource, StringComparison.Ordinal);
        Assert.Contains("ISFS_Write(", mainSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the packaged-disc Wii entrypoint no longer mounts <c>sd:/</c> during startup because the old libogc SD trace probe trips argv invalid-read warnings before normal runtime boot.
    /// </summary>
    [Fact]
    public void PackagedDebugLogging_DoesNotMountSdTraceProbeDuringStartup() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string mainSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "main.cpp"));

        Assert.DoesNotContain("#include <fat.h>", mainSource, StringComparison.Ordinal);
        Assert.DoesNotContain("fatInitDefault();", mainSource, StringComparison.Ordinal);
        Assert.DoesNotContain("sd:/main_entry_trace.txt", mainSource, StringComparison.Ordinal);
        Assert.DoesNotContain("std::fopen(\"sd:/main_entry_trace.txt\", \"ab\")", mainSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the packaged disc file bridge indexes the apploader-loaded FST from Wii low memory instead of rereading <c>boot.bin</c> through the opened partition.
    /// </summary>
    [Fact]
    public void PackagedDiscFileSystem_UsesApploaderLoadedFstFromLowMemory() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string source = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiDiscFileSystem.cpp"));
        Assert.Contains("static constexpr uint32_t FstAddressLowMemoryAddress = 0x80000038u;", source, StringComparison.Ordinal);
        Assert.Contains("static constexpr uint32_t FstSizeLowMemoryAddress = 0x8000003Cu;", source, StringComparison.Ordinal);
        Assert.Contains("const uint32_t fstAddress = *reinterpret_cast<volatile uint32_t*>(FstAddressLowMemoryAddress);", source, StringComparison.Ordinal);
        Assert.Contains("const uint32_t fstSize = *reinterpret_cast<volatile uint32_t*>(FstSizeLowMemoryAddress);", source, StringComparison.Ordinal);
        Assert.Contains("const uint8_t* fstBytes = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(fstAddress));", source, StringComparison.Ordinal);
        Assert.Contains("FstBytes.assign(fstBytes, fstBytes + fstSize);", source, StringComparison.Ordinal);
        Assert.Contains("bool CaptureFstSnapshotFromLowMemory() {", source, StringComparison.Ordinal);
        Assert.Contains("if (FstBytes.empty() && !CaptureFstSnapshotFromLowMemory()) {", source, StringComparison.Ordinal);
        Assert.DoesNotContain("ReadDiscRange(discHeader, 0U, DiscHeaderReadLength)", source, StringComparison.Ordinal);
        Assert.DoesNotContain("const uint32_t fstOffsetWords = ReadBigEndianU32(discHeader + 0x424);", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures packaged Wii content reads use the decrypted partition DI path with partition-data-relative byte offsets chunked into aligned sector-sized transfers.
    /// </summary>
    [Fact]
    public void PackagedDiscFileSystem_UsesPartitionDataRelativeDecryptedDiReads() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string source = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiDiscFileSystem.cpp"));

        Assert.Contains("static constexpr std::size_t MaxDiReadLength = 0x800U;", source, StringComparison.Ordinal);
        Assert.Contains("std::size_t remainingLength = length;", source, StringComparison.Ordinal);
        Assert.Contains("const std::size_t chunkLength = std::min(remainingLength, MaxDiReadLength);", source, StringComparison.Ordinal);
        Assert.Contains("const std::size_t wordOffset = currentOffset >> 2U;", source, StringComparison.Ordinal);
        Assert.Contains("const std::size_t alignedLength = Align32(chunkLength);", source, StringComparison.Ordinal);
        Assert.Contains("uint8_t* alignedBuffer = static_cast<uint8_t*>(memalign(32, alignedLength));", source, StringComparison.Ordinal);
        Assert.Contains("const int readResult = DI_Read(alignedBuffer, static_cast<u32>(alignedLength), static_cast<u32>(wordOffset));", source, StringComparison.Ordinal);
        Assert.Contains("std::memcpy(destinationBytes, alignedBuffer, chunkLength);", source, StringComparison.Ordinal);
        Assert.DoesNotContain("DVD_ReadPrio(", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures packaged Wii FST file-entry offsets continue to use the quarter-word byte units emitted in the Wii FST.
    /// </summary>
    [Fact]
    public void PackagedDiscFileSystem_ConvertsFileEntryOffsetsFromQuarterWordsToBytes() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string source = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiDiscFileSystem.cpp"));

        Assert.Contains("const uint32_t fileOffsetWords = ReadBigEndianU32(FstBytes.data() + entryOffset + 4);", source, StringComparison.Ordinal);
        Assert.Contains("const uint32_t fileOffset = fileOffsetWords << 2U;", source, StringComparison.Ordinal);
        Assert.Contains("fileOffset,", source, StringComparison.Ordinal);
        Assert.DoesNotContain("ReadBigEndianU32(FstBytes.data() + entryOffset + 4),", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the project-owned Wii apploader now requests the FST, writes the low-memory FST handoff fields, and reports its boot progress through Dolphin's apploader logger bridge.
    /// </summary>
    [Fact]
    public void PackagedBoot_ApploaderLoadsFstAndReportsThroughDolphinBridge() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string apploaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "apploader", "WiiDiscApploader.cpp"));
        string apploaderLinkerScript = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "apploader", "WiiDiscApploader.ld"));
        string apploaderImageBuilderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "builder", "WiiGeneratedApploaderImageBuilder.cs"));

        Assert.Contains("typedef void (*ApploaderReportFunction)(const char* format, ...);", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("__attribute__((section(\".data.apploader_state\"), used))", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("static ApploaderReportFunction ReportFunction = nullptr;", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("static constexpr uint32_t Bi2HeaderDiscOffsetWords = 0x110u;", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("static constexpr uint32_t BootInfoVersionLowMemoryAddress = 0x80000024u;", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("static constexpr uint32_t SimulatedMemorySizeLowMemoryAddress = 0x800000F0u;", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("static constexpr uint32_t Bi2LowMemoryAddress = 0x800000F4u;", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("static constexpr uint32_t DiscLayerStateLowMemoryAddress = 0x8000319Cu;", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("static volatile uint32_t Bi2HeaderWords[8] = {}", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("static volatile uint32_t Bi2HeaderLoaded = 0u;", apploaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("EnableHardcodedFirstRequestDiagnostic", apploaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("EnableDirectScratchWriteDiagnostic", apploaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("DolphinScratchDestinationAddress", apploaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("[HA] diag dst=", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("[HA] bi2 dst=", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("Config.FstLoadAddress", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("Config.FstSize", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("Config.FstDiscOffsetWords", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("*reinterpret_cast<volatile uint32_t*>(BootInfoVersionLowMemoryAddress) = 1u;", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("*reinterpret_cast<volatile uint32_t*>(SimulatedMemorySizeLowMemoryAddress)", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("*reinterpret_cast<volatile uint32_t*>(Bi2LowMemoryAddress)", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("*reinterpret_cast<volatile uint8_t*>(DiscLayerStateLowMemoryAddress) = 0x80u;", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("[HA] init requests=", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("[HA] dol[", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("[HA] fst dst=", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("[HA] close entry=", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("0x80000034u", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("0x80000038u", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("0x8000003Cu", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("static constexpr uint32_t ArenaHighReserveBytes = 0x20u;", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("Config.CurrentRequestIndex == Config.RequestCount", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("const uint EntryPointAddress = 0x81200000U;", apploaderImageBuilderSource, StringComparison.Ordinal);
        Assert.Contains(". = 0x81200000;", apploaderLinkerScript, StringComparison.Ordinal);
        Assert.Contains("reinterpret_cast<volatile uint32_t*>(ArenaHighLowMemoryAddress)", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("const uint32_t reservedArenaHighAddress = Config.FstLoadAddress >= ArenaHighReserveBytes", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("? (Config.FstLoadAddress - ArenaHighReserveBytes) & ~0x1Fu", apploaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("*reinterpret_cast<volatile uint32_t*>(ArenaHighLowMemoryAddress) = Config.FstLoadAddress;", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("*(.data*)", apploaderLinkerScript, StringComparison.Ordinal);
        Assert.Contains("*(.bss*)", apploaderLinkerScript, StringComparison.Ordinal);
    }
}
