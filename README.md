# Arc

A modern terminal-based text editor with Tree-sitter powered syntax highlighting, designed for simplicity and efficiency. Arc combines the accessibility of nano with powerful features and a clean interface.

## ✨ Features

- **Intuitive Interface**: Clean, minimal design focused on your content with smart viewport management
- **Advanced Syntax Highlighting**: Tree-sitter powered highlighting for accurate, AST-based syntax coloring
- **Modern Editing**: Gap buffer implementation for efficient text operations with unlimited undo/redo
- **Smart Selection**: Visual selection mode with copy, cut, and paste operations
- **Live Configuration**: Hot-reload themes and settings without restarting
- **Cross-Platform**: Runs on Windows (PDCurses), Linux, and macOS (NCurses)
- **Lightweight**: Fast startup and minimal resource usage
- **Customizable**: Multiple color themes and configurable editor behavior

### Supported Languages

Built-in Tree-sitter support for:
- **C/C++** - Full syntax highlighting with semantic tokens
- **Python** - Complete Python 3 support
- **Rust** - Modern Rust syntax
- **JavaScript/TypeScript** - Including JSX/TSX variants
- **Markdown** - With inline code highlighting
- **Go** - Standard Go syntax
- **Zig** - Native Zig support

## 🚀 Quick Start

### Prerequisites

**All Platforms:**
- CMake 3.16+
- C++20 compatible compiler
- Git (for submodules)

**Platform-Specific:**

| Platform | Requirements |
|----------|-------------|
| **Linux** | `build-essential cmake libncurses5-dev libyaml-cpp-dev` |
| **macOS** | `cmake ncurses yaml-cpp` (via Homebrew) |
| **Windows** | Visual Studio 2019+ with vcpkg |

### Installation

#### Linux/Ubuntu

```bash
# Install dependencies
sudo apt install build-essential cmake libncurses5-dev libyaml-cpp-dev

# Clone with submodules
git clone --recursive https://github.com/moisnx/arc.git
cd arc

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run
./arc
```

#### macOS

```bash
# Install dependencies
brew install cmake ncurses yaml-cpp

# Clone and build
git clone --recursive https://github.com/moisnx/arc.git
cd arc
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
./arc
```

#### Windows

```powershell
# Install vcpkg (one-time setup)
git clone https://github.com/microsoft/vcpkg.git C:\tools\vcpkg
cd C:\tools\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Install dependencies (static linking recommended)
.\vcpkg install pdcurses:x64-windows-static yaml-cpp:x64-windows-static

# Clone and build
git clone --recursive https://github.com/moisnx/arc.git
cd arc
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake `
         -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build . --config Release

# Run
.\Release\arc.exe
```

> **Note:** Forgot `--recursive`? Run `git submodule update --init --recursive`

## 🎯 Usage

### Basic Commands

```bash
# Open a file
arc filename.txt

# Start with empty buffer
arc

# View help
arc --help
```

### Key Bindings

#### File Operations
| Key | Action |
|-----|--------|
| `Ctrl+S` | Save file |
| `Ctrl+Q` | Quit (with unsaved changes prompt) |

#### Editing
| Key | Action |
|-----|--------|
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Backspace` | Delete character before cursor |
| `Delete` | Delete character at cursor |
| `Enter` | Insert new line |
| `Tab` | Insert 4 spaces |

#### Navigation
| Key | Action |
|-----|--------|
| `Arrow Keys` | Move cursor |
| `Home` | Move to line start |
| `End` | Move to line end |
| `Page Up/Down` | Scroll viewport |

#### Selection
| Key | Action |
|-----|--------|
| `Shift+Arrows` | Extend selection |
| `Ctrl+A` | Select all |
| `Ctrl+C` | Copy selection |
| `Ctrl+X` | Cut selection |
| `Ctrl+V` | Paste from clipboard |
| `Esc` | Clear selection |

### Configuration

Arc automatically creates configuration files on first run:

**Linux/macOS:** `~/.config/arceditor/`  
**Windows:** `%APPDATA%\arceditor\`

#### Configuration Files

```
~/.config/arceditor/
├── config.yaml          # Editor settings
├── keybinds.conf        # Custom keybindings (future)
└── themes/
    ├── default.theme
    ├── gruvbox_dark.theme
    ├── monokai.theme
    └── ...
```

#### config.yaml

```yaml
appearance:
  theme: default           # Theme name (without .theme extension)

editor:
  tab_size: 4             # Spaces per tab
  line_numbers: true      # Show line numbers
  cursor_style: auto      # auto, block, bar, underline

syntax:
  highlighting: viewport  # none, viewport, full
```

**Syntax Modes:**
- `none` - Disable highlighting (fastest)
- `viewport` - Highlight visible lines only (recommended, default)
- `full` - Highlight entire file (slower for large files)

#### Themes

Arc includes 14 built-in themes. Create custom themes in `.config/arceditor/themes/`:

```yaml
# mytheme.theme
name: "My Theme"
background: "#1e1e2e"
foreground: "#cdd6f4"
keyword: "#cba6f7"
string_literal: "#a6e3a1"
comment: "#6c7086"
# ... see existing themes for all options
```

Themes support:
- Hex colors (`#rrggbb`)
- 256-color codes (`color_123`)
- Standard terminal colors (`red`, `blue`, etc.)

**Hot-reload:** Changes to themes and `config.yaml` are detected automatically while Arc is running.

## 🛠️ Development

### Project Structure

```
arc/
├── src/
│   ├── core/                  # Core editor logic
│   │   ├── editor.{cpp,h}    # Main editor class
│   │   ├── buffer.{cpp,h}    # Gap buffer implementation
│   │   └── config_manager.{cpp,h}  # Configuration system
│   ├── features/              # Advanced features
│   │   ├── syntax_highlighter.{cpp,h}  # Tree-sitter integration
│   │   └── syntax_config_loader.{cpp,h} # Language config
│   ├── ui/                    # User interface
│   │   ├── renderer.{cpp,h}  # Display logic
│   │   ├── input_handler.{cpp,h}  # Key handling
│   │   └── style_manager.{cpp,h}  # Theme system
│   └── main.cpp
├── deps/                      # Git submodules
│   ├── tree-sitter-core/
│   ├── tree-sitter-python/
│   ├── tree-sitter-c/
│   ├── tree-sitter-cpp/
│   ├── tree-sitter-rust/
│   ├── tree-sitter-javascript/
│   ├── tree-sitter-typescript/
│   ├── tree-sitter-markdown/
│   ├── tree-sitter-go/
│   ├── tree-sitter-zig/
│   └── efsw/                  # File watcher
├── treesitter/
│   ├── languages.yaml         # Language registry
│   └── queries/               # Syntax query files
├── .config/arceditor/         # Default configs
└── CMakeLists.txt
```

### Building from Source

See [build.md](build.md) for detailed instructions including:
- Platform-specific setup
- Dependency installation
- CMake configuration options
- Troubleshooting common issues

Quick development build:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

### Adding New Languages

1. Add Tree-sitter grammar as submodule in `deps/`
2. Update `treesitter/languages.yaml`:

```yaml
languages:
  mylang:
    name: "My Language"
    extensions: ["ml", "mylang"]
    builtin: true
    parser_name: "mylang"
    queries: ["treesitter/queries/mylang/highlights.scm"]
```

3. Add query file with highlight rules
4. Rebuild to link parser

### Contributing

We welcome contributions! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow the commit message format in `.gitmessage`
4. Run tests before submitting
5. Open a Pull Request

See [.gitmessage](.gitmessage) for commit conventions.

## 🎨 Customization

### Creating Themes

Themes are YAML files defining color schemes. All fields support:
- Hex colors: `"#rrggbb"`
- Named colors: `"red"`, `"bright_blue"`, etc.
- 256-color codes: `"color_123"`

```yaml
name: "My Theme"

# UI Elements
background: "#1e1e2e"
foreground: "#cdd6f4"
cursor: "#f5e0dc"
selection: "#45475a"
line_numbers: "#6c7086"
status_bar_bg: "#313244"
status_bar_fg: "#cdd6f4"

# Syntax Categories
keyword: "#cba6f7"
string_literal: "#a6e3a1"
number: "#fab387"
comment: "#6c7086"
function_name: "#89b4fa"
variable: "#cdd6f4"
type: "#f9e2af"
operator: "#94e2d5"

# Markup (Markdown)
markup_heading: "#cba6f7"
markup_bold: "#f9e2af"
markup_italic: "#94e2d5"
markup_code: "#a6e3a1"
```

### Extending Syntax Highlighting

Query files use Tree-sitter's S-expression format:

```scheme
; treesitter/queries/mylang/highlights.scm
(function_definition
  name: (identifier) @function)

(string_literal) @string

(comment) @comment
```

Supported capture names map to theme colors (see `style_manager.h`).

## 🐛 Known Issues

- **Windows:** Minor cursor positioning differences with PDCurses vs NCurses
- **All Platforms:** Very large files (>10MB) may experience slower highlighting with `syntax: full` mode

## 📋 Roadmap

- [ ] Multiple file tabs/buffers
- [ ] Advanced search and replace (regex support)
- [ ] Git integration (diff view, blame annotations)
- [ ] LSP support for code intelligence
- [ ] Plugin system
- [ ] Mouse support enhancements
- [ ] Split panes
- [ ] Configurable keybindings

## 📊 Performance

Typical startup times:
- **Empty file:** <50ms
- **1000 line file:** ~100ms
- **10,000 line file:** ~300ms (viewport mode)

Memory usage: ~5-15MB depending on file size and highlighting mode.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- [Tree-sitter](https://tree-sitter.github.io/) - Incremental parsing library
- [NCurses](https://invisible-island.net/ncurses/) - Terminal UI library
- [PDCurses](https://github.com/Bill-Gray/PDCursesMod) - Cross-platform curses implementation
- [EFSW](https://github.com/SpartanJ/efsw) - File system watcher
- [yaml-cpp](https://github.com/jbeder/yaml-cpp) - YAML parser
- Inspired by nano, micro, and helix editors

## 📞 Support & Feedback

- **Issues:** [GitHub Issues](https://github.com/moisnx/arc/issues)
- **Discussions:** [GitHub Discussions](https://github.com/moisnx/arc/discussions)
- **Documentation:** See [build.md](build.md) and [quickstart.md](quickstart.md)

---

**Arc** - Simple. Modern. Efficient.
