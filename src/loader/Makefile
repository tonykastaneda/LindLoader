# Makefile for building linuxloader on Linux and Win32

.PHONY: all linux win32 clean

# Build directories
BUILD_LINUX = build-linux
BUILD_WIN32 = build-win32

# Default target: build both
all: linux win32

# Linux build
linux:
	@echo "Building for Linux..."
	@mkdir -p $(BUILD_LINUX)
	@cd $(BUILD_LINUX) && cmake .. && $(MAKE)
	@echo "Linux build complete: $(BUILD_LINUX)/"

# Win32 build (cross-compile using MinGW)
win32:
	@echo "Building for Win32..."
	@mkdir -p $(BUILD_WIN32)
	@cd $(BUILD_WIN32) && cmake .. -DCMAKE_TOOLCHAIN_FILE=../mingw32.cmake && $(MAKE)
	@echo "Win32 build complete: $(BUILD_WIN32)/"

# Clean all build directories
clean:
	@echo "Cleaning build directories..."
	rm -rf $(BUILD_LINUX) $(BUILD_WIN32)
	@echo "Clean complete."
