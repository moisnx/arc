# Quick Build Guide

**TL;DR:** Get up and running fast.

## Linux

```bash
# Install deps
sudo apt install build-essential cmake libncurses5-dev libyaml-cpp-dev

# Build
git clone --recursive https://github.com/moisnx/arc.git
cd arc
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./arc
```

## macOS

```bash
# Install deps
brew install cmake ncurses yaml-cpp

# Build
git clone --recursive https://github.com/moisnx/arc.git
cd arc
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
./arc
```

## Windows

```powershell
# Install vcpkg (one-time setup)
git clone https://github.com/microsoft/vcpkg.git C:\tools\vcpkg
cd C:\tools\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Install deps
.\vcpkg install pdcurses:x64-windows-static yaml-cpp:x64-windows-static

# Build
git clone --recursive https://github.com/moisnx/arc.git
cd arc
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build . --config Release
.\Release\arc.exe
```

## Troubleshooting

**Missing dependencies?** See [BUILD.md](BUILD.md) for detailed instructions.

**Build errors?** Try cleaning: `rm -rf build && mkdir build && cd build && cmake ..`

**Runtime issues?** Make sure you're running the Release build, not Debug.
