#!/bin/bash

# Clean PATH - remove ALL Windows paths
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/usr/lib/wsl/lib"

# Clean environment variables
unset INCLUDE
unset LIB  
unset LIBPATH
unset CPATH
unset C_INCLUDE_PATH
unset CPLUS_INCLUDE_PATH
unset LIBRARY_PATH

# Verify we're using Linux tools
echo "Using compiler: $(which gcc)"
echo "Compiler version: $(gcc --version | head -n1)"

# Clean build
rm -rf build CMakeCache.txt CMakeFiles/ cmake_install.cmake Makefile generated/

# Create fresh build
mkdir -p build
cd build

# Configure
cmake \
    -DCMAKE_C_COMPILER=/usr/bin/gcc \
    -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
    -DCMAKE_BUILD_TYPE=Release \
    ..

# Build
make -j$(nproc)
