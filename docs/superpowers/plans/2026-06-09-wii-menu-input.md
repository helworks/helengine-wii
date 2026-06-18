# Wii Menu Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the packaged Wii main menu navigable by feeding the existing generated menu/input stack with sideways Wii Remote input while preserving GameCube controller fallback.

**Architecture:** Extend `WiiInputManager` so it polls both `WPAD` and `PAD`, resolves one active logical controller per frame, and maps both devices into the existing `InputGamepadState` contract. Lock that contract in with focused source tests, then verify with the packaged Wii rebuild and a fresh Dolphin ISO launch.

**Tech Stack:** C++, libogc (`PAD`, `WPAD`), xUnit source-contract tests, Docker Wii native build, Dolphin ISO launch helpers

---

## File Structure

- Modify: `src/platform/wii/WiiInputManager.cpp`
  - Add `WPAD` initialization, per-frame polling, sideways Wii button mapping, and unified active-device selection.
- Modify: `src/platform/wii/WiiInputManager.hpp`
  - Keep the public backend shape stable unless small private helpers are needed for clarity.
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
  - Add one focused source-contract test for the Wii input bridge and update any string expectations needed for the backend contract.

## Task 1: Add the Failing Wii Input Source Test

**Files:**
- Modify: `builder.tests/WiiRuntimeSourceTests.cs`
- Test: `builder.tests/helengine.wii.builder.tests.csproj`

- [ ] **Step 1: Write the failing source-contract test**

Add a new test near the other Wii runtime source-contract tests:

```csharp
/// <summary>
/// Ensures the Wii input backend polls both Wii Remote and GameCube controller paths and maps sideways Wii Remote buttons into the shared logical gamepad contract.
/// </summary>
[Fact]
public void PackagedInput_UsesWiiRemoteSidewaysMappingWithGameCubeFallback() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string inputHeaderSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiInputManager.hpp"));
    string inputSource = File.ReadAllText(Path.Combine(repositoryRootPath, "src", "platform", "wii", "WiiInputManager.cpp"));

    Assert.Contains("#include <wiiuse/wpad.h>", inputSource, StringComparison.Ordinal);
    Assert.Contains("WPAD_Init();", inputSource, StringComparison.Ordinal);
    Assert.Contains("WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS);", inputSource, StringComparison.Ordinal);
    Assert.Contains("WPAD_ScanPads();", inputSource, StringComparison.Ordinal);
    Assert.Contains("const u32 wiiButtonsHeld = WPAD_ButtonsHeld(WPAD_CHAN_0);", inputSource, StringComparison.Ordinal);
    Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::DPadUp, (wiiButtonsHeld & WPAD_BUTTON_UP) != 0);", inputSource, StringComparison.Ordinal);
    Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::DPadDown, (wiiButtonsHeld & WPAD_BUTTON_DOWN) != 0);", inputSource, StringComparison.Ordinal);
    Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::DPadLeft, (wiiButtonsHeld & WPAD_BUTTON_LEFT) != 0);", inputSource, StringComparison.Ordinal);
    Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::DPadRight, (wiiButtonsHeld & WPAD_BUTTON_RIGHT) != 0);", inputSource, StringComparison.Ordinal);
    Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::South, (wiiButtonsHeld & WPAD_BUTTON_2) != 0);", inputSource, StringComparison.Ordinal);
    Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::East, (wiiButtonsHeld & WPAD_BUTTON_1) != 0);", inputSource, StringComparison.Ordinal);
    Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::Start, (wiiButtonsHeld & WPAD_BUTTON_PLUS) != 0);", inputSource, StringComparison.Ordinal);
    Assert.Contains("gamepadState.SetButtonDown(InputGamepadButton::Select, (wiiButtonsHeld & WPAD_BUTTON_MINUS) != 0);", inputSource, StringComparison.Ordinal);
    Assert.Contains("const u16 heldButtons = PAD_ButtonsHeld(0);", inputSource, StringComparison.Ordinal);
    Assert.Contains("PAD_ScanPads();", inputSource, StringComparison.Ordinal);
    Assert.Contains("InputFrameState CaptureFrame() override;", inputHeaderSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the focused test to verify it fails**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter PackagedInput_UsesWiiRemoteSidewaysMappingWithGameCubeFallback
```

Expected: `FAIL` because `WiiInputManager.cpp` does not yet include `wiiuse/wpad.h`, `WPAD_Init()`, or the Wii Remote button mappings.

- [ ] **Step 3: Commit the red test**

```bash
git add builder.tests/WiiRuntimeSourceTests.cs
git commit -m "test: add failing Wii menu input source contract"
```

## Task 2: Implement the Unified Wii/GameCube Input Bridge

**Files:**
- Modify: `src/platform/wii/WiiInputManager.cpp`
- Modify: `src/platform/wii/WiiInputManager.hpp`
- Test: `builder.tests/WiiRuntimeSourceTests.cs`

- [ ] **Step 1: Extend the Wii input backend includes and initialization**

Update the top of `src/platform/wii/WiiInputManager.cpp`:

```cpp
#include <gccore.h>
#include <wiiuse/wpad.h>
```

Update the constructor body:

```cpp
WiiInputManager::WiiInputManager() {
    PAD_Init();
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS);
}
```

- [ ] **Step 2: Replace the single-source frame capture with a unified active-device path**

In `src/platform/wii/WiiInputManager.cpp`, rework `CaptureFrame()` so it polls both backends and resolves one logical controller:

```cpp
InputFrameState WiiInputManager::CaptureFrame() {
    PAD_ScanPads();
    WPAD_ScanPads();

    InputFrameState frame;
    frame.set_GamepadCount(1);

    Array<InputGamepadState>* gamepads = new Array<InputGamepadState>(1);
    InputGamepadState gamepadState;
    gamepadState.set_Connected(true);

    const u32 wiiButtonsHeld = WPAD_ButtonsHeld(WPAD_CHAN_0);
    const u16 heldButtons = PAD_ButtonsHeld(0);
    const bool hasWiiInput = wiiButtonsHeld != 0U;
    const bool hasGameCubeInput = heldButtons != 0U || PAD_StickX(0) != 0 || PAD_StickY(0) != 0 || PAD_SubStickX(0) != 0 || PAD_SubStickY(0) != 0;

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

    gamepadState.set_Connected(hasWiiInput || hasGameCubeInput || WPAD_Probe(WPAD_CHAN_0, nullptr) == WPAD_ERR_NONE);
    (*gamepads)[0] = gamepadState;
    frame.set_Gamepads(gamepads);
    return frame;
}
```

- [ ] **Step 3: Keep the public header stable**

`src/platform/wii/WiiInputManager.hpp` should remain minimal:

```cpp
class WiiInputManager : public IInputBackend {
public:
    WiiInputManager();
    ~WiiInputManager();
    InputFrameState CaptureFrame() override;
};
```

Only add declarations if they are truly required for readability and can stay private.

- [ ] **Step 4: Run the focused source test to verify it passes**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter PackagedInput_UsesWiiRemoteSidewaysMappingWithGameCubeFallback
```

Expected: `PASS`

- [ ] **Step 5: Commit the green implementation**

```bash
git add src/platform/wii/WiiInputManager.cpp src/platform/wii/WiiInputManager.hpp builder.tests/WiiRuntimeSourceTests.cs
git commit -m "feat: add Wii menu input bridge"
```

## Task 3: Run the Packaged Verification Slice

**Files:**
- Test: `builder.tests/helengine.wii.builder.tests.csproj`

- [ ] **Step 1: Run the packaged Wii test slice**

Run:

```powershell
dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter Packaged
```

Expected: `PASS` with the packaged Wii source-contract and builder/runtime slice still green.

- [ ] **Step 2: Commit only if the packaged slice required follow-up test adjustments**

If the packaged slice exposed assertion drift and you had to tighten tests:

```bash
git add builder.tests/WiiRuntimeSourceTests.cs
git commit -m "test: align packaged Wii input coverage"
```

If no further changes were required, skip this commit.

## Task 4: Rebuild and Verify in Dolphin

**Files:**
- Runtime artifact: `build/helengine_wii.dol`
- Disc root: `tmp/self-apploader-package-v3/disc-refresh-v54`
- ISO: `tmp/self-apploader-package-v3/city-self-apploader-v54.iso`

- [ ] **Step 1: Rebuild the packaged Wii native runtime**

Run:

```powershell
rtk proxy docker run --rm -v C:/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii -e HELENGINE_WII_BOOT_MODE=packaged-disc helengine-wii sh -lc "make 2>&1 | tail -c 12000"
```

Expected: `build/helengine_wii.dol` is rebuilt successfully.

- [ ] **Step 2: Regenerate the packaged disc layout**

Run:

```powershell
rtk dotnet C:\dev\helworks\helengine-wii\builder\bin\Debug\net9.0-windows\helengine.wii.builder.dll --write-disc-layout C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\staging C:\dev\helworks\helengine-wii\build\helengine_wii.dol C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\disc-refresh-v54 RCIE01 city
```

Expected: `disc-refresh-v54` is regenerated.

- [ ] **Step 3: Verify the staged DOL matches the rebuilt DOL**

Run:

```powershell
Get-FileHash C:\dev\helworks\helengine-wii\build\helengine_wii.dol, C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\disc-refresh-v54\sys\main.dol -Algorithm SHA256
```

Expected: both hashes are identical.

- [ ] **Step 4: Package the fresh ISO**

Run:

```powershell
$env:HELENGINE_WII_WIT_PATH='C:\dev\helworks\helengine-wii\tmp\tools\wit-v3.05a-r8638-cygwin64\bin\wit.exe'
rtk dotnet C:\dev\helworks\helengine-wii\builder\bin\Debug\net9.0-windows\helengine.wii.builder.dll --package-image C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\disc-refresh-v54 C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\city-self-apploader-v54.iso
```

Expected: `city-self-apploader-v54.iso` is produced.

- [ ] **Step 5: Launch Dolphin on the fresh ISO**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-wii\tmp\launch_wii_iso_in_dolphin.ps1 -IsoPath C:\dev\helworks\helengine-wii\tmp\self-apploader-package-v3\city-self-apploader-v54.iso
```

Expected: Dolphin opens on the new ISO and reports a fresh `PROCESS_ID`.

- [ ] **Step 6: Manually verify menu navigation**

In the visible Dolphin window:

- press sideways Wii Remote `D-pad` to move between `Demo Scenes`, `Physics Scenes`, and `Options`
- press `2` to activate the selected item
- press `1` to back out if the selected flow supports going back
- optionally verify GameCube pad fallback still moves the menu

Expected: menu selection moves, confirm works, and back/cancel works where the authored menu supports it.

- [ ] **Step 7: Commit the verified runtime result**

```bash
git add src/platform/wii/WiiInputManager.cpp src/platform/wii/WiiInputManager.hpp builder.tests/WiiRuntimeSourceTests.cs
git commit -m "feat: enable Wii packaged menu navigation"
```
