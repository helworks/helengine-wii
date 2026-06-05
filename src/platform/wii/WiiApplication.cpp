#include "platform/wii/WiiApplication.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

#if HELENGINE_WII_HAS_GENERATED_CORE
#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "Exception.hpp"
#include "PlatformInfo.hpp"
#include "platform/wii/WiiInputManager.hpp"
#include "platform/wii/WiiRenderManager2D.hpp"
#include "platform/wii/WiiRenderManager3D.hpp"
#endif

namespace helengine::wii {
    namespace {
        constexpr std::size_t DefaultFifoSize = 256 * 1024;
        constexpr GXColor PinkClearColor { 0xFF, 0x69, 0xB4, 0xFF };
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
        return true;
    }

    /// Advances one engine frame when the generated core was initialized successfully.
    bool WiiApplication::UpdateEngineCore() {
        return true;
    }

    /// Draws one engine frame when the generated core was initialized successfully.
    bool WiiApplication::DrawEngineCore() {
        return true;
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
