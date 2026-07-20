# Wii Multi-Device Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add concurrent Wiimote+Nunchuk and GameCube input on Wii while keeping physical devices separate and allowing Demo Disc actions to use either source.

**Architecture:** `WiiInputManager` will publish Wiimote slot 0, Nunchuk slot 1, and one separate slot for each GameCube port (slots 2–5) through the existing gamepad-slot contract. A reusable Demo Disc input helper will aggregate connected gamepad entries for menu navigation, action buttons, tilt, and camera controls without changing device identity in the engine.

**Tech Stack:** C++17/libogc (`WPAD`, `PAD`), generated Helengine input contracts, C# Demo Disc gameplay/menu components, xUnit source and behavior tests.

---

### Task 1: Add failing Wii native input coverage

**Files:**
- Modify: `C:\dev\helworks\helengine-wii\builder.tests\WiiRuntimeSourceTests.cs`
- Test source: `C:\dev\helworks\helengine-wii\src\platform\wii\WiiInputManager.cpp`

- [ ] Add a source test that requires `WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);`, `WPAD_Data(WPAD_CHAN_0)`, `WPAD_EXP_NUNCHUK`, `WPAD_BUTTON_A`, `WPAD_BUTTON_B`, `gforce`, `btns`, `PAD_ScanPads()`, and a loop over all four GameCube ports.
- [ ] Add assertions that the native source creates more than one `InputGamepadState` entry and does not use an `if (hasWiiInput) ... else ...` branch that suppresses GameCube polling.
- [ ] Run:
  `dotnet test C:\dev\helworks\helengine-wii\builder.tests\helengine.wii.builder.tests.csproj --no-restore --filter FullyQualifiedName~WiiRuntimeSourceTests -v minimal`
  Expected: the new input test fails because the current backend polls buttons-only Wiimote data, emits one state, and uses the mutually exclusive branch.

### Task 2: Implement separate Wiimote/Nunchuk and GameCube device capture

**Files:**
- Modify: `C:\dev\helworks\helengine-wii\src\platform\wii\WiiInputManager.cpp`
- Modify: `C:\dev\helworks\helengine-wii\src\platform\wii\WiiInputManager.hpp` only if a private helper declaration is required
- Test: `C:\dev\helworks\helengine-wii\builder.tests\WiiRuntimeSourceTests.cs`

- [ ] Configure Wiimote channel 0 for a data format containing extension information and call `WPAD_Data` once per frame.
- [ ] Emit Wiimote state at a stable slot, mapping A to `South`, B to `East`, and the Wiimote D-pad to the four D-pad buttons. Keep 1/2, Plus, and Minus available in the native state.
- [ ] When the extension type is `WPAD_EXP_NUNCHUK`, emit a separate slot containing the Nunchuk stick axes using the existing signed-axis convention; mark that slot connected only while the Nunchuk extension is present.
- [ ] Poll `PAD_ScanPads()` once, iterate ports 0 through 3, and emit one state per port. Preserve the existing GameCube button, stick, and trigger mappings, including inverted left-stick Y.
- [ ] Keep disconnected slots represented consistently so a missing Nunchuk or controller is not an exception and does not erase another device’s state.
- [ ] Run the Task 1 test and the focused Wii source tests; expected result is pass for the new native input contract.
- [ ] Commit: `feat: add separate Wii controller input capture`.

### Task 3: Add Demo Disc multi-device action aggregation tests

**Files:**
- Create: `C:\dev\helprojs\demodisc\assets\codebase\game\DemoDiscGamepadInput.cs`
- Create or modify: `C:\dev\helprojs\demodisc\assets\codebase\gameplay.tests\DemoDiscGamepadInputTests.cs`
- Modify: the Demo Disc gameplay test project file only if the new test file is not covered by its existing glob

- [ ] Define a stateless helper whose methods inspect every connected gamepad in `Core.Instance.Input`, not just index 0. Required methods are `IsButtonDown`, `WasButtonPressed`, `GetLeftStickX`, and `GetLeftStickY`; button methods return true when any connected device supplies the control, and axis methods select the connected value with the greatest absolute magnitude.
- [ ] Write tests with two fake gamepads proving Wiimote A/GameCube A both activate the same action, Wiimote B/GameCube B both activate Menu/Back, D-pad inputs are merged, and the Nunchuk/GameCube left-stick values are merged without device collapse.
- [ ] Write a test proving disconnected slots do not contribute input.
- [ ] Run the focused Demo Disc gameplay tests before changing consumers; expected result is pass for the helper tests.

### Task 4: Route Demo Disc menu and gameplay consumers through the aggregator

**Files:**
- Modify: `C:\dev\helprojs\demodisc\assets\codebase\menu\MenuComponent.cs`
- Modify: `C:\dev\helprojs\demodisc\assets\codebase\menu\DemoDiscReturnToMenuComponent.cs`
- Modify: `C:\dev\helprojs\demodisc\assets\codebase\game\TiltTrialLevelSelectComponent.cs`
- Modify: `C:\dev\helprojs\demodisc\assets\codebase\game\TiltTrialSessionComponent.cs`
- Modify: `C:\dev\helprojs\demodisc\assets\codebase\game\DemoTiltStageComponent.cs`
- Modify: `C:\dev\helprojs\demodisc\assets\codebase\game\DemoTiltFollowCameraComponent.cs`
- Modify: relevant Demo Disc source tests under `C:\dev\helprojs\demodisc\assets\codebase\*tests`

- [ ] Replace direct index-0 button checks used for menu navigation, Play, Menu/Back, and selector movement with `DemoDiscGamepadInput` calls.
- [ ] Replace direct index-0 left/right stick reads used for tilt and camera orbit with the helper’s strongest connected-axis result.
- [ ] Preserve keyboard bindings and existing per-platform button semantics; this task only broadens gamepad sources.
- [ ] Add source assertions preventing new Demo Disc navigation paths from hardcoding gamepad index 0.
- [ ] Run the smallest Demo Disc test set covering menu, level selection, tilt session, and camera input.
- [ ] Commit: `feat: merge Wii controller sources for Demo Disc actions`.

### Task 5: Update Wii Demo Disc hint text and input documentation

**Files:**
- Modify: `C:\dev\helprojs\demodisc\assets\codebase\game\TiltTrialLevelSelectComponent.cs`
- Modify: `C:\dev\helprojs\demodisc\assets\codebase\game\TiltTrialSessionComponent.cs` or the shared hint owner found by its existing tests
- Modify: related source tests
- Modify: `C:\dev\helworks\helengine-wii\docs\superpowers\specs\2026-07-20-wii-multi-device-input-design.md` only if implementation details need clarification

- [ ] Change the Wii selector hint from `2 Play   1 Menu` to exactly `A Play   B Menu`.
- [ ] Add Wii navigation wording that identifies both D-pad and Nunchuk stick where the existing UI has room, without changing DS/3DS layout rules.
- [ ] Add tests proving Wii hints mention A/B and do not mention 1/2.
- [ ] Run the focused hint and menu tests.

### Task 6: Build and validate the Wii packaged demo

**Files:**
- No source changes expected unless validation exposes a concrete failure.

- [ ] Run the streamed Wii build:
  `C:\dev\helworks\helengine\scripts\build-platform.ps1 -Project C:\dev\helprojs\demodisc\project.heproj -Platform wii -Output C:\dev\helprojs\demodisc\output\wii-multi-device-input-20260720`
- [ ] Confirm `wii-build-phase.txt` reaches `native build completed`, `disc image packaged`, and `packaged outputs verified`.
- [ ] Launch the resulting ISO in full Dolphin UI, not batch mode, with the Dolphin log window available.
- [ ] Validate that either Wiimote A/B or GameCube A/B can enter/back out of the Demo Disc menu, both D-pad and Nunchuk stick navigate, and the GameCube stick remains usable in Tilt Play.
- [ ] Inspect Dolphin OSReport/runtime traces for input or crash errors before claiming completion.
- [ ] Commit any final test-only adjustments separately; do not commit generated cooked output.
