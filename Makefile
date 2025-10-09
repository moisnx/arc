# ================================================================
# Arc Editor Makefile — Cross-Platform CMake Wrapper
# ================================================================

# Default settings
BUILD_DIR := build
BUILD_TYPE ?= Release
TARGET := arc

# Detect OS and compiler
ifeq ($(OS),Windows_NT)
    PLATFORM := Windows
else
    PLATFORM := $(shell uname -s)
endif

# Detect whether we’re using MSVC or something else
ifdef VisualStudioVersion
    COMPILER := MSVC
else
    # Assume GCC/Clang on Linux/macOS, MinGW on Windows
    COMPILER := GCC
endif

# Pick generator (for CMake)
ifeq ($(COMPILER),MSVC)
    GENERATOR := "NMake Makefiles"
else ifeq ($(PLATFORM),Windows)
    GENERATOR := "MinGW Makefiles"
else
    GENERATOR := "Unix Makefiles"
endif

# Compiler overrides (optional)
CXX ?= g++
CC ?= gcc

# CMake configuration flags
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
CMAKE_FLAGS += -DCMAKE_CXX_COMPILER="$(CXX)"
CMAKE_FLAGS += -DCMAKE_C_COMPILER="$(CC)"

# ================================================================
# Targets
# ================================================================

.PHONY: all configure build run clean rebuild debug release info tree

# ------------------------------------------------
# 1. Configure CMake project
# ------------------------------------------------
configure:
	@echo "🔧 Configuring Arc Editor..."
	@cmake -S . -B $(BUILD_DIR) -G $(GENERATOR) $(CMAKE_FLAGS)

# ------------------------------------------------
# 2. Build project
# ------------------------------------------------
build: configure
	@echo "🚀 Building Arc Editor ($(BUILD_TYPE))..."
ifeq ($(COMPILER),MSVC)
	@cmake --build $(BUILD_DIR) --config $(BUILD_TYPE) -- /m
else
	@cmake --build $(BUILD_DIR) --config $(BUILD_TYPE) -j
endif

# ------------------------------------------------
# 3. Run executable
# ------------------------------------------------
run: build
	@echo "▶️ Running Arc..."
ifeq ($(PLATFORM),Windows)
	@$(BUILD_DIR)/$(TARGET).exe
else
	@$(BUILD_DIR)/$(TARGET)
endif

# ------------------------------------------------
# 4. Clean build files
# ------------------------------------------------
clean:
	@echo "🧹 Cleaning build directory..."
ifeq ($(COMPILER),MSVC)
	@cmake --build $(BUILD_DIR) --target clean --config $(BUILD_TYPE) || true
else
	@cmake --build $(BUILD_DIR) --target clean || true
endif
	@rm -rf $(BUILD_DIR)

# ------------------------------------------------
# 5. Rebuild (clean + build)
# ------------------------------------------------
rebuild: clean all

# ------------------------------------------------
# 6. Debug/Release quick targets
# ------------------------------------------------
debug:
	@$(MAKE) BUILD_TYPE=Debug build

release:
	@$(MAKE) BUILD_TYPE=Release build

# ------------------------------------------------
# 7. Show basic build info
# ------------------------------------------------
info:
	@echo "========================================"
	@echo "🧩 Arc Editor Build Info"
	@echo "Platform: $(PLATFORM)"
	@echo "Compiler: $(COMPILER)"
	@echo "Generator: $(GENERATOR)"
	@echo "Build Type: $(BUILD_TYPE)"
	@echo "Build Directory: $(BUILD_DIR)"
	@echo "========================================"

# ------------------------------------------------
# 8. Show Tree-sitter parsers (if any)
# ------------------------------------------------
tree:
	@echo "🌳 Listing Tree-sitter parsers..."
	@grep -E "Built parser:|Registered parsers" $(BUILD_DIR)/CMakeOutput.log 2>/dev/null || echo "No parsers found (build first)."

# ------------------------------------------------
# Default target
# ------------------------------------------------
all: build
