#include "platform/wii/WiiApplication.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <malloc.h>

#include <ogc/system.h>

#if HELENGINE_WII_HAS_GENERATED_CORE
#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "PlatformInfo.hpp"
#include "platform/wii/WiiInputManager.hpp"
#include "platform/wii/WiiRenderManager2D.hpp"
#include "platform/wii/WiiRenderManager3D.hpp"
#include "platform/wii/WiiSceneBootstrap.hpp"
#include "runtime/native_exceptions.hpp"
#endif

namespace helengine::wii {
    namespace {
        constexpr std::size_t DefaultFifoSize = 256 * 1024;
        constexpr GXColor PinkClearColor { 0xFF, 0x69, 0xB4, 0xFF };
        constexpr GXColor FailureClearColor { 0xFF, 0x00, 0x00, 0xFF };
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
        if (!InitializeVideo()) {
            return 1;
        }

        if (!InitializeGraphics()) {
            return 1;
        }

#if HELENGINE_WII_HAS_GENERATED_CORE
        if (!InitializeEngineCore()) {
            while (true) {
                PresentFrame();
            }
        }
#endif

        while (true) {
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
        const u16 xfbHeight = GX_SetDispCopyYScale(yScale);

        GX_SetScissor(0, 0, RenderMode->fbWidth, RenderMode->efbHeight);
        GX_SetDispCopySrc(0, 0, RenderMode->fbWidth, RenderMode->efbHeight);
        GX_SetDispCopyDst(RenderMode->fbWidth, xfbHeight);
        GX_SetCopyFilter(RenderMode->aa, RenderMode->sample_pattern, GX_TRUE, RenderMode->vfilter);
        GX_SetFieldMode(RenderMode->field_rendering, ((RenderMode->viHeight == (RenderMode->xfbHeight * 2)) ? GX_ENABLE : GX_DISABLE));
        GX_SetCullMode(GX_CULL_NONE);
        GX_SetDispCopyGamma(GX_GM_1_0);
        GX_SetNumChans(1);
        GX_SetNumTexGens(0);
        GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        GX_SetColorUpdate(GX_TRUE);
        GX_SetAlphaUpdate(GX_FALSE);
        GX_SetViewport(0.0F, 0.0F, static_cast<f32>(RenderMode->fbWidth), static_cast<f32>(RenderMode->efbHeight), 0.0F, 1.0F);
        GX_InvVtxCache();
        GX_InvalidateTexAll();
        return true;
    }

    /// Initializes the generated engine core when generated sources are present in the build.
    bool WiiApplication::InitializeEngineCore() {
#if HELENGINE_WII_HAS_GENERATED_CORE
        const char* initializationStage = "BeforeCoreConstruction";
        try {
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
            options->ContentRootPath = ".";
#if HELENGINE_WII_HAS_RUNTIME_SCENE_MANIFEST
            options->SceneCatalog = WiiSceneBootstrap::CreatePackagedSceneCatalog();
            const std::string packagedStartupSceneId = WiiSceneBootstrap::GetPackagedStartupSceneId();
            SYS_Report("[Wii] Runtime startup scene id: %s\n", packagedStartupSceneId.c_str());
#else
            options->SceneCatalog = nullptr;
#endif
            options->UpdateOrderLayers = 4;
            options->RenderOrderLayers3D = 4;
            options->UpdateListInitialCapacity = 64;
            options->RenderList2DInitialCapacity = 64;
            options->RenderList3DInitialCapacity = 64;

            initializationStage = "ConstructBridgeServices";
            SetBootPhase(WiiBootPhase::BridgeConstruction, GXColor { 0x00, 0xFF, 0xFF, 0xFF });
            EngineRenderManager3D = new WiiRenderManager3D();
            EngineRenderManager2D = new WiiRenderManager2D();
            EngineRenderManager3D->SetOverlayRenderManager2D(EngineRenderManager2D);
            EngineInputManager = new WiiInputManager();
            EnginePlatformInfo = new PlatformInfo("wii", "wii-headless");

            initializationStage = "AddPrimaryWindow";
            SetBootPhase(WiiBootPhase::CoreInitialization, GXColor { 0x00, 0x00, 0xFF, 0xFF });
            EngineRenderManager3D->AddWindow(0, RenderMode->fbWidth, RenderMode->efbHeight);

            initializationStage = "InitializeCore";
            EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputManager, EnginePlatformInfo, options);
            SYS_Report("[Wii] Engine core initialized.\n");
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
            SYS_Report("[Wii] Engine core initialization threw std::exception stage=%s message=%s\n", initializationStage, exception.what());
            return false;
        }
        catch (Exception* exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine core initialization threw Exception stage=%s message=%s\n", initializationStage, exception != nullptr ? exception->what() : "<null>");
            delete exception;
            return false;
        }
        catch (...) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine core initialization threw stage=%s.\n", initializationStage);
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
            EngineRenderManager2D->BeginFrame();
            EngineCore->Update();
            EngineRenderManager2D->FlushReleasedTextures();
            if (EngineRenderManager3D != nullptr) {
                EngineRenderManager3D->FlushReleasedAssets();
            }

            UpdateCompletedSincePresent = true;
            return true;
        }
        catch (Exception* exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine update threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            delete exception;
            return false;
        }
        catch (const std::exception& exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine update threw std::exception: %s\n", exception.what());
            return false;
        }
        catch (...) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine update threw.\n");
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
            EngineCore->Draw();
            DrawCompletedSincePresent = true;
            VerifiedFrameCount++;
            return true;
        }
        catch (Exception* exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine draw threw Exception*: %s\n", exception != nullptr ? exception->what() : "<null>");
            delete exception;
            return false;
        }
        catch (const std::exception& exception) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine draw threw std::exception: %s\n", exception.what());
            return false;
        }
        catch (...) {
            EngineInitialized = false;
            FailBootPhase(WiiBootPhase::Failed, FailureClearColor);
            SYS_Report("[Wii] Engine draw threw.\n");
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
            if (UpdateCompletedSincePresent && DrawCompletedSincePresent) {
                UpdateCompletedSincePresent = false;
                DrawCompletedSincePresent = false;
                return GXColor { 0x00, 0x80, 0x80, 0xFF };
            }

            if (UpdateCompletedSincePresent) {
                UpdateCompletedSincePresent = false;
                return GXColor { 0xC0, 0xC0, 0x00, 0xFF };
            }

            if (DrawCompletedSincePresent) {
                DrawCompletedSincePresent = false;
                return GXColor { 0x00, 0x60, 0xA0, 0xFF };
            }
        }
#endif

        return ClearColor;
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
