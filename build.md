# Building Arc Editor

Arc is a terminal-based text editor with syntax highlighting and live configuration reloading.

## Prerequisites

### All Platforms

- CMake 3.16 or higher
- C++20 compatible compiler
- Git (for cloning submodules/dependencies)

### Platform-Specific Requirements

#### Linux/Ubuntu

```bash
sudo apt update
sudo apt install build-essential cmake libncurses5-dev libncursesw5-dev libyaml-cpp-dev
```

#### macOS

```bash
brew install cmake ncurses yaml-cpp
```

#### Windows

- Visual Studio 2019 or later (with C++ workload)
- [vcpkg](https://github.com/microsoft/vcpkg) package manager

```powershell
# Install vcpkg if not already installed
git clone https://github.com/microsoft/vcpkg.git C:\tools\vcpkg
cd C:\tools\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Install dependencies (static libraries recommended)
.\vcpkg install pdcurses:x64-windows-static
.\vcpkg install yaml-cpp:x64-windows-static
```

## Dependencies Structure

The project expects dependencies in the following structure:

```
arc/
├── deps/
│   ├── tree-sitter-core/       # Tree-sitter parser library
│   ├── tree-sitter-python/     # Python language parser
│   ├── tree-sitter-c/          # C language parser
│   ├── tree-sitter-cpp/        # C++ language parser
│   ├── tree-sitter-rust/       # Rust language parser
│   ├── tree-sitter-javascript/ # JavaScript language parser
│   ├── tree-sitter-typescript/ # TypeScript parsers
│   ├── tree-sitter-markdown/   # Markdown parser
│   └── efsw/                   # File system watcher library
├── src/
├── CMakeLists.txt
└── ...
```

### Cloning Dependencies

If you have submodules set up:

```bash
git clone --recursive https://github.com/moisnx/arc.git
cd arc
```

Or if you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

## Building

### Linux/macOS

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)  # Linux
make -j$(sysctl -n hw.ncpu)  # macOS

# Run
./arc
```

### Windows (MSVC)

```powershell
# Create build directory
mkdir build
cd build

# Configure with vcpkg toolchain
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static

# Build
cmake --build . --config Release

# Run
.\Release\arc.exe
```

### Windows (MinGW)

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j4
./arc.exe
```

## Build Options

### CMake Options

```bash
# Disable Tree-sitter (fallback to basic syntax highlighting)
cmake .. -DTREE_SITTER_ENABLED=OFF

# Debug build with symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Specify custom dependency location
cmake .. -DDEPS_DIR=/path/to/deps
```

### Tree-sitter Language Support

The build system automatically discovers available Tree-sitter language parsers in the `deps/` directory. Supported languages:

- Python
- C/C++
- Rust
- JavaScript/TypeScript/TSX
- Markdown

If a parser is not found, it will be skipped and the build will continue.

## Build Targets

```bash
# Build everything
cmake --build .

# Build specific target
cmake --build . --target arc

# Clean build
cmake --build . --target clean

# Rebuild from scratch
rm -rf build && mkdir build && cd build && cmake .. && cmake --build .
```

## Troubleshooting

### Linux: ncurses not found

```bash
sudo apt install libncurses5-dev libncursesw5-dev
```

### macOS: yaml-cpp not found

```bash
brew install yaml-cpp
```

### Windows: Missing DLLs at runtime

If using dynamic linking, copy required DLLs to the executable directory:

```powershell
# PDCurses
copy C:\tools\vcpkg\installed\x64-windows\bin\pdcurses.dll .\Release\

# yaml-cpp
copy C:\tools\vcpkg\installed\x64-windows\bin\yaml-cpp.dll .\Release\
```

**Better solution**: Use static libraries (`:x64-windows-static` triplet) to avoid DLL dependencies.

### Windows: Runtime library mismatch errors

Ensure all dependencies use the same runtime library. The project uses static runtime (`/MT`) by default.

### Tree-sitter parsers not detected (Windows)

Check that parser paths don't contain special characters. The build system should automatically handle Windows paths correctly.

### Cursor positioning issues (Windows)

Known issue with PDCurses. This is a rendering difference between PDCurses and ncurses and will be addressed in a future update.

## Installation

```bash
# Linux/macOS
sudo cmake --install . --prefix /usr/local

# Or copy manually
sudo cp arc /usr/local/bin/

# Windows - copy to a directory in your PATH
copy Release\arc.exe C:\Windows\
```

## Development Build

For faster iteration during development:

```bash
# Debug build with minimal optimization
cmake .. -DCMAKE_BUILD_TYPE=Debug

# With verbose output
cmake --build . --verbose

# Run directly from build directory
./arc  # or .\Debug\arc.exe on Windows
```

## Cross-Compilation

### Linux → Windows (MinGW)

```bash
sudo apt install mingw-w64
mkdir build-windows && cd build-windows
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-w64-x86_64.cmake
make
```

## Performance Considerations

- **Release builds** are significantly faster than Debug builds
- **Static linking** produces larger binaries but has no runtime dependencies
- **Tree-sitter** adds ~5-10MB to binary size but provides superior syntax highlighting

## CI/CD Integration

Example GitHub Actions workflow:

```yaml
name: Build

on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v3
        with:
          submodules: recursive

      - name: Install dependencies (Ubuntu)
        if: matrix.os == 'ubuntu-latest'
        run: sudo apt install libncurses-dev libyaml-cpp-dev

      - name: Install dependencies (macOS)
        if: matrix.os == 'macos-latest'
        run: brew install ncurses yaml-cpp

      - name: Setup vcpkg (Windows)
        if: matrix.os == 'windows-latest'
        uses: lukka/run-vcpkg@v11

      - name: Build
        run: |
          mkdir build && cd build
          cmake .. -DCMAKE_BUILD_TYPE=Release
          cmake --build . --config Release

      - name: Test
        run: cd build && ctest
```

## Getting Help

- Check [GitHub Issues](https://github.com/moisnx/arc/issues) for known problems
- Read the [main README](README.md) for usage instructions
- Join discussions in [GitHub Discussions](https://github.com/moisnx/arc/discussions)

## License

See [LICENSE](LICENSE) file for details.
