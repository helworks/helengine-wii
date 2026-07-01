DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
LIBOGC ?= $(DEVKITPRO)/libogc
HELENGINE_CORE_CPP_ROOT ?=
HELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT ?= 0
HELENGINE_WII_BOOT_MODE ?= direct-dol

include $(DEVKITPPC)/wii_rules

BUILD_DIR := build
TARGET_ELF := $(BUILD_DIR)/helengine_wii.elf
TARGET_DOL := $(BUILD_DIR)/helengine_wii.dol
SOURCE_DIR := src
LIBOGC_WII_LIB_DIR := $(LIBOGC)/lib/wii
BASE_SOURCES := \
	$(SOURCE_DIR)/main.cpp \
	$(SOURCE_DIR)/platform/wii/WiiApplication.cpp
GENERATED_BRIDGE_SOURCES :=
GENERATED_CORE_SOURCE :=
GENERATED_CORE_TRANSLATION_UNIT :=
GENERATED_CONFIG := $(HELENGINE_CORE_CPP_ROOT)/helcpp_config.hpp
GENERATED_RUNTIME_SCENE_MANIFEST := $(HELENGINE_CORE_CPP_ROOT)/runtime/wii_runtime_scene_manifest.hpp
APPLOADER_SOURCE := $(SOURCE_DIR)/platform/wii/apploader/WiiDiscApploader.cpp
APPLOADER_LINKER_SCRIPT := $(SOURCE_DIR)/platform/wii/apploader/WiiDiscApploader.ld
APPLOADER_OBJECT := $(BUILD_DIR)/platform/wii/apploader/WiiDiscApploader.o
APPLOADER_ELF := $(BUILD_DIR)/helengine_wii_apploader.elf
APPLOADER_TEMPLATE := $(BUILD_DIR)/helengine_wii_apploader_template.bin

CXX := $(DEVKITPPC)/bin/powerpc-eabi-g++
ELF2DOL := $(DEVKITPRO)/tools/bin/elf2dol
OBJCOPY := $(DEVKITPPC)/bin/powerpc-eabi-objcopy
LD := $(DEVKITPPC)/bin/powerpc-eabi-ld

CPPFLAGS := \
	-I$(SOURCE_DIR) \
	-I$(LIBOGC)/include \
	-DGEKKO \
	-DHW_RVL=1 \
	-DHELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT=$(HELENGINE_WII_BATCH_VERIFY_FRAME_LIMIT)

ifeq ($(strip $(HELENGINE_WII_BOOT_MODE)),packaged-disc)
CPPFLAGS += -DHELENGINE_WII_PACKAGED_DISC_BOOT=1
else
CPPFLAGS += -DHELENGINE_WII_PACKAGED_DISC_BOOT=0
endif

ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CPPFLAGS += -DHELENGINE_WII_HAS_GENERATED_CORE=0
else
ifneq ($(wildcard $(HELENGINE_CORE_CPP_ROOT)/helengine_core_amalgamated.cpp),)
GENERATED_CORE_TRANSLATION_UNIT := helengine_core_amalgamated.cpp
else ifneq ($(wildcard $(HELENGINE_CORE_CPP_ROOT)/helengine_core_unity.cpp),)
GENERATED_CORE_TRANSLATION_UNIT := helengine_core_unity.cpp
else
$(error HELENGINE_CORE_CPP_ROOT does not contain helengine_core_amalgamated.cpp or helengine_core_unity.cpp)
endif
GENERATED_CORE_SOURCE := $(HELENGINE_CORE_CPP_ROOT)/$(GENERATED_CORE_TRANSLATION_UNIT)
ifeq ($(wildcard $(GENERATED_CONFIG)),)
$(error HELENGINE_CORE_CPP_ROOT does not contain helcpp_config.hpp)
endif
ifeq ($(shell tr -d '\r' < $(GENERATED_CONFIG) 2>/dev/null | grep -Ec '^#define HE_CPP_COMPILER_GCC 1$$'),0)
$(error HELENGINE_CORE_CPP_ROOT helcpp_config.hpp must define HE_CPP_COMPILER_GCC 1)
endif
ifeq ($(shell tr -d '\r' < $(GENERATED_CONFIG) 2>/dev/null | grep -Ec '^#define HE_CPP_PLATFORM_WII 1$$'),0)
$(error HELENGINE_CORE_CPP_ROOT helcpp_config.hpp must define HE_CPP_PLATFORM_WII 1)
endif
ifeq ($(shell tr -d '\r' < $(GENERATED_CONFIG) 2>/dev/null | grep -Ec '^#define HE_CPP_PLATFORM_IS_LITTLE_ENDIAN 0$$'),0)
$(error HELENGINE_CORE_CPP_ROOT helcpp_config.hpp must define HE_CPP_PLATFORM_IS_LITTLE_ENDIAN 0)
endif
ifeq ($(shell tr -d '\r' < $(GENERATED_CONFIG) 2>/dev/null | grep -Ec '^#define HE_CPP_PLATFORM_IS_WINDOWS_HOST 0$$'),0)
$(error HELENGINE_CORE_CPP_ROOT helcpp_config.hpp must define HE_CPP_PLATFORM_IS_WINDOWS_HOST 0)
endif
GENERATED_BRIDGE_SOURCES := \
	$(SOURCE_DIR)/platform/wii/WiiInputManager.cpp \
	$(SOURCE_DIR)/platform/wii/WiiMeshCache.cpp \
	$(SOURCE_DIR)/platform/wii/WiiRasterRenderer.cpp \
	$(SOURCE_DIR)/platform/wii/WiiRenderManager2D.cpp \
	$(SOURCE_DIR)/platform/wii/WiiRenderManager3D.cpp \
	$(SOURCE_DIR)/platform/wii/WiiSceneRenderBridge.cpp \
	$(SOURCE_DIR)/platform/wii/WiiFramePlan.cpp \
	$(SOURCE_DIR)/platform/wii/WiiRuntimeMaterial.cpp \
	$(SOURCE_DIR)/platform/wii/WiiRuntimeModel.cpp \
	$(SOURCE_DIR)/platform/wii/WiiRuntimeTexture.cpp \
	$(SOURCE_DIR)/platform/wii/WiiDiscFileSystem.cpp \
	$(SOURCE_DIR)/platform/wii/WiiSceneBootstrap.cpp
CPPFLAGS += -DHELENGINE_WII_HAS_GENERATED_CORE=1 -I$(HELENGINE_CORE_CPP_ROOT)
ifneq ($(wildcard $(GENERATED_RUNTIME_SCENE_MANIFEST)),)
CPPFLAGS += -DHELENGINE_WII_HAS_RUNTIME_SCENE_MANIFEST=1
else
CPPFLAGS += -DHELENGINE_WII_HAS_RUNTIME_SCENE_MANIFEST=0
endif
endif

ALL_SOURCE_SOURCES := $(BASE_SOURCES) $(GENERATED_BRIDGE_SOURCES)
OBJECTS := $(patsubst $(SOURCE_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(ALL_SOURCE_SOURCES))

ifneq ($(strip $(GENERATED_CORE_SOURCE)),)
OBJECTS += $(BUILD_DIR)/generated/$(GENERATED_CORE_TRANSLATION_UNIT:.cpp=.o)
endif

CXXFLAGS := \
	-std=gnu++20 \
	-O2 \
	-Wall \
	-Wextra \
	$(MACHDEP) \
	-ffunction-sections \
	-fdata-sections

APPLOADER_CXXFLAGS := \
	-std=gnu++20 \
	-O2 \
	-Wall \
	-Wextra \
	$(MACHDEP) \
	-ffreestanding \
	-fno-exceptions \
	-fno-rtti \
	-fno-unwind-tables \
	-fno-asynchronous-unwind-tables \
	-fno-threadsafe-statics \
	-fno-use-cxa-atexit \
	-ffunction-sections \
	-fdata-sections \
	-msdata=none

LDFLAGS := \
	$(MACHDEP) \
	-L$(LIBOGC_WII_LIB_DIR) \
	-L$(LIBOGC)/lib \
	-Wl,-Map,$(BUILD_DIR)/helengine_wii.map \
	-Wl,--gc-sections

APPLOADER_LDFLAGS := \
	-T $(APPLOADER_LINKER_SCRIPT) \
	-Map $(BUILD_DIR)/helengine_wii_apploader.map \
	--gc-sections

LDLIBS := \
	-lwiiuse \
	-lbte \
	-lfat \
	-logc \
	-ldi \
	-ldb \
	-lm

.PHONY: all clean

all: $(TARGET_DOL) $(APPLOADER_TEMPLATE)

$(TARGET_DOL): $(TARGET_ELF)
	$(ELF2DOL) $< $@

$(TARGET_ELF): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

$(APPLOADER_TEMPLATE): $(APPLOADER_ELF)
	$(OBJCOPY) -O binary $< $@

$(APPLOADER_ELF): $(APPLOADER_OBJECT)
	@mkdir -p $(dir $@)
	$(LD) $(APPLOADER_OBJECT) $(APPLOADER_LDFLAGS) -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(APPLOADER_OBJECT): $(APPLOADER_SOURCE) $(APPLOADER_LINKER_SCRIPT)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(APPLOADER_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/generated/$(GENERATED_CORE_TRANSLATION_UNIT:.cpp=.o): $(GENERATED_CORE_SOURCE)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR)
