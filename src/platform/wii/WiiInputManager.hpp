#pragma once

#include "IInputBackend.hpp"
#include "InputFrameState.hpp"

namespace helengine::wii {
    /// Implements the generated input backend contract for the bootstrap Wii host.
    class WiiInputManager : public IInputBackend {
    public:
        /// Creates the Wii input backend and initializes libogc controller polling.
        WiiInputManager();

        /// Releases the Wii input backend.
        ~WiiInputManager();

        /// Captures one bootstrap input frame with one optional controller state.
        InputFrameState CaptureFrame() override;
    };
}
