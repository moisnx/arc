#!/bin/bash
# Download STB image libraries for image rendering

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DEPS_DIR="$PROJECT_ROOT/deps"
STB_DIR="$DEPS_DIR/stb"

echo "========================================="
echo "Downloading STB Image Libraries"
echo "========================================="
echo ""

# Create deps/stb directory
mkdir -p "$STB_DIR"

# Base URL for STB headers
STB_BASE_URL="https://raw.githubusercontent.com/nothings/stb/master"

# Files to download
declare -a STB_FILES=(
    "stb_image.h"
    "stb_image_resize2.h"
    "stb_image_write.h"
)

# Download each file
for file in "${STB_FILES[@]}"; do
    echo "Downloading $file..."
    if command -v curl &> /dev/null; then
        curl -L -o "$STB_DIR/$file" "$STB_BASE_URL/$file"
    elif command -v wget &> /dev/null; then
        wget -O "$STB_DIR/$file" "$STB_BASE_URL/$file"
    else
        echo "Error: Neither curl nor wget found. Please install one of them."
        exit 1
    fi
    
    if [ -f "$STB_DIR/$file" ]; then
        echo "✓ Successfully downloaded $file"
    else
        echo "✗ Failed to download $file"
        exit 1
    fi
done

echo ""
echo "========================================="
echo "✓ All STB libraries downloaded!"
echo "Location: $STB_DIR"
echo "========================================="
echo ""
echo "Files downloaded:"
ls -lh "$STB_DIR"