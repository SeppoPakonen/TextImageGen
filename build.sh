#!/bin/bash

# TextImageGen - Build Script
# Builds the txt2png executable with appropriate flags

set -e  # Exit on any error

# Create bin directory if it doesn't exist
mkdir -p bin

echo "Building txt2png..."

# Build the executable
g++ -std=gnu++17 -O2 -Wall -Wextra -o bin/txt2png txt2png.cpp

echo "Build completed successfully!"
echo "Executable is located at: bin/txt2png"

# Optional: Run a quick test to verify the build
if [ -f "bin/txt2png" ]; then
    echo "Build verification: bin/txt2png exists"
    # Display help as a basic verification
    ./bin/txt2png --help > /dev/null 2>&1 && echo "Build verification: OK" || echo "Build verification: May have issues with dependencies"
else
    echo "Error: Build failed - bin/txt2png not found"
    exit 1
fi