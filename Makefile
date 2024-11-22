HOST_OS := $(shell uname -s | tr '[:upper:]' '[:lower:]')

# Directories
SRCDIR := src
INCDIR := include
OBJDIR := obj
BINDIR := bin
LIBDIR := lib
TARGET_LIBDIR :=

# Compiler and tools
CXX := g++
AR := ar
STRIP := strip
RANLIB :=

# Flags
CFLAGS := -Wall -Wextra -O2
CXXFLAGS := $(CFLAGS) -std=c++14 -I$(INCDIR)
# LDFLAGS := -L$(LIBDIR)
# LIBS := -lm -lpthread
# DEBUG := -g

# Files
SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET_BINNAME := ft
TARGET_BINOUT := $(BINDIR)/ft
LIB_SRCS := $(filter-out $(SRCDIR)/main.cpp, $(SRCS))
LIB_OBJS := $(LIB_SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET_LIBNAME := libDexFileTransfer.a
TARGET_LIBOUT :=

BUILD_TYPE := release

ifeq ($(BUILD_TYPE),debug)
CXXFLAGS += -DDEBUG
endif

ifeq ($(TARGET_OS), android)
# Set Android paths
ifeq ($(HOST_OS),linux)
NDK_PATH ?= /mnt/c/Users/Dexter/AppData/Local/Android/Sdk/ndk/27.0.12077973
else ifeq ($(HOST_OS),darwin)
NDK_PATH ?= ~/Library/Android/sdk/ndk/26.1.10909125/
endif
TOOLCHAIN := $(NDK_PATH)/toolchains/llvm/prebuilt/$(HOST_OS)-x86_64
SYSROOT := $(TOOLCHAIN)/sysroot

# Set OS name to 'windows' if Android sdk/ndk is on WSL
ifeq ($(shell grep -qEi "(microsoft|wsl)" /proc/version && echo WSL), WSL)
HOST_OS := windows
endif

# Set Android API level to compile against
ANDROID_API_LEVEL := 24

# Set Android target architecture (armeabi-v7a, arm64-v8a, x86, x86_64)
TARGET_ARCH ?= arm64-v8a

# Set Android compiler and flags
# CXXFLAGS += -fPIC --sysroot=$(SYSROOT) -stdlib=libc++ -I$(TOOLCHAIN)/include/c++/4.9.x
# CXXFLAGS += -fPIC --sysroot=$(SYSROOT) -stdlib=libc++ -I $(SYSROOT)/usr/include -I $(TOOLCHAIN)/include/c++/4.9.x
CXXFLAGS += -fPIC -stdlib=libc++
AR := $(TOOLCHAIN)/bin/llvm-ar
RANLIB = $(TOOLCHAIN)/bin/llvm-ranlib

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
TARGET_LIBDIR := $(LIBDIR)/$(TARGET_OS)/$(TARGET_ARCH)
TARGET_LIBOUT := $(TARGET_LIBDIR)/$(TARGET_LIBNAME)
endif # ifeq ($(TARGET_OS), android)

ifeq ($(TARGET_OS), ios)
TARGET_ARCH = arm64
# IOS_SDK = iphoneos
IOS_SDK = iphonesimulator
IOS_SDK_PATH = $(shell xcrun --sdk $(IOS_SDK) --show-sdk-path)
CXXFLAGS += -arch $(TARGET_ARCH) -isysroot $(IOS_SDK_PATH) -Wall -I$(INCDIR)
CXX := xcrun -sdk iphoneos clang++

TARGET_LIBDIR := $(LIBDIR)/$(TARGET_OS)/$(TARGET_ARCH)
TARGET_LIBOUT := $(TARGET_LIBDIR)/$(TARGET_LIBNAME)
endif

default: $(TARGET_BINOUT)

library: $(TARGET_LIBOUT)

android_libs:
	make clean
	make library TARGET_OS:=android TARGET_ARCH:=arm64-v8a
	make clean
	make library TARGET_OS:=android TARGET_ARCH:=armeabi-v7a
	make clean
	make library TARGET_OS:=android TARGET_ARCH:=x86
	make clean
	make library TARGET_OS:=android TARGET_ARCH:=x86_64
	make android_copy

android_copy:
	mkdir -p android/DexFileTransfer/app/src/main/cpp/libs/
	mkdir -p android/DexFileTransfer/app/src/main/cpp/include/
	cp -r lib/android/* android/DexFileTransfer/app/src/main/cpp/libs/
	cp include/* android/DexFileTransfer/app/src/main/cpp/include/

ios_libs:
	make clean
	make library TARGET_OS=ios TARGET_ARCH=arm64
	make ios_copy_libs

ios_copy_libs:
	mkdir -p ios/DexFileTransfer/DexFileTransfer/CPP/include
	mkdir -p ios/DexFileTransfer/DexFileTransfer/CPP/lib
	cp include/* ios/DexFileTransfer/DexFileTransfer/CPP/include
	cp -r lib/ios/* ios/DexFileTransfer/DexFileTransfer/CPP/lib

$(TARGET_BINOUT): $(OBJS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

$(TARGET_LIBOUT): $(LIB_OBJS)
	mkdir -p $(TARGET_LIBDIR)
	$(AR) rcs $@ $(LIB_OBJS)
ifeq ($(TARGET_OS), android)
	$(RANLIB) $@
endif

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJS) $(TARGET_BINOUT)

cleanAndroid:
	rm -rf $(LIBDIR)

.PHONY: default clean cleanAndroid android_libs android_copy library ios_libs \
    ios_copy_libs