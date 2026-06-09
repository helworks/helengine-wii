#include "platform/wii/WiiInputManager.hpp"

#include <gccore.h>
#include <wiiuse/wpad.h>

#include "InputGamepadButton.hpp"
#include "InputGamepadState.hpp"
#include "runtime/array.hpp"

namespace helengine::wii {
    /// Creates the Wii input backend and initializes libogc controller polling.
    WiiInputManager::WiiInputManager() {
        PAD_Init();
        WPAD_Init();
        WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS);
    }

    /// Releases the Wii input backend.
    WiiInputManager::~WiiInputManager() {
    }

    /// Captures one bootstrap input frame with one optional controller state.
    InputFrameState WiiInputManager::CaptureFrame() {
        PAD_ScanPads();
        WPAD_ScanPads();

        InputFrameState frame;
        frame.set_GamepadCount(1);

        Array<InputGamepadState>* gamepads = new Array<InputGamepadState>(1);
        InputGamepadState gamepadState;
        const u32 wiiButtonsHeld = WPAD_ButtonsHeld(WPAD_CHAN_0);
        const u16 heldButtons = PAD_ButtonsHeld(0);
        const bool hasWiiInput = wiiButtonsHeld != 0U;
        const bool hasGameCubeInput = heldButtons != 0U || PAD_StickX(0) != 0 || PAD_StickY(0) != 0 || PAD_SubStickX(0) != 0 || PAD_SubStickY(0) != 0;
        u32 wiiProbeResult = 0U;
        const bool isWiiConnected = WPAD_Probe(WPAD_CHAN_0, &wiiProbeResult) == WPAD_ERR_NONE;

        if (hasWiiInput) {
            gamepadState.SetButtonDown(InputGamepadButton::DPadUp, (wiiButtonsHeld & WPAD_BUTTON_UP) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::DPadDown, (wiiButtonsHeld & WPAD_BUTTON_DOWN) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::DPadLeft, (wiiButtonsHeld & WPAD_BUTTON_LEFT) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::DPadRight, (wiiButtonsHeld & WPAD_BUTTON_RIGHT) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::South, (wiiButtonsHeld & WPAD_BUTTON_2) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::East, (wiiButtonsHeld & WPAD_BUTTON_1) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::Start, (wiiButtonsHeld & WPAD_BUTTON_PLUS) != 0);
            gamepadState.SetButtonDown(InputGamepadButton::Select, (wiiButtonsHeld & WPAD_BUTTON_MINUS) != 0);
            gamepadState.set_LeftStickX(0);
            gamepadState.set_LeftStickY(0);
            gamepadState.set_RightStickX(0);
            gamepadState.set_RightStickY(0);
            gamepadState.set_LeftTrigger(0);
            gamepadState.set_RightTrigger(0);
        } else {
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
        }

        gamepadState.set_Connected(isWiiConnected || hasGameCubeInput);

        (*gamepads)[0] = gamepadState;
        frame.set_Gamepads(gamepads);
        return frame;
    }
}
