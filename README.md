<!-- Banner / Logo -->
<p align="center">
  <img src="https://raw.githubusercontent.com/moisnx/arc/main/.github/assets/logo.png" alt="Arc Editor Logo" width="120" height="120"/>
</p>

<h1 align="center">Arc Editor</h1>
<p align="center"><b>Simple. Modern. Efficient.</b></p>
<p align="center">
  <em>A modern terminal-based text editor with Tree-sitter powered syntax highlighting.</em>
</p>

<p align="center">
  <a href="https://github.com/moisnx/arc/actions"><img src="https://img.shields.io/github/actions/workflow/status/moisnx/arc/ci.yml?branch=main&style=flat-square" alt="Build Status"></a>
  <a href="https://github.com/moisnx/arc/blob/main/LICENSE"><img src="https://img.shields.io/github/license/moisnx/arc?style=flat-square" alt="License"></a>
  <img src="https://img.shields.io/badge/platform-linux%20%7C%20macos%20%7C%20windows-blue?style=flat-square" alt="Platforms">
  <img src="https://img.shields.io/badge/editor%20type-terminal-informational?style=flat-square">
</p>

---

> **Arc** combines the accessibility of nano with powerful features and a clean interface.

---

## ✨ Features

- **Intuitive Interface**: Minimal, focused on your content with smart viewport management
- **Advanced Syntax Highlighting**: Tree-sitter powered, accurate AST-based coloring
- **Modern Editing**: Gap buffer for efficient text ops, unlimited undo/redo
- **Smart Selection**: Visual selection with copy, cut, paste
- **Live Configuration**: Hot-reload themes and settings
- **Cross-Platform**: Windows (PDCurses), Linux, macOS (NCurses)
- **Lightweight**: Fast startup, minimal resource usage
- **Customizable**: Multiple color themes, configurable behavior

---

## 🖥️ Demo

<p align="center">
  <img src="https://raw.githubusercontent.com/moisnx/arc/main/.github/assets/screenshot.gif" alt="Arc Editor Demo" width="700"/>
</p>

---

## 🚀 Getting Started

### Prerequisites

- **All Platforms:** CMake 3.16+, C++20 compiler, Git (for submodules)
- **Linux:** `build-essential cmake libncurses5-dev libyaml-cpp-dev`
- **macOS:** `cmake ncurses yaml-cpp` (via Homebrew)
- **Windows:** Visual Studio 2019+ with vcpkg

### Installation

<details>
  <summary><b>Linux/Ubuntu</b></summary>

```bash
sudo apt install build-essential cmake libncurses5-dev libyaml-cpp-dev
git clone --recursive https://github.com/moisnx/arc.git
cd arc
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./arc
```
</details>

<details>
  <summary><b>macOS</b></summary>

```bash
brew install cmake ncurses yaml-cpp
git clone --recursive https://github.com/moisnx/arc.git
cd arc
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
./arc
```
</details>

<details>
  <summary><b>Windows</b></summary>

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\tools\vcpkg
cd C:\tools\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
.\vcpkg install pdcurses:x64-windows-static yaml-cpp:x64-windows-static

git clone --recursive https://github.com/moisnx/arc.git
cd arc
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake `
         -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build . --config Release
.\Release\arc.exe
```
</details>

> **Note:** Forgot `--recursive`? Run `git submodule update --init --recursive`

---

## 📂 Supported Languages

- **C/C++**
- **Python**
- **Rust**
- **JavaScript/TypeScript** (incl. JSX/TSX)
- **Markdown** (with inline code)
- **Go**
- **Zig**

---

## 🎮 Usage

```bash
# Open a file
arc filename.txt

# Start with empty buffer
arc

# View help
arc --help
```

### Key Bindings

<details>
  <summary><b>Show Key Bindings</b></summary>

#### File Operations
| Key      | Action                              |
|----------|-------------------------------------|
| Ctrl+S   | Save file                           |
| Ctrl+Q   | Quit (with unsaved changes prompt)  |

#### Editing
| Key        | Action                        |
|------------|------------------------------|
| Ctrl+Z     | Undo                         |
| Ctrl+Y     | Redo                         |
| Backspace  | Delete char before cursor    |
| Delete     | Delete char at cursor        |
| Enter      | Insert new line              |
| Tab        | Insert 4 spaces              |

#### Navigation
| Key         | Action                 |
|-------------|-----------------------|
| Arrow Keys  | Move cursor           |
| Home/End    | Line start/end        |
| PgUp/PgDn   | Scroll viewport       |

#### Selection
| Key           | Action              |
|---------------|---------------------|
| Shift+Arrows  | Extend selection    |
| Ctrl+A        | Select all          |
| Ctrl+C        | Copy selection      |
| Ctrl+X        | Cut selection       |
| Ctrl+V        | Paste from clipboard|
| Esc           | Clear selection     |

</details>

---

## ⚙️ Configuration

Arc auto-creates config files on first run:

- Linux/macOS: `~/.config/arceditor/`
- Windows: `%APPDATA%\arceditor\`

#### Example: `config.yaml`

```yaml
appearance:
  theme: default

editor:
  tab_size: 4
  line_numbers: true
  cursor_style: auto

syntax:
  highlighting: viewport
```

#### Themes

- 14 built-in themes.
- Add custom themes in `.config/arceditor/themes/`.
- Supports hex, 256-color, and named colors.
- **Hot-reload:** Changes are auto-reloaded.

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

---

## 🛠️ Development

<details>
  <summary><b>Project Structure</b></summary>

```
arc/
├── src/
│   ├── core/
│   ├── features/
│   ├── ui/
│   └── main.cpp
├── deps/                  # Git submodules: tree-sitter, efsw, etc.
├── treesitter/
│   ├── languages.yaml
│   └── queries/
├── .config/arceditor/
└── CMakeLists.txt
```
</details>

See [build.md](build.md) for advanced setup and troubleshooting.

---

## 📈 Roadmap

- [ ] Multiple file tabs/buffers
- [ ] Advanced search/replace (regex)
- [ ] Git integration (diff, blame)
- [ ] LSP support
- [ ] Plugin system
- [ ] Mouse support enhancements
- [ ] Split panes
- [ ] Configurable keybindings

---

## 👥 Community & Contributing

💡 **Contributions welcome!**  
1. Fork & create a feature branch
2. Follow [.gitmessage](.gitmessage) for commits
3. Run tests before submitting
4. Open a PR

- **Issues:** [GitHub Issues](https://github.com/moisnx/arc/issues)
- **Discussions:** [GitHub Discussions](https://github.com/moisnx/arc/discussions)
- **Docs:** [build.md](build.md) | [quickstart.md](quickstart.md)

---

## 📝 License

MIT License – see [LICENSE](LICENSE) for details.

---

## 🙏 Acknowledgments

- [Tree-sitter](https://tree-sitter.github.io/)
- [NCurses](https://invisible-island.net/ncurses/)
- [PDCurses](https://github.com/Bill-Gray/PDCursesMod)
- [EFSW](https://github.com/SpartanJ/efsw)
- [yaml-cpp](https://github.com/jbeder/yaml-cpp)
- Inspired by nano, micro, and helix editors

---

<p align="center"><b>Arc Editor</b> – Simple. Modern. Efficient.</p>
