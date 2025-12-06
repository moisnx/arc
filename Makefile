# Arc Editor - Makefile (CMake Wrapper)
# Provides convenient shortcuts for common build operations

# Configuration
BUILD_DIR := build
BUILD_TYPE ?= Release
CMAKE := cmake
MAKE := make
NPROCS := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Build targets
.PHONY: all configure build clean distclean run install debug release help test

# Default target
all: build

# Show help
help:
	@echo "Arc Editor - Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make              - Build the project (Release mode)"
	@echo "  make debug        - Build in Debug mode"
	@echo "  make release      - Build in Release mode"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make distclean    - Remove entire build directory"
	@echo "  make run          - Build and run the editor"
	@echo "  make install      - Install the binary (requires sudo)"
	@echo "  make configure    - Reconfigure CMake"
	@echo "  make test         - Run tests (if available)"
	@echo ""
	@echo "Configuration:"
	@echo "  BUILD_TYPE        - Release or Debug (default: Release)"
	@echo "  BUILD_DIR         - Build directory (default: build)"
	@echo ""
	@echo "Examples:"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make BUILD_TYPE=Debug run  - Build and run in debug mode"

# Configure CMake
configure:
	@echo "Configuring CMake ($(BUILD_TYPE))..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) ..

# Build the project
build: configure
	@echo "Building Arc Editor ($(BUILD_TYPE))..."
	@cd $(BUILD_DIR) && $(MAKE) -j$(NPROCS)
	@echo "Build complete: $(BUILD_DIR)/arc"

# Debug build
debug:
	@$(MAKE) BUILD_TYPE=Debug build

# Release build
release:
	@$(MAKE) BUILD_TYPE=Release build

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@if [ -d $(BUILD_DIR) ]; then cd $(BUILD_DIR) && $(MAKE) clean; fi

# Remove entire build directory
distclean:
	@echo "Removing build directory..."
	@rm -rf $(BUILD_DIR)

# Run the editor
run: build
	@echo "Running Arc Editor..."
	@./$(BUILD_DIR)/arc

# Install system-wide (requires root)
install: build
	@echo "Installing Arc Editor..."
	@cd $(BUILD_DIR) && sudo $(MAKE) install

# Run tests (if available)
test: build
	@echo "Running tests..."
	@cd $(BUILD_DIR) && ctest --output-on-failure

# Quick rebuild (skip configure)
rebuild:
	@cd $(BUILD_DIR) && $(MAKE) -j$(NPROCS)

# Format check (requires clang-format)
format:
	@echo "Formatting source files..."
	@find src -name '*.cpp' -o -name '*.h' | xargs clang-format -i

# Show build info
info:
	@echo "Build Configuration:"
	@echo "  BUILD_TYPE: $(BUILD_TYPE)"
	@echo "  BUILD_DIR:  $(BUILD_DIR)"
	@echo "  CMAKE:      $(CMAKE)"
	@echo "  Processors: $(NPROCS)"
	@if [ -d $(BUILD_DIR) ]; then \
		echo ""; \
		echo "Build Status:"; \
		if [ -f $(BUILD_DIR)/arc ]; then \
			echo "  Binary:     BUILT"; \
			ls -lh $(BUILD_DIR)/arc | awk '{print "  Size:      " $$5}'; \
		else \
			echo "  Binary:     NOT BUILT"; \
		fi; \
	else \
		echo ""; \
		echo "Build Status: Not configured"; \
	fi