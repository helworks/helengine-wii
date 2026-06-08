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
    /// Ensures the Wii 2D bridge can rebuild packaged font atlas textures into native GX runtime textures.
    /// </summary>
    [Fact]
    public void PackagedGpuText_DeclaresWiiRuntimeTextureAndRawTextureUploadPath() {
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
        Assert.Contains("TextureAssetColorFormat::Rgba32", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("GX_InitTexObj(", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("GX_InitTexObjFilterMode(", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("DCFlushRange(", runtimeTextureSource, StringComparison.Ordinal);
        Assert.Contains("RuntimeTexture* WiiRenderManager2D::BuildTextureFromRaw(TextureAsset* data)", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("new WiiRuntimeTexture()", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("runtimeTexture->LoadFromRaw(data);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("texture->Dispose();", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("delete texture;", renderManagerSource, StringComparison.Ordinal);
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

        Assert.Contains("void RenderCapturedText(uint16_t frameWidth, uint16_t frameHeight);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("TextLayoutUtils::WrapText(", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("TextLayoutAlignmentUtils::MeasureVisibleLineWidth(", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("if (line.empty()) {", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("lineOffsets.push_back(0.0);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("font->get_Texture()", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("GX_LoadTexObj(", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("GX_Begin(GX_QUADS, GX_VTXFMT0, 4);", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("TextQueue", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager2D->RenderCapturedText(", applicationSource, StringComparison.Ordinal);
        Assert.Contains("EngineCore->Draw();", applicationSource, StringComparison.Ordinal);
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
        string generatedFileSourcePath = Path.Combine(repositoryRootPath, "tmp", "generated-core-wii", "system", "io", "file.cpp");

        Assert.True(File.Exists(discFileSystemHeaderPath), "Expected WiiDiscFileSystem.hpp to exist.");
        Assert.True(File.Exists(discFileSystemSourcePath), "Expected WiiDiscFileSystem.cpp to exist.");

        string discFileSystemHeaderSource = File.ReadAllText(discFileSystemHeaderPath);
        string discFileSystemSource = File.ReadAllText(discFileSystemSourcePath);
        string generatedFileSource = File.ReadAllText(generatedFileSourcePath);

        Assert.Contains("class WiiDiscFileSystem", discFileSystemHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static bool CanHandlePath(const char* path);", discFileSystemHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static bool Exists(const char* path);", discFileSystemHeaderSource, StringComparison.Ordinal);
        Assert.Contains("static FileStream* OpenRead(const char* path);", discFileSystemHeaderSource, StringComparison.Ordinal);
        Assert.Contains("DI_Read(", discFileSystemSource, StringComparison.Ordinal);
        Assert.Contains("<< 2U", discFileSystemSource, StringComparison.Ordinal);
        Assert.Contains("#include \"platform/wii/WiiDiscFileSystem.hpp\"", generatedFileSource, StringComparison.Ordinal);
        Assert.Contains("helengine::wii::WiiDiscFileSystem::CanHandlePath(fileName)", generatedFileSource, StringComparison.Ordinal);
        Assert.Contains("helengine::wii::WiiDiscFileSystem::Exists(fileName)", generatedFileSource, StringComparison.Ordinal);
        Assert.Contains("helengine::wii::WiiDiscFileSystem::OpenRead(filePath)", generatedFileSource, StringComparison.Ordinal);
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
        Assert.Contains("[string]$IsoPath", launcherSource, StringComparison.Ordinal);
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
        Assert.Contains("'-u', $userDir, '-e', $resolvedIsoPath", launcherSource, StringComparison.Ordinal);
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

        Assert.Contains("SYS_STDIO_Report(true);", mainSource, StringComparison.Ordinal);
        Assert.Contains("std::fprintf(stderr, \"[Wii] stderr bridge armed.\\n\");", mainSource, StringComparison.Ordinal);
        Assert.Contains("std::fflush(stderr);", mainSource, StringComparison.Ordinal);
        Assert.Contains("SYS_Report(\"[Wii] main() entered.\\n\");", mainSource, StringComparison.Ordinal);
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
    /// Ensures the packaged-disc Wii entrypoint mounts <c>sd:/</c> and writes one earliest-possible host-readable trace file there for Dolphin folder-sync debugging.
    /// </summary>
    [Fact]
    public void PackagedDebugLogging_WritesMainEntryProbeIntoSdTracePath() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string mainSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "main.cpp"));

        Assert.Contains("#include <fat.h>", mainSource, StringComparison.Ordinal);
        Assert.Contains("fatInitDefault();", mainSource, StringComparison.Ordinal);
        Assert.Contains("sd:/main_entry_trace.txt", mainSource, StringComparison.Ordinal);
        Assert.Contains("std::fopen(\"sd:/main_entry_trace.txt\", \"ab\")", mainSource, StringComparison.Ordinal);
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
        Assert.Contains("Config.CurrentRequestIndex == Config.RequestCount", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("const uint EntryPointAddress = 0x81200000U;", apploaderImageBuilderSource, StringComparison.Ordinal);
        Assert.Contains(". = 0x81200000;", apploaderLinkerScript, StringComparison.Ordinal);
        Assert.Contains("reinterpret_cast<volatile uint32_t*>(ArenaHighLowMemoryAddress)", apploaderSource, StringComparison.Ordinal);
        Assert.Contains("*(.data*)", apploaderLinkerScript, StringComparison.Ordinal);
        Assert.Contains("*(.bss*)", apploaderLinkerScript, StringComparison.Ordinal);
    }
}
