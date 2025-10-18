# --- Build Configuration Variables ---
# Default build directory for CMake out-of-source builds
BUILD_DIR ?= build

# Default build type. Use 'Release' for production.
# Other common options: Debug, RelWithDebInfo, MinSizeRel
BUILD_TYPE ?= Debug

# --- Installation Variables ---
# Installation directories (Standard GNU conventions)
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share/arc
QUERYDIR = $(DATADIR)/queries

# --- Core Build Targets ---

# The default target. It configures and builds the project with the set BUILD_TYPE.
all: $(BUILD_DIR)/arc
.PHONY: all

# Main build rule: CMake configure, then build
$(BUILD_DIR)/arc:
	@echo "🛠️  Configuring CMake project with BUILD_TYPE=$(BUILD_TYPE)..."
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	@echo "🚀 Starting build..."
	@cmake --build $(BUILD_DIR) --target arc -j $$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "✅ Build complete: $(BUILD_DIR)/arc"

# Clean build artifacts
clean:
	@echo "🧹 Cleaning build directory: $(BUILD_DIR)"
	@if [ -d "$(BUILD_DIR)" ]; then \
		cmake --build $(BUILD_DIR) --target clean 2>/dev/null || rm -rf $(BUILD_DIR); \
	else \
		echo "Build directory not found, nothing to clean."; \
	fi
.PHONY: clean

# Force a rebuild from scratch (clean and then build)
rebuild: clean all
.PHONY: rebuild

# --- Convenience Targets for Build Types ---

# Production Build (Optimization: -O2, No Debug info)
release:
	@make BUILD_TYPE=Release all
.PHONY: release

# Debug Build (Optimization: -O0, Full Debug info)
debug:
	@make BUILD_TYPE=Debug all
.PHONY: debug

# --- Installation Targets ---

# Standard system-wide install (requires 'sudo' unless PREFIX is local)
install: $(BUILD_DIR)/arc
	@echo "📦 Installing Arc Editor to $(PREFIX)..."
	@install -d $(DESTDIR)$(BINDIR)
	@install -m 755 $< $(DESTDIR)$(BINDIR)/arc
	@echo "✅ Installed binary to $(BINDIR)/arc"
	
	@echo "📦 Installing query files to $(QUERYDIR)..."
	@install -d $(DESTDIR)$(QUERYDIR)
	@for lang in runtime/queries/*; do \
		if [ -d "$$lang" ]; then \
			lang_name=$$(basename $$lang); \
			echo "  - Installing $$lang_name queries..."; \
			install -d $(DESTDIR)$(QUERYDIR)/$$lang_name; \
			install -m 644 $$lang/*.scm $(DESTDIR)$(QUERYDIR)/$$lang_name/ 2>/dev/null || true; \
		fi \
	done
	@echo "✅ Installed query files to $(QUERYDIR)"
	
	@echo "📦 Installing language registry..."
	@install -d $(DESTDIR)$(DATADIR)
	@install -m 644 runtime/languages.yaml $(DESTDIR)$(DATADIR)/languages.yaml
	@echo "✅ Installation complete!"
.PHONY: install

# Install to user directory (~/.local/bin)
install-user: $(BUILD_DIR)/arc
	@echo "📦 Installing Arc Editor (user mode)..."
	@install -d $(HOME)/.local/bin
	@install -m 755 $< $(HOME)/.local/bin/arc
	@echo "✅ Installed binary to $$(HOME)/.local/bin/arc"
	
	@echo "📦 Installing query files..."
	@install -d $(HOME)/.local/share/arc/queries
	@for lang in runtime/queries/*; do \
		if [ -d "$$lang" ]; then \
			lang_name=$$(basename $$lang); \
			echo "  - Installing $$lang_name queries..."; \
			install -d $(HOME)/.local/share/arc/queries/$$lang_name; \
			install -m 644 $$lang/*.scm $(HOME)/.local/share/arc/queries/$$lang_name/ 2>/dev/null || true; \
		fi \
	done
	@echo "✅ Installation complete!"
	@echo ""
	@echo "⚠️  Make sure $$(HOME)/.local/bin is in your PATH."
.PHONY: install-user

# Uninstall from system-wide location
uninstall:
	@echo "🗑️  Uninstalling Arc Editor..."
	@rm -f $(DESTDIR)$(BINDIR)/arc
	@rm -rf $(DESTDIR)$(DATADIR)
	@echo "✅ Uninstalled"
.PHONY: uninstall

# Test installation
test-install:
	@echo "🧪 Testing query resolution..."
	@echo ""
	@echo "Binary location: $$(which arc 2>/dev/null || echo 'NOT IN PATH')"
	@echo ""
	@echo "Checking query search paths:"
	@echo "  1. User config: $$(HOME)/.config/arceditor/queries/"
	@ls -la $$(HOME)/.config/arceditor/queries/ 2>/dev/null && echo "    ✅ Found" || echo "    ❌ Not found (OK - this is for custom queries)"
	@echo ""
	@echo "  2. System install: $(PREFIX)/share/arc/queries/"
	@ls -la $(PREFIX)/share/arc/queries/ 2>/dev/null && echo "    ✅ Found" || echo "    ❌ Not found"
	@echo ""
	@echo "  3. User install: $$(HOME)/.local/share/arc/queries/"
	@ls -la $$(HOME)/.local/share/arc/queries/ 2>/dev/null && echo "    ✅ Found" || echo "    ❌ Not found"
	@echo ""
	@echo "  4. Embedded: Built into binary"
	@echo "    ✅ Always available"
	@echo ""
	@echo "Test: arc should work from any directory"
	@cd /tmp && arc --version 2>/dev/null && echo "✅ Works!" || echo "⚠️  Check your installation"
.PHONY: test-install