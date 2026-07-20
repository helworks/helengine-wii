#include "platform/wii/WiiInputManager.hpp"

#include <algorithm>

#include <gccore.h>
#include <wiiuse/wpad.h>

#include "InputGamepadButton.hpp"
#include "InputGamepadState.hpp"
#include "runtime/array.hpp"

namespace helengine::wii {
    namespace {
        /// Converts one calibrated Nunchuk axis into the shared signed 16-bit stick range.
        int16_t ConvertNunchukAxis(s8 position, s8 center, bool invert) {
            const int32_t offset = static_cast<int32_t>(position) - static_cast<int32_t>(center);
            const int32_t signedValue = invert ? -offset : offset;
            return static_cast<int16_t>(std::clamp(signedValue * 256, -32768, 32767));
        }
    }

    /// Creates the Wii input backend and initializes libogc controller polling.
    WiiInputManager::WiiInputManager() {
        PAD_Init();
        WPAD_Init();
        WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
    }

    /// Releases the Wii input backend.
    WiiInputManager::~WiiInputManager() {
    }

    /// Captures Wiimote, Nunchuk, and GameCube controller states as separate engine gamepad slots.
    InputFrameState WiiInputManager::CaptureFrame() {
        PAD_ScanPads();
        WPAD_ScanPads();

        InputFrameState frame;
        frame.set_GamepadCount(6);
        Array<InputGamepadState>* gamepads = new Array<InputGamepadState>(6);

        u32 wiiProbeResult = 0U;
        const bool isWiiConnected = WPAD_Probe(WPAD_CHAN_0, &wiiProbeResult) == WPAD_ERR_NONE;
        const u32 wiiButtonsHeld = WPAD_ButtonsHeld(WPAD_CHAN_0);
        WPADData* wiiData = WPAD_Data(WPAD_CHAN_0);

        InputGamepadState wiimoteState;
        wiimoteState.SetButtonDown(InputGamepadButton::DPadUp, (wiiButtonsHeld & WPAD_BUTTON_UP) != 0);
        wiimoteState.SetButtonDown(InputGamepadButton::DPadDown, (wiiButtonsHeld & WPAD_BUTTON_DOWN) != 0);
        wiimoteState.SetButtonDown(InputGamepadButton::DPadLeft, (wiiButtonsHeld & WPAD_BUTTON_LEFT) != 0);
        wiimoteState.SetButtonDown(InputGamepadButton::DPadRight, (wiiButtonsHeld & WPAD_BUTTON_RIGHT) != 0);
        wiimoteState.SetButtonDown(InputGamepadButton::South, (wiiButtonsHeld & WPAD_BUTTON_A) != 0);
        wiimoteState.SetButtonDown(InputGamepadButton::East, (wiiButtonsHeld & WPAD_BUTTON_B) != 0);
        wiimoteState.SetButtonDown(InputGamepadButton::West, (wiiButtonsHeld & WPAD_BUTTON_1) != 0);
        wiimoteState.SetButtonDown(InputGamepadButton::North, (wiiButtonsHeld & WPAD_BUTTON_2) != 0);
        wiimoteState.SetButtonDown(InputGamepadButton::Start, (wiiButtonsHeld & WPAD_BUTTON_PLUS) != 0);
        wiimoteState.SetButtonDown(InputGamepadButton::Select, (wiiButtonsHeld & WPAD_BUTTON_MINUS) != 0);
        wiimoteState.set_Connected(isWiiConnected);
        (*gamepads)[0] = wiimoteState;

        InputGamepadState nunchukState;
        const bool hasNunchuk = isWiiConnected && wiiData != nullptr && wiiData->exp.type == WPAD_EXP_NUNCHUK;
        if (hasNunchuk) {
            nunchukState.set_LeftStickX(ConvertNunchukAxis(wiiData->exp.nunchuk.js.pos.x, wiiData->exp.nunchuk.js.center.x, false));
            nunchukState.set_LeftStickY(ConvertNunchukAxis(wiiData->exp.nunchuk.js.pos.y, wiiData->exp.nunchuk.js.center.y, true));
        }
        nunchukState.set_Connected(hasNunchuk);
        (*gamepads)[1] = nunchukState;

        for (int controllerIndex = 0; controllerIndex < 4; controllerIndex++) {
            const u16 heldButtons = PAD_ButtonsHeld(controllerIndex);
            const bool hasGameCubeInput = heldButtons != 0U
                || PAD_StickX(controllerIndex) != 0
                || PAD_StickY(controllerIndex) != 0
                || PAD_SubStickX(controllerIndex) != 0
                || PAD_SubStickY(controllerIndex) != 0;
            InputGamepadState gameCubeState;
            gameCubeState.SetButtonDown(InputGamepadButton::DPadUp, (heldButtons & PAD_BUTTON_UP) != 0);
            gameCubeState.SetButtonDown(InputGamepadButton::DPadDown, (heldButtons & PAD_BUTTON_DOWN) != 0);
            gameCubeState.SetButtonDown(InputGamepadButton::DPadLeft, (heldButtons & PAD_BUTTON_LEFT) != 0);
            gameCubeState.SetButtonDown(InputGamepadButton::DPadRight, (heldButtons & PAD_BUTTON_RIGHT) != 0);
            gameCubeState.SetButtonDown(InputGamepadButton::South, (heldButtons & PAD_BUTTON_A) != 0);
            gameCubeState.SetButtonDown(InputGamepadButton::East, (heldButtons & PAD_BUTTON_B) != 0);
            gameCubeState.SetButtonDown(InputGamepadButton::West, (heldButtons & PAD_BUTTON_X) != 0);
            gameCubeState.SetButtonDown(InputGamepadButton::North, (heldButtons & PAD_BUTTON_Y) != 0);
            gameCubeState.SetButtonDown(InputGamepadButton::LeftShoulder, (heldButtons & PAD_TRIGGER_L) != 0);
            gameCubeState.SetButtonDown(InputGamepadButton::RightShoulder, (heldButtons & PAD_TRIGGER_R) != 0);
            gameCubeState.SetButtonDown(InputGamepadButton::Start, (heldButtons & PAD_BUTTON_START) != 0);
            gameCubeState.SetButtonDown(InputGamepadButton::Select, (heldButtons & PAD_TRIGGER_Z) != 0);
            gameCubeState.set_LeftStickX(static_cast<int16_t>(PAD_StickX(controllerIndex) * 256));
            gameCubeState.set_LeftStickY(static_cast<int16_t>(-PAD_StickY(controllerIndex) * 256));
            gameCubeState.set_RightStickX(static_cast<int16_t>(PAD_SubStickX(controllerIndex) * 256));
            gameCubeState.set_RightStickY(static_cast<int16_t>(PAD_SubStickY(controllerIndex) * 256));
            gameCubeState.set_LeftTrigger(static_cast<int16_t>(PAD_TriggerL(controllerIndex) * 256));
            gameCubeState.set_RightTrigger(static_cast<int16_t>(PAD_TriggerR(controllerIndex) * 256));
            gameCubeState.set_Connected(hasGameCubeInput);
            (*gamepads)[controllerIndex + 2] = gameCubeState;
        }

        frame.set_Gamepads(gamepads);
        return frame;
    }
}
