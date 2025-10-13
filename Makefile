# Add these targets to your existing Makefile

# Installation directories
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share/arc
QUERYDIR = $(DATADIR)/queries

# Install the binary and runtime files
install: build/arc
	@echo "📦 Installing Arc Editor..."
	@install -d $(DESTDIR)$(BINDIR)
	@install -m 755 build/arc $(DESTDIR)$(BINDIR)/arc
	@echo "✅ Installed binary to $(BINDIR)/arc"
	
	@echo "📦 Installing query files..."
	@install -d $(DESTDIR)$(QUERYDIR)
	@for lang in runtime/queries/*; do \
		if [ -d "$$lang" ]; then \
			lang_name=$$(basename $$lang); \
			echo "  - Installing $$lang_name queries..."; \
			install -d $(DESTDIR)$(QUERYDIR)/$$lang_name; \
			install -m 644 $$lang/*.scm $(DESTDIR)$(QUERYDIR)/$$lang_name/ 2>/dev/null || true; \
		fi \
	done
	@echo "✅ Installed query files to $(QUERYDIR)"
	
	@echo "📦 Installing language registry..."
	@install -d $(DESTDIR)$(DATADIR)
	@install -m 644 runtime/languages.yaml $(DESTDIR)$(DATADIR)/languages.yaml
	@echo "✅ Installation complete!"
	@echo ""
	@echo "Arc Editor installed to: $(BINDIR)/arc"
	@echo "Query files installed to: $(QUERYDIR)"
	@echo ""
	@echo "Note: Query files are embedded in the binary as fallback."
	@echo "You can customize queries by placing them in:"
	@echo "  ~/.config/arceditor/queries/LANGUAGE/TYPE.scm"

# Uninstall
uninstall:
	@echo "🗑️  Uninstalling Arc Editor..."
	@rm -f $(DESTDIR)$(BINDIR)/arc
	@rm -rf $(DESTDIR)$(DATADIR)
	@echo "✅ Uninstalled"

# Install to user directory (no sudo needed)
install-user: build/arc
	@echo "📦 Installing Arc Editor (user mode)..."
	@install -d $(HOME)/.local/bin
	@install -m 755 build/arc $(HOME)/.local/bin/arc
	@echo "✅ Installed binary to ~/.local/bin/arc"
	
	@echo "📦 Installing query files..."
	@install -d $(HOME)/.local/share/arc/queries
	@for lang in runtime/queries/*; do \
		if [ -d "$$lang" ]; then \
			lang_name=$$(basename $$lang); \
			echo "  - Installing $$lang_name queries..."; \
			install -d $(HOME)/.local/share/arc/queries/$$lang_name; \
			install -m 644 $$lang/*.scm $(HOME)/.local/share/arc/queries/$$lang_name/ 2>/dev/null || true; \
		fi \
	done
	@echo "✅ Installation complete!"
	@echo ""
	@echo "⚠️  Make sure ~/.local/bin is in your PATH"
	@echo "   Add this to your ~/.bashrc or ~/.zshrc:"
	@echo "   export PATH=\"\$$HOME/.local/bin:\$$PATH\""

# Test installation
test-install:
	@echo "🧪 Testing query resolution..."
	@echo ""
	@echo "Binary location: $$(which arc 2>/dev/null || echo 'NOT IN PATH')"
	@echo ""
	@echo "Checking query search paths:"
	@echo "  1. User config: ~/.config/arceditor/queries/"
	@ls -la ~/.config/arceditor/queries/ 2>/dev/null && echo "    ✅ Found" || echo "    ❌ Not found (OK - this is for custom queries)"
	@echo ""
	@echo "  2. System install: /usr/local/share/arc/queries/"
	@ls -la /usr/local/share/arc/queries/ 2>/dev/null && echo "    ✅ Found" || echo "    ❌ Not found"
	@echo ""
	@echo "  3. User install: ~/.local/share/arc/queries/"
	@ls -la ~/.local/share/arc/queries/ 2>/dev/null && echo "    ✅ Found" || echo "    ❌ Not found"
	@echo ""
	@echo "  4. Embedded: Built into binary"
	@echo "    ✅ Always available"
	@echo ""
	@echo "Test: arc should work from any directory"
	@cd /tmp && arc --version 2>/dev/null && echo "✅ Works!" || echo "⚠️  Check your installation"

.PHONY: install uninstall install-user test-install