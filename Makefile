DEVKITPRO ?= /opt/devkitpro
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
LIBOGC ?= $(DEVKITPRO)/libogc
HELENGINE_CORE_CPP_ROOT ?=

include $(DEVKITPPC)/wii_rules

BUILD_DIR := build
TARGET_ELF := $(BUILD_DIR)/helengine_wii.elf
TARGET_DOL := $(BUILD_DIR)/helengine_wii.dol
SOURCE_DIR := src
LIBOGC_WII_LIB_DIR := $(LIBOGC)/lib/wii
SOURCES := \
	$(SOURCE_DIR)/main.cpp \
	$(SOURCE_DIR)/platform/wii/WiiBootHost.cpp
OBJECTS := $(patsubst $(SOURCE_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

CXX := $(DEVKITPPC)/bin/powerpc-eabi-g++
ELF2DOL := $(DEVKITPRO)/tools/bin/elf2dol

CPPFLAGS := \
	-I$(SOURCE_DIR) \
	-I$(LIBOGC)/include \
	-DGEKKO \
	-DHW_RVL=1

ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CPPFLAGS += -DHELENGINE_WII_HAS_GENERATED_CORE=0
else
CPPFLAGS += -DHELENGINE_WII_HAS_GENERATED_CORE=1 -I$(HELENGINE_CORE_CPP_ROOT)
endif

CXXFLAGS := \
	-std=gnu++17 \
	-O2 \
	-Wall \
	-Wextra \
	$(MACHDEP) \
	-ffunction-sections \
	-fdata-sections

LDFLAGS := \
	$(MACHDEP) \
	-L$(LIBOGC_WII_LIB_DIR) \
	-L$(LIBOGC)/lib \
	-Wl,-Map,$(BUILD_DIR)/helengine_wii.map \
	-Wl,--gc-sections

LDLIBS := \
	-logc \
	-ldb \
	-lm

.PHONY: all clean

all: $(TARGET_DOL)

$(TARGET_DOL): $(TARGET_ELF)
	$(ELF2DOL) $< $@

$(TARGET_ELF): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR)
