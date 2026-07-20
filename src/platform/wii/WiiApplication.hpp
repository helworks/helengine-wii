#pragma once

#include <cstdint>

#include <gccore.h>

#include "platform/wii/WiiBootPhase.hpp"

class Core;
class IAudioBackend;
class PlatformInfo;

namespace helengine::wii {
    class WiiInputManager;
    class WiiRenderManager2D;
    class WiiRenderManager3D;

    /// Owns Wii host startup, optional generated-core boot, and the steady-state frame loop.
    class WiiApplication {
    public:
        /// Creates the Wii application with no initialized native or engine state.
        WiiApplication();

        /// Releases generated-core bridge objects after the application loop finishes.
        ~WiiApplication();

        /// Initializes the native host and enters the steady-state frame loop.
        int Run();

    private:
        /// Initializes the VI display state and allocates the external framebuffers.
        bool InitializeVideo();

        /// Initializes GX for the host clear-and-present loop.
        bool InitializeGraphics();

        /// Initializes the generated engine core when generated sources are present in the build.
        bool InitializeEngineCore();

        /// Advances one engine frame when the generated core was initialized successfully.
        bool UpdateEngineCore();

        /// Draws one engine frame when the generated core was initialized successfully.
        bool DrawEngineCore();

        /// Presents one fallback or generated frame to the active framebuffer.
        void PresentFrame();

        /// Resolves the currently visible diagnostic color for the next presented frame.
        GXColor ResolvePresentedClearColor();

        /// Returns whether the current Wii system configuration requests widescreen presentation.
        bool IsWidescreenAspectEnabled() const;

        /// Resolves the logical frame width reported to the shared engine layout systems.
        uint16_t ResolveLogicalFrameWidth() const;

        /// Resolves the logical frame height reported to the shared engine layout systems.
        uint16_t ResolveLogicalFrameHeight() const;

        /// Sets the current boot phase and visible clear color.
        void SetBootPhase(WiiBootPhase phase, GXColor color);

        /// Marks the current boot phase as failed and updates the visible clear color.
        void FailBootPhase(WiiBootPhase phase, GXColor color);

        /// Returns whether runtime verification has presented the requested number of generated frames.
        bool HasSatisfiedVerificationExitCondition() const;

        /// Stores the preferred video mode selected for the current console or emulator.
        GXRModeObj* RenderMode;

        /// Stores the allocated external framebuffers used for display output.
        void* FrameBuffers[2];

        /// Stores the index of the next external framebuffer that will receive the copied display image.
        uint32_t FrameBufferIndex;

        /// Stores the GX command FIFO allocation used by the renderer bootstrap.
        void* FifoBuffer;

        /// Stores the current fallback clear color for boot-state diagnostics.
        GXColor ClearColor;

        /// Stores the current host boot phase.
        WiiBootPhase BootPhase;

        /// Tracks whether the generated engine core finished initialization.
        bool EngineInitialized;

        /// Counts the number of frames presented after generated-core initialization succeeds.
        uint32_t PresentedFrameCount;

        /// Counts the number of generated-core frames that completed both update and draw.
        uint32_t VerifiedFrameCount;

        /// Tracks whether the current frame completed the generated update step before presentation.
        bool UpdateCompletedSincePresent;

        /// Tracks whether the current frame completed the generated draw step before presentation.
        bool DrawCompletedSincePresent;

#if HELENGINE_WII_HAS_GENERATED_CORE
        /// Stores the generated engine core instance when the build includes generated sources.
        Core* EngineCore;

        /// Stores the generated 3D render manager bridge.
        WiiRenderManager3D* EngineRenderManager3D;

        /// Stores the generated 2D render manager bridge.
        WiiRenderManager2D* EngineRenderManager2D;

        /// Stores the generated input manager bridge.
        WiiInputManager* EngineInputManager;

        /// Stores the generated audio backend bridge.
        IAudioBackend* EngineAudioBackend;

        /// Stores the platform descriptor passed into the generated core initialization contract.
        PlatformInfo* EnginePlatformInfo;
#endif
    };
}
