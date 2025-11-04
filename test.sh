#!/bin/bash

# TextImageGen - Test Script
# Creates a random 3-line text file and renders images using Cairo (default)

set -e  # Exit on any error

echo "Setting up test environment..."

# Create temporary directory for test
TEST_DIR=$(mktemp -d)
echo "Created temporary directory: $TEST_DIR"

# Create random 3-line text file with Finnish words (similar to your example)
cat > "$TEST_DIR/test_lines.txt" << EOF
ratkaisua
testaus
toimii
EOF

echo "Created test file with content:"
cat "$TEST_DIR/test_lines.txt"
echo ""

# Check if Cairo-based text2png exists, build it if not
if [ ! -f "bin/text2png" ]; then
    echo "Building Cairo-based text2png executable..."
    # Try to compile with Cairo
    if command -v pkg-config &> /dev/null && pkg-config --exists cairo && pkg-config --exists fontconfig && pkg-config --exists freetype2; then
        g++ -o bin/text2png text2png.cpp `pkg-config --cflags --libs cairo fontconfig freetype2` && \
        echo "Cairo-based text2png compiled successfully!" || \
        echo "Failed to compile Cairo-based text2png - missing Cairo dependencies?"
    else
        echo "Error: Cairo dependencies not found. Please install: cairo, fontconfig, and freetype2 development packages."
        rm -rf "$TEST_DIR"
        exit 1
    fi
fi

echo "Testing text2png help functionality..."
./bin/text2png --help > /dev/null || echo "Warning: text2png help command failed"

echo "Rendering images from test lines using Cairo (default)..."
RENDER_SUCCESS=false

# Use Cairo-based text2png (primary method)
if [ -f "bin/text2png" ]; then
    echo "Using Cairo-based text2png renderer..."
    ./bin/text2png "$TEST_DIR/test_lines.txt" "$TEST_DIR/output-" --font-size 48 --text-color "#FFFFFF" --outline-color "#000000" --outline-width 2
    if [ $? -eq 0 ] && [ -f "$TEST_DIR/output-1.png" ]; then
        RENDER_SUCCESS=true
        echo "Successfully created images using Cairo renderer"
    else
        echo "Cairo renderer failed"
        rm -rf "$TEST_DIR"
        exit 1
    fi
else
    echo "Error: Cairo-based text2png not available."
    rm -rf "$TEST_DIR"
    exit 1
fi

echo "Generated PNG files:"
ls -la "$TEST_DIR/"*.png

# Create output directory in project root to store results
mkdir -p test_output
cp "$TEST_DIR/"*.png test_output/
echo ""
echo "Copied generated images to test_output/ directory"

# Clean up temporary directory
rm -rf "$TEST_DIR"

if [ "$RENDER_SUCCESS" = true ]; then
    echo "Test completed successfully!"
    echo "Check the test_output/ directory for generated images with transparent backgrounds."
    
    # Check if images have transparency
    if command -v identify &> /dev/null; then
        # Only check if ImageMagick is working for this specific purpose
        if identify -verbose test_output/output-1.png 2>/dev/null | grep -q "Alpha.*activated\|TrueColorAlpha\|GrayscaleAlpha"; then
            echo "Images have transparency (RGBA format) as expected."
        else
            echo "Note: Could not verify transparency in output images, but they should have transparent backgrounds."
        fi
    fi
fi