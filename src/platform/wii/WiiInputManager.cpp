#include "platform/wii/WiiInputManager.hpp"

#include <gccore.h>

#include "InputGamepadButton.hpp"
#include "InputGamepadState.hpp"
#include "runtime/array.hpp"

namespace helengine::wii {
    /// Creates the Wii input backend and initializes libogc controller polling.
    WiiInputManager::WiiInputManager() {
        PAD_Init();
    }

    /// Releases the Wii input backend.
    WiiInputManager::~WiiInputManager() {
    }

    /// Captures one bootstrap input frame with one optional controller state.
    InputFrameState WiiInputManager::CaptureFrame() {
        PAD_ScanPads();

        InputFrameState frame;
        frame.set_GamepadCount(1);

        Array<InputGamepadState>* gamepads = new Array<InputGamepadState>(1);
        InputGamepadState gamepadState;
        gamepadState.set_Connected(true);

        const u16 heldButtons = PAD_ButtonsHeld(0);
        gamepadState.SetButtonDown(InputGamepadButton::DPadUp, (heldButtons & PAD_BUTTON_UP) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::DPadDown, (heldButtons & PAD_BUTTON_DOWN) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::DPadLeft, (heldButtons & PAD_BUTTON_LEFT) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::DPadRight, (heldButtons & PAD_BUTTON_RIGHT) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::South, (heldButtons & PAD_BUTTON_A) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::East, (heldButtons & PAD_BUTTON_B) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::West, (heldButtons & PAD_BUTTON_X) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::North, (heldButtons & PAD_BUTTON_Y) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::LeftShoulder, (heldButtons & PAD_TRIGGER_L) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::RightShoulder, (heldButtons & PAD_TRIGGER_R) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::Start, (heldButtons & PAD_BUTTON_START) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::Select, (heldButtons & PAD_TRIGGER_Z) != 0);
        gamepadState.set_LeftStickX(static_cast<int16_t>(PAD_StickX(0) * 256));
        gamepadState.set_LeftStickY(static_cast<int16_t>(PAD_StickY(0) * 256));
        gamepadState.set_RightStickX(static_cast<int16_t>(PAD_SubStickX(0) * 256));
        gamepadState.set_RightStickY(static_cast<int16_t>(PAD_SubStickY(0) * 256));
        gamepadState.set_LeftTrigger(static_cast<int16_t>(PAD_TriggerL(0) * 256));
        gamepadState.set_RightTrigger(static_cast<int16_t>(PAD_TriggerR(0) * 256));

        (*gamepads)[0] = gamepadState;
        frame.set_Gamepads(gamepads);
        return frame;
    }
}
