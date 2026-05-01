# Nintendo Wii Bootstrap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first `helengine-wii` Dockerized devkitPPC bootstrap that targets Wii only and boots in Dolphin to a stable solid pink screen with no input dependency.

**Architecture:** Mirror the proven `helengine-gc` scaffold closely so future GC/Wii sharing stays mechanical, but hard-target Wii in the build and runtime layers. Keep the platform host boundary explicit with `main.cpp` delegating immediately to `WiiBootHost`, and preserve the generated-core seam without introducing generated code yet.

**Tech Stack:** Docker, devkitPro `devkitPPC`, libogc, GNU Make, C++17, Dolphin

---

## File Structure

- Create: `Dockerfile`
  Purpose: define the Wii build container on top of `devkitpro/devkitppc:latest` with explicit toolchain environment variables.
- Create: `Makefile`
  Purpose: compile/link a Wii-only `.dol` using `wii_rules`, explicit compiler/tool paths, explicit library search paths, and the generated-core seam macros.
- Create: `README.md`
  Purpose: document the Docker build flow, output artifact, and the expected Dolphin pink-screen verification result.
- Create: `src/main.cpp`
  Purpose: enter the Wii bootstrap and hand off immediately to `WiiBootHost`.
- Create: `src/platform/wii/WiiBootHost.hpp`
  Purpose: declare the Wii-specific boot host interface and pink-screen helpers/constants.
- Create: `src/platform/wii/WiiBootHost.cpp`
  Purpose: initialize Wii video, configure the framebuffer, paint the frame pink, and keep the frame alive.

### Task 1: Scaffold The Wii Build Surface

**Files:**
- Create: `Dockerfile`
- Create: `Makefile`

- [ ] **Step 1: Inspect the working GameCube build files before copying patterns**

Run:

```bash
rtk sed -n '1,220p' /mnt/c/dev/helworks/helengine-gc/Dockerfile
rtk sed -n '1,260p' /mnt/c/dev/helworks/helengine-gc/Makefile
```

Expected: the GameCube Dockerfile shows explicit `DEVKITPRO`/`DEVKITPPC`/`LIBOGC` environment wiring, and the GameCube Makefile shows explicit tool paths plus `gamecube_rules`/`MACHDEP`.

- [ ] **Step 2: Write the Wii Dockerfile**

Create `Dockerfile` with:

```dockerfile
FROM devkitpro/devkitppc:latest

ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITPPC=/opt/devkitpro/devkitPPC
ENV LIBOGC=/opt/devkitpro/libogc
ENV PATH=/opt/devkitpro/devkitPPC/bin:/opt/devkitpro/tools/bin:${PATH}

WORKDIR /workspace
```

- [ ] **Step 3: Write the initial Wii Makefile**

Create `Makefile` with:

```makefile
DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
LIBOGC ?= $(DEVKITPRO)/libogc
HELENGINE_CORE_CPP_ROOT ?=

include $(DEVKITPPC)/wii_rules

TARGET := helengine_wii
BUILD := build
SOURCES := \
	src \
	src/platform/wii
INCLUDES := \
	src

CXX := $(DEVKITPPC)/bin/powerpc-eabi-g++
ELF2DOL := $(DEVKITPRO)/tools/bin/elf2dol

CFLAGS := \
	-I$(CURDIR)/src \
	-I$(LIBOGC)/include \
	-DGEKKO \
	-DHW_RVL=1 \
	-std=gnu++17 \
	-O2 \
	-Wall \
	-Wextra \
	-ffunction-sections \
	-fdata-sections

ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CFLAGS += -DHELENGINE_WII_HAS_GENERATED_CORE=0
else
CFLAGS += -DHELENGINE_WII_HAS_GENERATED_CORE=1 -I$(HELENGINE_CORE_CPP_ROOT)
endif

LDFLAGS := \
	$(MACHDEP) \
	-L$(LIBOGC)/lib/wii \
	-L$(LIBOGC)/lib \
	-Wl,-Map,$(BUILD)/$(TARGET).map \
	-Wl,--gc-sections

LIBS := -logc -ldb -lm
```

- [ ] **Step 4: Add the recursive build/file discovery rules**

Append to `Makefile`:

```makefile
CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
OFILES := $(patsubst %.cpp,$(BUILD)/%.o,$(CPPFILES))

.PHONY: all clean

all: $(BUILD)/$(TARGET).dol

$(BUILD)/$(TARGET).dol: $(BUILD)/$(TARGET).elf
	@$(ELF2DOL) $< $@

$(BUILD)/$(TARGET).elf: $(OFILES)
	@$(CXX) $^ $(LDFLAGS) $(LIBS) -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD)
```

- [ ] **Step 5: Dry-run the Makefile shape before runtime code exists**

Run:

```bash
rtk make -n
```

Expected: make prints the explicit `powerpc-eabi-g++` and `elf2dol` commands and references `wii_rules`, `HW_RVL=1`, and `build/helengine_wii.*` paths without trying to use `cube.specs`.

- [ ] **Step 6: Commit the build scaffold**

Run:

```bash
rtk git add Dockerfile Makefile
rtk git commit -m "Add Wii Docker build scaffold"
```

Expected: commit succeeds with only the Dockerfile and Makefile staged.

### Task 2: Add The Wii Boot Host

**Files:**
- Create: `src/main.cpp`
- Create: `src/platform/wii/WiiBootHost.hpp`
- Create: `src/platform/wii/WiiBootHost.cpp`

- [ ] **Step 1: Write the minimal entry point**

Create `src/main.cpp` with:

```cpp
#include "platform/wii/WiiBootHost.hpp"

int main() {
	return helengine::platform::wii::WiiBootHost::Run();
}
```

- [ ] **Step 2: Declare the Wii boot host interface**

Create `src/platform/wii/WiiBootHost.hpp` with:

```cpp
#pragma once

#include <gccore.h>

namespace helengine::platform::wii {

class WiiBootHost {
public:
	static int Run();

private:
	static void* ConfigureFrameBuffer(GXRModeObj* RenderMode);
	static void ClearFrameBuffer(void* FrameBuffer, const GXRModeObj* RenderMode);
	static u32 BuildPinkPixel();
};

} // namespace helengine::platform::wii
```

- [ ] **Step 3: Implement Wii video startup and framebuffer clearing**

Create `src/platform/wii/WiiBootHost.cpp` with:

```cpp
#include "platform/wii/WiiBootHost.hpp"

namespace helengine::platform::wii {

int WiiBootHost::Run() {
	VIDEO_Init();

	GXRModeObj* RenderMode = VIDEO_GetPreferredMode(nullptr);
	void* FrameBuffer = ConfigureFrameBuffer(RenderMode);

	VIDEO_Configure(RenderMode);
	VIDEO_SetNextFramebuffer(FrameBuffer);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	if (RenderMode->viTVMode & VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}

	ClearFrameBuffer(FrameBuffer, RenderMode);
	VIDEO_SetNextFramebuffer(FrameBuffer);
	VIDEO_Flush();

	for (;;) {
		VIDEO_WaitVSync();
	}
}

void* WiiBootHost::ConfigureFrameBuffer(GXRModeObj* RenderMode) {
	return MEM_K0_TO_K1(SYS_AllocateFramebuffer(RenderMode));
}

void WiiBootHost::ClearFrameBuffer(void* FrameBuffer, const GXRModeObj* RenderMode) {
	const u32 Pixel = BuildPinkPixel();
	u32* Pixels = static_cast<u32*>(FrameBuffer);
	const u32 PixelCount = (RenderMode->fbWidth * RenderMode->xfbHeight) / 2;

	for (u32 Index = 0; Index < PixelCount; ++Index) {
		Pixels[Index] = Pixel;
	}

	DCFlushRange(FrameBuffer, RenderMode->fbWidth * RenderMode->xfbHeight * VI_DISPLAY_PIX_SZ);
}

u32 WiiBootHost::BuildPinkPixel() {
	return 0xf81ff81f;
}

} // namespace helengine::platform::wii
```

- [ ] **Step 4: Check include paths and symbols before container verification**

Run:

```bash
rtk sed -n '1,220p' src/main.cpp
rtk sed -n '1,260p' src/platform/wii/WiiBootHost.hpp
rtk sed -n '1,320p' src/platform/wii/WiiBootHost.cpp
```

Expected: `main.cpp` includes `platform/wii/WiiBootHost.hpp`, `WiiBootHost.cpp` includes the same header path, and all planned symbol names are consistent.

- [ ] **Step 5: Build in Docker to verify the boot host compiles and links**

Run:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-wii .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-wii make
```

Expected: the container emits `build/helengine_wii.elf` and `build/helengine_wii.dol`.

- [ ] **Step 6: Commit the Wii bootstrap code**

Run:

```bash
rtk git add src/main.cpp src/platform/wii/WiiBootHost.hpp src/platform/wii/WiiBootHost.cpp
rtk git commit -m "Add Wii pink-screen bootstrap"
```

Expected: commit succeeds with only the new source files staged.

### Task 3: Document The Verified Flow

**Files:**
- Create: `README.md`

- [ ] **Step 1: Write the README with the exact Docker build flow**

Create `README.md` with:

````md
# helengine-wii

Nintendo Wii bootstrap scaffold for HelEngine.

## Build

```bash
docker build -t helengine-wii .
docker run --rm -v "$PWD":/workspace -w /workspace helengine-wii make
```

If Docker credential-helper issues prevent anonymous pulls, use:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-wii .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-wii make
```

## Output

The build produces:

- `build/helengine_wii.elf`
- `build/helengine_wii.dol`

## Verification

Load `build/helengine_wii.dol` in Dolphin.

The expected first milestone result is an immediate solid pink screen with no controller or input dependency.

## Generated Core Seam

`HELENGINE_CORE_CPP_ROOT` is reserved for future generated core code. When unset, the bootstrap builds with `HELENGINE_WII_HAS_GENERATED_CORE=0`.
````

- [ ] **Step 2: Check the README for command accuracy against the Makefile and Dockerfile**

Run:

```bash
rtk sed -n '1,240p' README.md
rtk sed -n '1,260p' Makefile
rtk sed -n '1,200p' Dockerfile
```

Expected: image name, output filenames, generated-core macro name, and Docker commands all match the actual scaffold.

- [ ] **Step 3: Commit the documentation**

Run:

```bash
rtk git add README.md
rtk git commit -m "Document Wii bootstrap workflow"
```

Expected: commit succeeds with the README only.

### Task 4: Final Verification And Cleanup

**Files:**
- Verify: `Dockerfile`
- Verify: `Makefile`
- Verify: `README.md`
- Verify: `src/main.cpp`
- Verify: `src/platform/wii/WiiBootHost.hpp`
- Verify: `src/platform/wii/WiiBootHost.cpp`

- [ ] **Step 1: Run the full container build again from a clean state**

Run:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-wii make clean
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-wii .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-wii make
```

Expected: clean removes `build/`, the image rebuilds successfully, and `make` regenerates `build/helengine_wii.elf` and `build/helengine_wii.dol`.

- [ ] **Step 2: Verify the runtime result in Dolphin**

Run manually:

```text
Open build/helengine_wii.dol in Dolphin and observe the first rendered frame.
```

Expected: Dolphin immediately shows a stable solid pink screen with no required input.

- [ ] **Step 3: Confirm the repo only contains valid tracked files**

Run:

```bash
rtk git status --short
```

Expected: no unexpected untracked temp files; only intentional build artifacts in ignored paths such as `build/`.

- [ ] **Step 4: Create the final scaffold commit if any fixes were made during verification**

Run:

```bash
rtk git add Dockerfile Makefile README.md src/main.cpp src/platform/wii/WiiBootHost.hpp src/platform/wii/WiiBootHost.cpp
rtk git commit -m "Polish Wii bootstrap verification"
```

Expected: if verification required no code changes, skip this step; otherwise create one last small fix commit.

## Self-Review

- Spec coverage check:
  - Docker/devkitPPC Wii build: covered by Task 1 and Task 4
  - Wii-only `wii_rules` / `HW_RVL=1` / `MACHDEP`: covered by Task 1
  - Generated-core seam: covered by Task 1 and Task 3
  - Wii boot host and immediate pink screen: covered by Task 2 and Task 4
  - Dolphin verification: covered by Task 4
- Placeholder scan:
  - No `TODO`, `TBD`, or “similar to Task N” placeholders remain
- Type/name consistency:
  - All planned source files use `WiiBootHost`
  - Output names are consistently `helengine_wii.elf` and `helengine_wii.dol`
  - Macro name is consistently `HELENGINE_WII_HAS_GENERATED_CORE`
