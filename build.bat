@echo off
if not exist build mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build
echo.
echo ✓ Build complete! Running Arc...
echo.
.\build\Debug\arc.exe