#!/bin/bash

# TextImageGen - Package Release Script
# Packages the built executable and documentation for distribution

set -e  # Exit on any error

VERSION="v1.0"
BUILD_DIR="build"
RELEASE_DIR="release"
BIN_DIR="bin"

# Ensure build exists
if [ ! -f "$BIN_DIR/txt2png" ]; then
    echo "Error: bin/txt2png not found. Please run build.sh first."
    exit 1
fi

# Create release directory if it doesn't exist
mkdir -p $RELEASE_DIR

# Create a zip file containing the executable and documentation
RELEASE_NAME="txt2png-${VERSION}-linux64.zip"
cd $BIN_DIR
zip "../$RELEASE_DIR/$RELEASE_NAME" txt2png
cd ..

# Also copy documentation to release
cp README.md USAGE.txt LICENSE "$RELEASE_DIR/"

echo "Release package created: $RELEASE_DIR/$RELEASE_NAME"
echo "Documentation and license included in release directory."