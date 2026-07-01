#include "platform/wii/WiiApplication.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <cstdio>
#include <malloc.h>

#include <ogc/conf.h>
#include <ogc/dvd.h>
#include <ogc/isfs.h>
#include <ogc/system.h>
#include <wiiuse/wpad.h>

#if HELENGINE_WII_HAS_GENERATED_CORE
#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "InputControlId.hpp"
#include "InputControlKind.hpp"
#include "InputDeviceKind.hpp"
#include "InputGamepadButton.hpp"
#include "PlatformInfo.hpp"
#include "RuntimeSceneLoadService.hpp"
#include "SceneManager.hpp"
#include "SceneLoadMode.hpp"
#include "StandardPlatformAction.hpp"
#include "StandardPlatformActionBinding.hpp"
#include "StandardPlatformInputConfiguration.hpp"
#include "platform/wii/WiiInputManager.hpp"
#include "platform/wii/WiiRenderManager2D.hpp"
#include "platform/wii/WiiRenderManager3D.hpp"
#include "platform/wii/WiiSceneBootstrap.hpp"
#include "runtime/native_exceptions.hpp"
#include "runtime/native_list.hpp"
#endif

namespace helengine::wii {
    namespace {
        uint32_t UpdateFrameLogCount = 0U;
        uint32_t DrawFrameLogCount = 0U;
    }

    namespace {
        volatile bool ShutdownRequested = false;
        constexpr std::size_t DefaultFifoSize = 256 * 1024;
        constexpr GXColor PinkClearColor { 0xFF, 0x69, 0xB4, 0xFF };
        constexpr GXColor FailureClearColor { 0xFF, 0x00, 0x00, 0xFF };
        constexpr const char* RuntimeTracePaths[] = {
            "sd:/runtime_registry_trace.txt",
            "runtime_registry_trace.txt"
        };
        constexpr const char* BuildStamp = __DATE__ " " __TIME__;
        bool RuntimeTraceIsfsInitializationAttempted = false;
        bool RuntimeTraceIsfsAvailable = false;
        char RuntimeTraceIsfsPath[144] {};

        /// <summary>
        /// Records one host-owned shutdown request so the main Wii frame loop can exit cleanly.
        /// </summary>
        void RequestShutdown() {
            ShutdownRequested = true;
        }

        /// <summary>
        /// Handles the Wii console reset button by stopping the current emulation session.
        /// </summary>
        void HandleResetButtonPressed(u32 resetKind, void* context) {
            static_cast<void>(resetKind);
            static_cast<void>(context);
            RequestShutdown();
        }

        /// <summary>
        /// Handles the Wii console power button by stopping the current emulation session.
        /// </summary>
        void HandlePowerButtonPressed() {
            RequestShutdown();
        }

        /// <summary>
        /// Handles the Wii Remote power button by stopping the current emulation session.
        /// </summary>
        /// <param name="channel">Controller channel that issued the power request.</param>
        void HandleWiimotePowerButtonPressed(s32 channel) {
            static_cast<void>(channel);
            RequestShutdown();
        }

        /// <summary>
        /// Registers Wii host shutdown callbacks so Dolphin close requests can terminate the guest on the first attempt.
        /// </summary>
        void RegisterShutdownCallbacks() {
            ShutdownRequested = false;
            SYS_SetResetCallback(HandleResetButtonPressed);
            SYS_SetPowerCallback(HandlePowerButtonPressed);
            WPAD_SetPowerButtonCallback(HandleWiimotePowerButtonPressed);
        }

        /// <summary>
        /// Creates one host-readable per-title trace file path under the emulated Wii save-data tree.
        /// </summary>
        /// <param name="fileName">Trace file name to place under the title data directory.</param>
        /// <param name="pathBuffer">Destination buffer that receives the resolved ISFS path.</param>
        /// <param name="pathBufferLength">Capacity of <paramref name="pathBuffer"/> in bytes.</param>
        /// <returns><see langword="true"/> when the current disc id was available and the path fit in the supplied buffer.</returns>
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
        /// Creates the per-title save-data directory used for packaged-disc host-readable trace files.
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

        void InitializeRuntimeTraceIsfsPath() {
            if (RuntimeTraceIsfsInitializationAttempted) {
                return;
            }

            RuntimeTraceIsfsInitializationAttempted = true;
            if (ISFS_Initialize() != ISFS_OK) {
                return;
            }

            char traceDirectoryPath[112];
            if (!TryEnsureTitleDataTraceDirectory(traceDirectoryPath, sizeof(traceDirectoryPath))
                || !TryResolveTitleDataTracePath("runtime_registry_trace.txt", RuntimeTraceIsfsPath, sizeof(RuntimeTraceIsfsPath))) {
                return;
            }

            s32 fileDescriptor = ISFS_Open(RuntimeTraceIsfsPath, ISFS_OPEN_RW);
            if (fileDescriptor < 0) {
                if (ISFS_CreateFile(RuntimeTraceIsfsPath, 0, 3, 3, 3) != ISFS_OK) {
                    return;
                }

                fileDescriptor = ISFS_Open(RuntimeTraceIsfsPath, ISFS_OPEN_RW);
                if (fileDescriptor < 0) {
                    return;
                }
            }

            ISFS_Close(fileDescriptor);
            RuntimeTraceIsfsAvailable = true;
        }

        void AppendRuntimeTraceToIsfs(const char* text) {
            InitializeRuntimeTraceIsfsPath();
            if (!RuntimeTraceIsfsAvailable) {
                return;
            }

            s32 fileDescriptor = ISFS_Open(RuntimeTraceIsfsPath, ISFS_OPEN_RW);
            if (fileDescriptor < 0) {
                RuntimeTraceIsfsAvailable = false;
                return;
            }

            const std::size_t textLength = std::strlen(text);
            if (textLength > 0) {
                ISFS_Seek(fileDescriptor, 0, SEEK_END);
                ISFS_Write(fileDescriptor, text, static_cast<u32>(textLength));
            }

            ISFS_Close(fileDescriptor);
        }

        void AppendRuntimeTrace(const char* format, ...) {
            char buffer[2048];
            va_list arguments;
            va_start(arguments, format);
            std::vsnprintf(buffer, sizeof(buffer), format, arguments);
            va_end(arguments);

            for (const char* runtimeTracePath : RuntimeTracePaths) {
                std::FILE* file = std::fopen(runtimeTracePath, "ab");
                if (file == nullptr) {
                    continue;
                }

                std::fputs(buffer, file);
                std::fflush(file);
                std::fclose(file);
            }

            AppendRuntimeTraceToIsfs(buffer);
        }
    }

    /// Creates the Wii application with no initialized native or engine state.
    WiiApplication::WiiApplication()
        : RenderMode(nullptr)
        , FrameBuffers { nullptr, nullptr }
        , FrameBufferIndex(0)
        , FifoBuffer(nullptr)
        , ClearColor(PinkClearColor)
        , BootPhase(WiiBootPhase::NativeStartup)
        , EngineInitialized(false)
        , PresentedFrameCount(0)
        , VerifiedFrameCount(0)
        , UpdateCompletedSincePresent(false)
        , DrawCompletedSincePresent(false)
#if HELENGINE_WII_HAS_GENERATED_CORE
        , EngineCore(nullptr)
        , EngineRenderManager3D(nullptr)
        , EngineRenderManager2D(nullptr)
        , EngineInputManager(nullptr)
        , EnginePlatformInfo(nullptr)
#endif
    {
    }

    /// Releases generated-core bridge objects after the application loop finishes.
    WiiApplication::~WiiApplication() {
#if HELENGINE_WII_HAS_GENERATED_CORE
        delete EngineInputManager;
        delete EngineRenderManager2D;
        delete EngineRenderManager3D;
        delete EnginePlatformInfo;
        delete EngineCore;
#endif

        if (FifoBuffer != nullptr) {
            std::free(FifoBuffer);
        }
    }

    /// Initializes the native host and enters the steady-state frame loop.
    int WiiApplication::Run() {
        RegisterShutdownCallbacks();
        if (!InitializeVideo()) {
            return 1;
        }

        if (!InitializeGraphics()) {
            return 1;
        }

#if HELENGINE_WII_HAS_GENERATED_CORE
        if (!InitializeEngineCore()) {
            while (!ShutdownRequested) {
                PresentFrame();
            }

            return 0;
        }
#endif

        while (!ShutdownRequested) {
#if HELENGINE_WII_HAS_GENERATED_CORE
            if (!UpdateEngineCore()) {
                PresentFrame();
                return 1;
            }

            if (!DrawEngineCore()) {
                PresentFrame();
                return 1;
            }
#endif
            PresentFrame();
            if (HasSatisfiedVerificationExitCondition()) {
                return 0;
            }
        }

        return 0;
    }

    /// Initializes the VI display state and allocates the external framebuffers.
    bool WiiApplication::InitializeVideo() {
        VIDEO_Init();

        RenderMode = VIDEO_GetPreferredMode(nullptr);
        if (RenderMode == nullptr) {
            return false;
        }

        FrameBuffers[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(RenderMode));
        FrameBuffers[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(RenderMode));
        if (FrameBuffers[0] == nullptr || FrameBuffers[1] == nullptr) {
            return false;
        }

        VIDEO_Configure(RenderMode);
        VIDEO_SetNextFramebuffer(FrameBuffers[FrameBufferIndex]);
        VIDEO_SetBlack(FALSE);
        VIDEO_Flush();
        VIDEO_WaitVSync();

        if (RenderMode->viTVMode & VI_NON_INTERLACE) {
            VIDEO_WaitVSync();
        }

        return true;
    }

    /// Initializes GX for the host clear-and-present loop.
    bool WiiApplication::InitializeGraphics() {
        FifoBuffer = memalign(32, DefaultFifoSize);
        if (FifoBuffer == nullptr) {
            return false;
        }

        std::memset(FifoBuffer, 0, DefaultFifoSize);
        GX_Init(FifoBuffer, DefaultFifoSize);

        const f32 yScale = GX_GetYScaleFactor(RenderMode->efbHeight, RenderMode->xfbHeight);
        const u16 xfbScaledHeight = GX_SetDispCopyYScale(yScale);

        GX_SetScissor(0, 0, RenderMode->fbWidth, RenderMode->efbHeight);
        GX_SetDispCopySrc(0, 0, RenderMode->fbWidth, RenderMode->efbHeight);
        GX_SetDispCopyDst(RenderMode->fbWidth, xfbScaledHeight);
        GX_SetCopyFilter(RenderMode->aa, RenderMode->sample_pattern, GX_TRUE, RenderMode->vfilter);
        GX_SetFieldMode(RenderMode->field_rendering, ((RenderMode->viHeight == (RenderMode->xfbHeight * 2)) ? GX_ENABLE : GX_DISABLE));
        GX_SetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
        GX_SetCullMode(GX_CULL_NONE);
        GX_SetDispCopyGamma(GX_GM_1_0);
        GX_SetNumChans(1);
        GX_SetNumTexGens(0);
        GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        GX_SetColorUpdate(GX_TRUE);
        GX_SetAlphaUpdate(GX_FALSE);
        GX_SetViewport(0.0F, 0.0F, static_cast<f32>(RenderMode->fbWidth), static_cast<f32>(RenderMode->efbHeight), 0.0F, 1.0F);
        SYS_Report(
            "[Wii] Video mode fb=%ux%u efbHeight=%u xfbHeight=%u vi=%ux%u xfbScaled=%u\n",
            static_cast<unsigned>(RenderMode->fbWidth),
            static_cast<unsigned>(RenderMode->efbHeight),
            static_cast<unsigned>(RenderMode->efbHeight),
            static_cast<unsigned>(RenderMode->xfbHeight),
            static_cast<unsigned>(RenderMode->viWidth),
            static_cast<unsigned>(RenderMode->viHeight),
            static_cast<unsigned>(xfbScaledHeight));
        GX_InvVtxCache();
        GX_InvalidateTexAll();
        return true;
    }

    /// Initializes the generated engine core when generated sources are present in the build.
    bool WiiApplication::InitializeEngineCore() {
#if HELENGINE_WII_HAS_GENERATED_CORE
        const char* initializationStage = "BeforeCoreConstruction";
        try {
            AppendRuntimeTrace("\n=== Wii runtime session %s ===\n", BuildStamp);
            SYS_Report("[Wii] InitializeEngineCore begin.\n");
            initializationStage = "ConstructCore";
            SetBootPhase(WiiBootPhase::CoreConstruction, GXColor { 0xFF, 0xFF, 0x00, 0xFF });
            EngineCore = new Core();

            initializationStage = "ReadInitializationOptions";
            SetBootPhase(WiiBootPhase::CoreOptions, GXColor { 0xFF, 0x80, 0x00, 0xFF });
            CoreInitializationOptions* options = EngineCore->get_InitializationOptions();
            if (options == nullptr) {
                FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
                return false;
            }

            initializationStage = "ConfigureSceneBootstrap";
            SetBootPhase(WiiBootPhase::SceneBootstrap, GXColor { 0x40, 0x80, 0xFF, 0xFF });
#if HELENGINE_WII_PACKAGED_DISC_BOOT
            if (!WiiSceneBootstrap::InitializePackagedStorage()) {
                FailBootPhase(WiiBootPhase::SceneBootstrap, FailureClearColor);
                SYS_Report("[Wii] Runtime content root initialization failed.\n");
                return false;
            }
            const std::string packagedContentRootPath = WiiSceneBootstrap::GetPackagedContentRootPath();
            SYS_Report("[Wii] Runtime content root: %s\n", packagedContentRootPath.c_str());
            AppendRuntimeTrace("[WiiFile] Runtime content root: %s\n", packagedContentRootPath.c_str());
            options->ContentRootPath = packagedContentRootPath;
            options->SceneCatalog = WiiSceneBootstrap::CreatePackagedSceneCatalog();
            const std::string packagedStartupSceneId = WiiSceneBootstrap::GetPackagedStartupSceneId();
            SYS_Report("[Wii] Runtime startup scene id: %s\n", packagedStartupSceneId.c_str());
            AppendRuntimeTrace("[WiiFile] Runtime startup scene id: %s\n", packagedStartupSceneId.c_str());
#else
            const std::string contentRootPath = WiiSceneBootstrap::GetValidatedContentRootPath();
            const std::string startupSceneId = WiiSceneBootstrap::GetStartupSceneId();
            SYS_Report("[Wii] Staged content root: %s\n", contentRootPath.c_str());
            AppendRuntimeTrace("[WiiFile] Staged content root: %s\n", contentRootPath.c_str());
            SYS_Report("[Wii] Startup scene id: %s\n", startupSceneId.c_str());
            AppendRuntimeTrace("[WiiFile] Startup scene id: %s\n", startupSceneId.c_str());
            options->ContentRootPath = contentRootPath;
            options->SceneCatalog = WiiSceneBootstrap::CreateSceneCatalog();
#endif
            options->UpdateOrderLayers = 4;
            options->RenderOrderLayers3D = 4;
            options->UpdateListInitialCapacity = 64;
            options->RenderList2DInitialCapacity = 64;
            options->RenderList3DInitialCapacity = 64;
            options->StandardPlatformInputConfiguration = new StandardPlatformInputConfiguration(new List<StandardPlatformActionBinding*>({
                new StandardPlatformActionBinding(StandardPlatformAction::Accept, InputControlId(InputDeviceKind::Gamepad, InputControlKind::Button, 0, static_cast<int32_t>(InputGamepadButton::South))),
                new StandardPlatformActionBinding(StandardPlatformAction::Return, InputControlId(InputDeviceKind::Gamepad, InputControlKind::Button, 0, static_cast<int32_t>(InputGamepadButton::East)))
            }));

            initializationStage = "ConstructBridgeServices";
            SetBootPhase(WiiBootPhase::BridgeConstruction, GXColor { 0x00, 0xFF, 0xFF, 0xFF });
            EngineRenderManager3D = new WiiRenderManager3D();
            EngineRenderManager2D = new WiiRenderManager2D();
            EngineRenderManager3D->SetOverlayRenderManager2D(EngineRenderManager2D);
            EngineInputManager = new WiiInputManager();
            EnginePlatformInfo = new PlatformInfo("wii", "1.0");

            initializationStage = "AddPrimaryWindow";
            SetBootPhase(WiiBootPhase::CoreInitialization, GXColor { 0x00, 0x00, 0xFF, 0xFF });
            const uint16_t logicalFrameWidth = ResolveLogicalFrameWidth();
            const uint16_t logicalFrameHeight = ResolveLogicalFrameHeight();
            EngineRenderManager3D->AddWindow(0, logicalFrameWidth, logicalFrameHeight);
            EngineRenderManager3D->SetPresentedFrameSize(static_cast<uint16_t>(RenderMode->fbWidth), static_cast<uint16_t>(RenderMode->efbHeight));

            initializationStage = "InitializeCore";
            EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputManager, EnginePlatformInfo, options);
            SYS_Report("[Wii] Engine core initialized.\n");
            AppendRuntimeTrace("[WiiFile] Engine core initialized.\n");
        }
        catch (const std::exception& exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine core initialization threw std::exception stage=%s message=%s\n", initializationStage, exception.what());
            AppendRuntimeTrace("[WiiFile] Engine core initialization threw std::exception stage=%s message=%s\n", initializationStage, exception.what());
            return false;
        }
        catch (Exception* exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine core initialization threw Exception stage=%s message=%s\n", initializationStage, exception != nullptr ? exception->what() : "<null>");
            AppendRuntimeTrace("[WiiFile] Engine core initialization threw Exception stage=%s message=%s\n", initializationStage, exception != nullptr ? exception->what() : "<null>");
            delete exception;
            return false;
        }
        catch (...) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine core initialization threw stage=%s.\n", initializationStage);
            AppendRuntimeTrace("[WiiFile] Engine core initialization threw stage=%s.\n", initializationStage);
            return false;
        }

        try {
            SetBootPhase(WiiBootPhase::SceneLoad, GXColor { 0x80, 0xC0, 0x40, 0xFF });
            if (EngineCore->get_SceneManager() == nullptr) {
#if HELENGINE_WII_PACKAGED_DISC_BOOT
                throw std::runtime_error("Manifest-backed Wii boot requires a runtime scene manager.");
#else
                throw std::runtime_error("Direct-DOL Wii boot requires a runtime scene manager.");
#endif
            }

#if HELENGINE_WII_PACKAGED_DISC_BOOT
            const std::string packagedStartupSceneId = WiiSceneBootstrap::GetPackagedStartupSceneId();
            EngineCore->get_SceneManager()->LoadScene(packagedStartupSceneId, SceneLoadMode::Single);
#else
            const std::string startupSceneId = WiiSceneBootstrap::GetStartupSceneId();
            EngineCore->get_SceneManager()->LoadScene(startupSceneId, SceneLoadMode::Single);
#endif
            SYS_Report("[Wii] Runtime startup scene queued.\n");
            AppendRuntimeTrace("[WiiFile] Runtime startup scene queued.\n");
            EngineInitialized = true;
            PresentedFrameCount = 0;
            VerifiedFrameCount = 0;
            UpdateCompletedSincePresent = false;
            DrawCompletedSincePresent = false;
            SetBootPhase(WiiBootPhase::Running, GXColor { 0x00, 0xFF, 0x00, 0xFF });
            return true;
        }
        catch (const std::exception& exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine scene bootstrap threw std::exception: %s\n", exception.what());
            AppendRuntimeTrace("[WiiFile] Engine scene bootstrap threw std::exception: %s\n", exception.what());
            return false;
        }
        catch (Exception* exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine scene bootstrap threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            AppendRuntimeTrace("[WiiFile] Engine scene bootstrap threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            delete exception;
            return false;
        }
        catch (...) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine scene bootstrap threw.\n");
            AppendRuntimeTrace("[WiiFile] Engine scene bootstrap threw.\n");
            return false;
        }
#else
        return true;
#endif
    }

    /// Advances one engine frame when the generated core was initialized successfully.
    bool WiiApplication::UpdateEngineCore() {
#if HELENGINE_WII_HAS_GENERATED_CORE
        if (!EngineInitialized || EngineCore == nullptr || EngineRenderManager2D == nullptr) {
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            return false;
        }

        try {
            SetBootPhase(WiiBootPhase::CoreUpdate, GXColor { 0x00, 0xA0, 0x00, 0xFF });
            if (UpdateFrameLogCount < 8U) {
                SYS_Report("[Wii] Engine update begin frame=%lu\n", static_cast<unsigned long>(UpdateFrameLogCount));
            }
            EngineRenderManager2D->BeginFrame();
            EngineCore->Update();
            if (UpdateFrameLogCount < 8U && EngineCore->get_SceneManager() != nullptr) {
                SYS_Report(
                    "[Wii] SceneManager trace stage=%s scene=%s loaded=%ld pending=%ld\n",
                    EngineCore->get_SceneManager()->get_LastTraceStage().c_str(),
                    EngineCore->get_SceneManager()->get_LastTraceSceneId().c_str(),
                    static_cast<long>(EngineCore->get_SceneManager()->get_LastTraceLoadedSceneCount()),
                    static_cast<long>(EngineCore->get_SceneManager()->get_LastTracePendingOperationCount()));
                AppendRuntimeTrace(
                    "[WiiFile] SceneManager trace stage=%s scene=%s loaded=%ld pending=%ld\n",
                    EngineCore->get_SceneManager()->get_LastTraceStage().c_str(),
                    EngineCore->get_SceneManager()->get_LastTraceSceneId().c_str(),
                    static_cast<long>(EngineCore->get_SceneManager()->get_LastTraceLoadedSceneCount()),
                    static_cast<long>(EngineCore->get_SceneManager()->get_LastTracePendingOperationCount()));
            }

            if (UpdateFrameLogCount < 8U && EngineCore->get_SceneLoadService() != nullptr) {
                SYS_Report(
                    "[Wii] SceneLoad trace stage=%s root=%ld depth=%ld component=%s textStage=%s textFont=%s texture=%s\n",
                    EngineCore->get_SceneLoadService()->get_LastTraceStage().c_str(),
                    static_cast<long>(EngineCore->get_SceneLoadService()->get_LastTraceRootEntityIndex()),
                    static_cast<long>(EngineCore->get_SceneLoadService()->get_LastTraceEntityDepth()),
                    EngineCore->get_SceneLoadService()->get_LastTraceComponentTypeId().c_str(),
                    EngineCore->get_SceneLoadService()->get_LastTextLoadStage().c_str(),
                    EngineCore->get_SceneLoadService()->get_LastTextFontRelativePath().c_str(),
                    EngineCore->get_SceneLoadService()->get_LastTextureRelativePath().c_str());
                AppendRuntimeTrace(
                    "[WiiFile] SceneLoad trace stage=%s root=%ld depth=%ld component=%s textStage=%s textFont=%s texture=%s\n",
                    EngineCore->get_SceneLoadService()->get_LastTraceStage().c_str(),
                    static_cast<long>(EngineCore->get_SceneLoadService()->get_LastTraceRootEntityIndex()),
                    static_cast<long>(EngineCore->get_SceneLoadService()->get_LastTraceEntityDepth()),
                    EngineCore->get_SceneLoadService()->get_LastTraceComponentTypeId().c_str(),
                    EngineCore->get_SceneLoadService()->get_LastTextLoadStage().c_str(),
                    EngineCore->get_SceneLoadService()->get_LastTextFontRelativePath().c_str(),
                    EngineCore->get_SceneLoadService()->get_LastTextureRelativePath().c_str());
            }
            EngineRenderManager2D->FlushReleasedTextures();
            if (EngineRenderManager3D != nullptr) {
                EngineRenderManager3D->FlushReleasedAssets();
            }

            if (UpdateFrameLogCount < 8U) {
                SYS_Report("[Wii] Engine update completed frame=%lu\n", static_cast<unsigned long>(UpdateFrameLogCount));
            }
            UpdateFrameLogCount++;

            UpdateCompletedSincePresent = true;
            return true;
        }
        catch (Exception* exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine update threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            AppendRuntimeTrace("[WiiFile] Engine update threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            delete exception;
            return false;
        }
        catch (const std::exception& exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine update threw std::exception: %s\n", exception.what());
            AppendRuntimeTrace("[WiiFile] Engine update threw std::exception: %s\n", exception.what());
            return false;
        }
        catch (...) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine update threw.\n");
            AppendRuntimeTrace("[WiiFile] Engine update threw.\n");
            return false;
        }
#else
        return true;
#endif
    }

    /// Draws one engine frame when the generated core was initialized successfully.
    bool WiiApplication::DrawEngineCore() {
#if HELENGINE_WII_HAS_GENERATED_CORE
        if (!EngineInitialized || EngineCore == nullptr || EngineRenderManager3D == nullptr || EngineRenderManager2D == nullptr) {
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            return false;
        }

        try {
            SetBootPhase(WiiBootPhase::CoreDraw, GXColor { 0x00, 0x60, 0x00, 0xFF });
            if (DrawFrameLogCount < 8U) {
                SYS_Report("[Wii] Engine draw begin frame=%lu\n", static_cast<unsigned long>(DrawFrameLogCount));
            }
            EngineCore->Draw();
            const uint16_t logicalFrameWidth = ResolveLogicalFrameWidth();
            const uint16_t logicalFrameHeight = ResolveLogicalFrameHeight();
            EngineRenderManager2D->RenderCapturedCommands(
                logicalFrameWidth,
                logicalFrameHeight,
                static_cast<uint16_t>(RenderMode->fbWidth),
                static_cast<uint16_t>(RenderMode->efbHeight));
            if (DrawFrameLogCount < 8U) {
                SYS_Report(
                    "[Wii] Render2D trace cameras=%ld drawables=%ld queuedText=%ld submittedGlyph=%s\n",
                    static_cast<long>(EngineRenderManager2D->get_VisitedCameraCount()),
                    static_cast<long>(EngineRenderManager2D->get_VisitedDrawableCount()),
                    static_cast<long>(EngineRenderManager2D->get_QueuedTextCount()),
                    EngineRenderManager2D->get_DidSubmitGlyph() ? "true" : "false");
                AppendRuntimeTrace(
                    "[WiiFile] Render2D trace cameras=%ld drawables=%ld queuedText=%ld submittedGlyph=%s\n",
                    static_cast<long>(EngineRenderManager2D->get_VisitedCameraCount()),
                    static_cast<long>(EngineRenderManager2D->get_VisitedDrawableCount()),
                    static_cast<long>(EngineRenderManager2D->get_QueuedTextCount()),
                    EngineRenderManager2D->get_DidSubmitGlyph() ? "true" : "false");
            }
            if (DrawFrameLogCount < 8U) {
                SYS_Report("[Wii] Engine draw completed frame=%lu\n", static_cast<unsigned long>(DrawFrameLogCount));
            }
            DrawFrameLogCount++;
            DrawCompletedSincePresent = true;
            VerifiedFrameCount++;
            return true;
        }
        catch (Exception* exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine draw threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            AppendRuntimeTrace("[WiiFile] Engine draw threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            delete exception;
            return false;
        }
        catch (const std::exception& exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine draw threw std::exception: %s\n", exception.what());
            AppendRuntimeTrace("[WiiFile] Engine draw threw std::exception: %s\n", exception.what());
            return false;
        }
        catch (...) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine draw threw.\n");
            AppendRuntimeTrace("[WiiFile] Engine draw threw.\n");
            return false;
        }
#else
        return true;
#endif
    }

    /// Presents one fallback or generated frame to the active framebuffer.
    void WiiApplication::PresentFrame() {
        GX_SetCopyClear(ResolvePresentedClearColor(), 0x00FFFFFF);
        FrameBufferIndex ^= 1U;
        GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        GX_SetColorUpdate(GX_TRUE);
        GX_SetAlphaUpdate(GX_TRUE);
        GX_CopyDisp(FrameBuffers[FrameBufferIndex], GX_TRUE);
        GX_DrawDone();
        GX_Flush();
        VIDEO_SetNextFramebuffer(FrameBuffers[FrameBufferIndex]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        PresentedFrameCount++;
    }

    /// Resolves the currently visible diagnostic color for the next presented frame.
    GXColor WiiApplication::ResolvePresentedClearColor() {
#if HELENGINE_WII_HAS_GENERATED_CORE
        if (EngineInitialized) {
            UpdateCompletedSincePresent = false;
            DrawCompletedSincePresent = false;
            if (EngineRenderManager3D != nullptr && EngineRenderManager3D->HasPresentedClearColor()) {
                return EngineRenderManager3D->GetPresentedClearColor();
            }
        }
#endif

        return ClearColor;
    }

    /// Returns whether the current Wii system configuration requests widescreen presentation.
    bool WiiApplication::IsWidescreenAspectEnabled() const {
        return CONF_GetAspectRatio() == CONF_ASPECT_16_9;
    }

    /// Resolves the logical frame width reported to the shared engine layout systems.
    uint16_t WiiApplication::ResolveLogicalFrameWidth() const {
        if (RenderMode == nullptr) {
            return 0U;
        }

        if (IsWidescreenAspectEnabled()) {
            return static_cast<uint16_t>(((static_cast<uint32_t>(RenderMode->efbHeight) * 16U) + 8U) / 9U);
        }

        return static_cast<uint16_t>(RenderMode->fbWidth);
    }

    /// Resolves the logical frame height reported to the shared engine layout systems.
    uint16_t WiiApplication::ResolveLogicalFrameHeight() const {
        if (RenderMode == nullptr) {
            return 0U;
        }

        return static_cast<uint16_t>(RenderMode->efbHeight);
    }

    /// Sets the current boot phase and visible clear color.
    void WiiApplication::SetBootPhase(WiiBootPhase phase, GXColor color) {
        BootPhase = phase;
        ClearColor = color;
    }

    /// Marks the current boot phase as failed and updates the visible clear color.
    void WiiApplication::FailBootPhase(WiiBootPhase phase, GXColor color) {
        BootPhase = phase;
        ClearColor = color;
    }

    /// Returns whether runtime verification has presented the requested number of generated frames.
    bool WiiApplication::HasSatisfiedVerificationExitCondition() const {
#if HELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT > 0
        return VerifiedFrameCount >= static_cast<uint32_t>(HELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT);
#else
        return false;
#endif
    }
}
