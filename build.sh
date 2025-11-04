#!/bin/bash

# TextImageGen - Build Script
# Builds the txt2png and text2png (Cairo-based) executables with appropriate flags

set -e  # Exit on any error

# Create bin directory if it doesn't exist
mkdir -p bin

echo "Building txt2png (ImageMagick-based)..."
# Build the ImageMagick-based executable
g++ -std=gnu++17 -O2 -Wall -Wextra -o bin/txt2png txt2png.cpp

echo "Building text2png (Cairo-based)..."
# Build the Cairo-based executable if libraries are available
if pkg-config --exists cairo && pkg-config --exists fontconfig && pkg-config --exists freetype2; then
    echo "Building Cairo-based renderer..."
    g++ -std=gnu++17 -O2 -Wall -Wextra -o bin/text2png text2png.cpp -I/usr/include/cairo -I/usr/include/libpng16 -I/usr/include/pixman-1 -I/usr/include/freetype2 -lcairo -lfontconfig -lfreetype
    echo "Cairo-based text2png built successfully!"
else
    echo "Cairo dependencies not found, skipping Cairo-based text2png compilation"
    echo "To build Cairo version, install: cairo, fontconfig, and freetype2 development packages"
fi

echo "Build completed successfully!"
echo "Executables are located in: bin/"
ls -la bin/

# Optional: Run a quick test to verify the builds
if [ -f "bin/txt2png" ]; then
    echo "Build verification: bin/txt2png exists"
    ./bin/txt2png --help > /dev/null 2>&1 && echo "txt2png build verification: OK" || echo "txt2png build verification: May have issues with dependencies"
fi

if [ -f "bin/text2png" ]; then
    echo "Build verification: bin/text2png (Cairo) exists"  
    ./bin/text2png --help > /dev/null 2>&1 && echo "text2png (Cairo) build verification: OK" || echo "text2png (Cairo) build verification: May have issues"
fi