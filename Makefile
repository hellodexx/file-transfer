CXX := g++
CXXFLAGS := -std=c++14 -Wall -O3
INCLUDE := -Iinclude
LDFLAGS :=
BUILD_TYPE := release

SRC_DIR := src
SRC := $(wildcard $(SRC_DIR)/*.cpp)
LIB_SRC := $(filter-out $(SRC_DIR)/main.cpp, $(SRC))
OBJ := $(patsubst $(SRC_DIR)/%.cpp, $(SRC_DIR)/%.o, $(SRC))
LIB_OBJ := $(patsubst $(SRC_DIR)/%.cpp, $(SRC_DIR)/%.o, $(LIB_SRC))

BIN_DIR := bin
TARGET := ft
BIN := $(BIN_DIR)/$(TARGET)

LIB_DIR := lib
LIB_NAME := libDexFileTransfer.a
LIB := $(LIB_DIR)/$(LIB_NAME)

ifeq ($(BUILD_TYPE),debug)
CXXFLAGS += -DDEBUG
endif

OS_NAME := $(shell uname -s | tr '[:upper:]' '[:lower:]')

ifeq ($(OS_NAME),linux)
    # Commands for Linux
    PLATFORM := linux
else ifeq ($(OS_NAME),darwin)
    # Commands for macOS
    PLATFORM := macos
else ifeq ($(OS_NAME),windows_nt)
    # Commands for Windows
    PLATFORM := windows
else
    # Other platforms
    PLATFORM := unknown
endif

ifeq ($(BUILD_OS), android)
# Android 
# Set the path to your Android NDK
# NDK_PATH := /path/to/your/android-ndk
ifeq ($(OS_NAME),linux)
NDK_PATH := /mnt/c/Users/Dexter/AppData/Local/Android/Sdk/ndk/27.0.12077973
else ifeq ($(OS_NAME),darwin)
NDK_PATH := ~/Library/Android/sdk/ndk/26.1.10909125/
endif

# Set the Android API level to compile against
ANDROID_API_LEVEL := 24
# Set the target architecture (armeabi-v7a, arm64-v8a, x86, x86_64)
TARGET_ARCH ?= arm64-v8a
# Set the compiler and toolchain paths
TOOLCHAIN := $(NDK_PATH)/toolchains/llvm/prebuilt/$(OS_NAME)-x86_64
CXX := $(TOOLCHAIN)/bin/aarch64-linux-android$(ANDROID_API_LEVEL)-clang++
CXXFLAGS += -fPIC

# Determine the toolchain based on the target architecture
ifeq ($(TARGET_ARCH), armeabi-v7a)
    CXX := $(TOOLCHAIN)/bin/armv7a-linux-androideabi$(ANDROID_API_LEVEL)-clang++
else ifeq ($(TARGET_ARCH), arm64-v8a)
    CXX := $(TOOLCHAIN)/bin/aarch64-linux-android$(ANDROID_API_LEVEL)-clang++
else ifeq ($(TARGET_ARCH), x86)
    CXX := $(TOOLCHAIN)/bin/i686-linux-android$(ANDROID_API_LEVEL)-clang++
else ifeq ($(TARGET_ARCH), x86_64)
    CXX := $(TOOLCHAIN)/bin/x86_64-linux-android$(ANDROID_API_LEVEL)-clang++
endif

# Set the android lib directory
LIB_DIR := lib/$(TARGET_ARCH)
LIB := $(LIB_DIR)/$(LIB_NAME)

endif # ifeq ($(BUILD_OS), android)

default: $(BIN)

lib: $(LIB)

android_libs:
	make clean
	make lib BUILD_OS:=android TARGET_ARCH:=arm64-v8a
	make clean
	make lib BUILD_OS:=android TARGET_ARCH:=armeabi-v7a
	make clean
	make lib BUILD_OS:=android TARGET_ARCH:=x86
	make clean
	make lib BUILD_OS:=android TARGET_ARCH:=x86_64
	make android_copy

android_copy:
	cp -r lib/ android/DexFileTransfer/app/src/main/cpp/libs/
	cp include/* android/DexFileTransfer/app/src/main/cpp/include/

$(BIN): $(OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)

$(LIB): $(LIB_OBJ)
	mkdir -p $(LIB_DIR)
	ar rcs $(LIB) $(LIB_OBJ)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJ) $(BIN) $(LIB) $(SRC_DIR)/*.o

.PHONY: default clean android_libs android_copy