#!/bin/bash

# TextImageGen - Static Build Script
# Builds a statically linked version of the executable

set -e  # Exit on any error

# Create bin directory if it doesn't exist
mkdir -p bin

echo "Building static txt2png..."

# Build the executable with static linking
g++ -std=gnu++17 -O2 -Wall -Wextra -static -o bin/txt2png-static txt2png.cpp

echo "Static build completed successfully!"
echo "Static executable is located at: bin/txt2png-static"

# Verify the executable is static
if ldd bin/txt2png-static 2>&1 | grep -q "not a dynamic executable\|statically linked"; then
    echo "Verification: Executable is statically linked"
else
    echo "Warning: Executable may not be fully static"
fi