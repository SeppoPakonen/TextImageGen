#!/bin/bash

# Build script for text2png using Cairo
# This script compiles the Cairo-based text to PNG converter

# Check if all required libraries are available
if ! pkg-config --exists cairo || ! pkg-config --exists fontconfig || ! pkg-config --exists freetype2; then
    echo "Error: Required libraries (cairo, fontconfig, freetype2) not found"
    exit 1
fi

# Get the compilation flags
CFLAGS=$(pkg-config --cflags cairo fontconfig freetype2)
LIBS=$(pkg-config --libs cairo fontconfig freetype2)

echo "Compiling with flags: $CFLAGS"
echo "Linking with libs: $LIBS"

# Compile the program
g++ -o bin/text2png text2png.cpp $CFLAGS $LIBS

if [ $? -eq 0 ]; then
    echo "Compilation successful!"
    echo "Cairo-based text2png is now available at: bin/text2png"
else
    echo "Compilation failed"
    exit 1
fi