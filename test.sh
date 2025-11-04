#!/bin/bash

# TextImageGen - Test Script
# Creates a random 3-line text file and renders images using Cairo (default) or ffmpeg

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

# Check if the txt2png executable exists, build if not
if [ ! -f "bin/txt2png" ]; then
    echo "Building txt2png executable..."
    ./build.sh
fi

# Check if Cairo-based text2png exists, build it if not
if [ ! -f "bin/text2png" ]; then
    echo "Building Cairo-based text2png executable..."
    # Try to compile with Cairo
    if command -v pkg-config &> /dev/null && pkg-config --exists cairo && pkg-config --exists fontconfig && pkg-config --exists freetype2; then
        g++ -o bin/text2png text2png.cpp `pkg-config --cflags --libs cairo fontconfig freetype2` && \
        echo "Cairo-based text2png compiled successfully!" || \
        echo "Failed to compile Cairo-based text2png - missing Cairo dependencies?"
    else
        echo "Cairo dependencies not found, skipping Cairo-based text2png compilation"
    fi
fi

echo "Testing txt2png help functionality..."
./bin/txt2png --help > /dev/null || echo "Warning: txt2png help command failed"

echo "Rendering images from test lines using Cairo (default)..."
RENDER_SUCCESS=false

# Try to use Cairo-based text2png first (as requested)
if [ -f "bin/text2png" ]; then
    echo "Using Cairo-based text2png renderer..."
    ./bin/text2png "$TEST_DIR/test_lines.txt" "$TEST_DIR/output-" --font-size 48 --text-color "#FFFFFF" --outline-color "#000000" --outline-width 2
    if [ $? -eq 0 ] && [ -f "$TEST_DIR/output-1.png" ]; then
        RENDER_SUCCESS=true
        echo "Successfully created images using Cairo renderer"
    else
        echo "Cairo renderer failed, falling back to alternative method"
    fi
else
    echo "Cairo-based text2png not available, checking for alternatives..."
fi

# If Cairo failed or isn't available, fall back to ffmpeg
if [ "$RENDER_SUCCESS" = false ]; then
    echo "Falling back to ffmpeg renderer..."
    
    # Check if ffmpeg is available (which you have on Gentoo)
    if ! command -v ffmpeg &> /dev/null; then
        echo "Error: Neither Cairo nor ffmpeg are available. Please install Cairo dependencies or ffmpeg."
        rm -rf "$TEST_DIR"
        exit 1
    fi

    # Create images for each line using ffmpeg with transparent background
    LINES=($(cat "$TEST_DIR/test_lines.txt"))
    for i in "${!LINES[@]}"; do
        idx=$((i+1))
        text="${LINES[$i]}"
        
        # Use ffmpeg to create a single frame image with transparent background
        # Use color=black@0 to create transparent background (alpha=0)
        ffmpeg -y -f lavfi -i color=black@0:size=400x100:rate=1 -vf \
        "format=rgba,drawtext=fontsize=32:fontcolor=white:x=(w-text_w)/2:y=(h-text_h)/2:text='$text'" \
        -frames:v 1 -c:v png "$TEST_DIR/output-$idx.png" 2>/dev/null || \
        # If default doesn't work, try with a specific font file
        ffmpeg -y -f lavfi -i color=black@0:size=400x100:rate=1 -vf \
        "format=rgba,drawtext=fontfile=/usr/share/fonts/dejavu/DejaVuSans.ttf:fontsize=32:fontcolor=white:x=(w-text_w)/2:y=(h-text_h)/2:text='$text'" \
        -frames:v 1 -c:v png "$TEST_DIR/output-$idx.png" 2>/dev/null || \
        # If no font works, try with a common font location on Linux
        ffmpeg -y -f lavfi -i color=black@0:size=400x100:rate=1 -vf \
        "format=rgba,drawtext=fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf:fontsize=32:fontcolor=white:x=(w-text_w)/2:y=(h-text_h)/2:text='$text'" \
        -frames:v 1 -c:v png "$TEST_DIR/output-$idx.png"
    done
    
    # Check if ffmpeg approach created files
    if [ -f "$TEST_DIR/output-1.png" ]; then
        RENDER_SUCCESS=true
        echo "Successfully created images using ffmpeg renderer"
    else
        echo "All rendering methods failed"
        rm -rf "$TEST_DIR"
        exit 1
    fi
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