# TextImageGen

Converts each line of a text file into a transparent PNG image with customizable styling. Perfect for creating visual elements like lyrics, captions, or annotations for video editing and other applications.

## Features

- Convert each non-empty line of a text file to a separate PNG image
- Support for custom fonts (by name or index)
- Configurable text styling (color, outline, size)
- Transparent background using PNG32 format
- Command preview with dry-run option

## Requirements

- Linux/Unix shell
- ImageMagick CLI in PATH (either `magick` or legacy `convert`)
- Fontconfig recommended (for font discovery via `fc-list`)

## Quick Start

```bash
# Build the project
./build.sh

# Convert lines in text file to images
./bin/txt2png --input lines.txt --prefix output- --font "DejaVu Sans" --size 64

# List available fonts
./bin/txt2png --list-fonts
```

## Build Options

- `./build.sh` - Build with standard linking
- `./build_static.sh` - Build static executable
- `./package_release.sh` - Create release package

## Examples

```bash
# Basic usage - DejaVu Sans, white text with black outline
./bin/txt2png --input lyrics.txt --prefix lyrics- --font "DejaVu Sans" \
            --size 64 --color "#FFFFFF" --outline-color "#000000" --outline 6

# Use font by index (after listing available fonts)
./bin/txt2png --list-fonts | less
./bin/txt2png --input lines.txt --font-index 7 --size 48 --prefix line-

# Preview commands without executing them
./bin/txt2png --input lines.txt --dry-run --prefix out- --font "Liberation Sans"
```
