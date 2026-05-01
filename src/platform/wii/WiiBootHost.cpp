#include "platform/wii/WiiBootHost.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

namespace helengine::wii {
    namespace {
        constexpr std::size_t DefaultFifoSize = 256 * 1024;
        constexpr GXColor PinkClearColor { 0xFF, 0x69, 0xB4, 0xFF };
    }

    /// Creates the Wii boot host with no initialized native state.
    WiiBootHost::WiiBootHost()
        : RenderMode(nullptr)
        , FrameBuffer(nullptr)
        , FifoBuffer(nullptr) {
    }

    /// Initializes the native video path and presents the first boot frame until shutdown.
    int WiiBootHost::Run() {
        if (!InitializeVideo()) {
            return 1;
        }

        if (!InitializeGraphics()) {
            return 1;
        }

        while (true) {
            PresentFrame();
        }

        return 0;
    }

    /// Initializes the VI display state and allocates the first framebuffer.
    bool WiiBootHost::InitializeVideo() {
        VIDEO_Init();

        RenderMode = VIDEO_GetPreferredMode(nullptr);

        if (RenderMode == nullptr) {
            return false;
        }

        FrameBuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(RenderMode));

        if (FrameBuffer == nullptr) {
            return false;
        }

        VIDEO_Configure(RenderMode);
        VIDEO_SetNextFramebuffer(FrameBuffer);
        VIDEO_SetBlack(FALSE);
        VIDEO_Flush();
        VIDEO_WaitVSync();

        if (RenderMode->viTVMode & VI_NON_INTERLACE) {
            VIDEO_WaitVSync();
        }

        return true;
    }

    /// Initializes GX for a simple clear-and-present loop.
    bool WiiBootHost::InitializeGraphics() {
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

    /// Presents one solid pink frame to the active framebuffer.
    void WiiBootHost::PresentFrame() {
        GX_SetCopyClear(PinkClearColor, 0x00FFFFFF);
        GX_CopyDisp(FrameBuffer, GX_TRUE);
        GX_DrawDone();
        GX_Flush();
        VIDEO_SetNextFramebuffer(FrameBuffer);
        VIDEO_Flush();
        VIDEO_WaitVSync();
    }
}
