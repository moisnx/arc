# Arc

A modern terminal-based text editor designed for simplicity and efficiency. Arc combines the accessibility of nano with contemporary features and a clean interface.

## ✨ Features

- **Intuitive Interface**: Clean, minimal design focused on your content
- **Modern Editing**: Gap buffer implementation for efficient text operations
- **Syntax Highlighting**: Built-in support for multiple programming languages
- **Cross-Platform**: Runs on Windows, Linux, and macOS
- **Lightweight**: Fast startup and minimal resource usage
- **Customizable**: Configurable keybindings and color themes

## 🚀 Quick Start

### Prerequisites

**Windows:**

- Visual Studio 2019+ or MinGW-w64
- CMake 3.16+
- vcpkg (for PDCurses)

**Linux/macOS:**

- GCC 7+ or Clang 5+
- CMake 3.16+
- NCurses development libraries

### Installation

#### Windows

```bash
# Install PDCurses via vcpkg
vcpkg install pdcurses

# Build Arc
git clone https://github.com/yourusername/arc.git
cd arc
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

#### Linux/Ubuntu

```bash
# Install dependencies
sudo apt install build-essential cmake libncurses5-dev

# Build Arc
git clone https://github.com/yourusername/arc.git
cd arc
mkdir build && cd build
cmake ..
make -j$(nproc)
```

#### macOS

```bash
# Install dependencies
brew install cmake ncurses

# Build Arc
git clone https://github.com/yourusername/arc.git
cd arc
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

## 🎯 Usage

### Basic Commands

```bash
# Open a file
arc filename.txt

# Open Arc without a file
arc

# View help
arc --help
```

### Key Bindings

| Key            | Action     |
| -------------- | ---------- |
| `Ctrl+S`       | Save file  |
| `Ctrl+O`       | Open file  |
| `Ctrl+X`       | Exit       |
| `Ctrl+G`       | Show help  |
| `Ctrl+W`       | Search     |
| `Ctrl+K`       | Cut line   |
| `Ctrl+U`       | Paste      |
| `Ctrl+L`       | Go to line |
| `Arrow Keys`   | Navigate   |
| `Page Up/Down` | Scroll     |

### Configuration

Arc looks for configuration files in:

- **Windows**: `%APPDATA%/arc/`
- **Linux/macOS**: `~/.config/arc/`

#### Example `keybinds.conf`

```ini
# Custom keybindings
save=Ctrl+S
open=Ctrl+O
exit=Ctrl+X
help=F1
```

## 🛠️ Development

### Building from Source

1. **Clone the repository**

   ```bash
   git clone https://github.com/yourusername/arc.git
   cd arc
   ```

2. **Install dependencies** (see platform-specific instructions above)

3. **Build**
   ```bash
   mkdir build && cd build
   cmake ..
   cmake --build .
   ```

### Project Structure

```
arc/
├── src/
│   ├── core/           # Core editor functionality
│   │   ├── editor.cpp
│   │   ├── editor.h
│   │   ├── gap_buffer.cpp
│   │   └── gap_buffer.h
│   ├── features/       # Advanced features
│   │   ├── syntax_highlighter.cpp
│   │   └── syntax_highlighter.h
│   ├── ui/            # User interface components
│   │   ├── colors.cpp
│   │   ├── colors.h
│   │   ├── display.cpp
│   │   └── display.h
│   └── main.cpp       # Application entry point
├── config/            # Default configuration files
├── samples/           # Example files for testing
└── CMakeLists.txt     # Build configuration
```

### Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 🎨 Customization

### Color Themes

Arc supports custom color themes. Create a `theme.conf` file in your config directory:

```ini
# Arc Color Theme
background=0
foreground=7
cursor=15
selection_bg=4
selection_fg=15
line_numbers=8
status_bar_bg=4
status_bar_fg=15
```

### Language Support

Currently supported languages:

- C/C++
- Python
- JavaScript
- Markdown
- Plain text

## 🐛 Known Issues

- **Linux**: Minor cursor positioning inconsistencies during fast scrolling
- **Linux**: Occasional input freezing in insert mode (working on fixes)

## 📋 Roadmap

- [ ] Plugin system
- [ ] Multiple file tabs
- [ ] Advanced search and replace
- [ ] Git integration
- [ ] LSP support
- [ ] Configurable syntax highlighting
- [ ] Mouse support improvements

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Inspired by nano, micro, and other terminal editors
- Built with NCurses/PDCurses for cross-platform compatibility
- Special thanks to the open-source community

## Feedback

- **Issues**: [GitHub Issues](https://github.com/moisnx/arc/issues)
- **Discussions**: [GitHub Discussions](https://github.com/moisnx/arc/discussions)

---

**Arc** - Simple. Modern. Efficient.
