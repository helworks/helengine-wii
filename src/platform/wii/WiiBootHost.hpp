#pragma once

#include <gccore.h>

namespace helengine::wii {
    /// Owns the first Wii native video bootstrap and boot-frame presentation loop.
    class WiiBootHost {
    public:
        /// Creates the Wii boot host with no initialized native state.
        WiiBootHost();

        /// Initializes the native video path and presents the first boot frame until shutdown.
        int Run();

    private:
        /// Initializes the VI display state and allocates the first framebuffer.
        bool InitializeVideo();

        /// Initializes GX for a simple clear-and-present loop.
        bool InitializeGraphics();

        /// Presents one solid pink frame to the active framebuffer.
        void PresentFrame();

        /// Stores the preferred video mode selected for the current console or emulator.
        GXRModeObj* RenderMode;

        /// Stores the allocated external framebuffer used for display output.
        void* FrameBuffer;

        /// Stores the GX command FIFO allocation used by the first renderer bootstrap.
        void* FifoBuffer;
    };
}
