/*
 * text2png - Convert text to transparent PNG images using Cairo
 * 
 * Compile with: g++ -o text2png text2png.cpp `pkg-config --cflags --libs cairo`
 */

#include <cairo.h>
#include <cairo-ft.h>
#include <fontconfig/fontconfig.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <cmath>

struct TextOptions {
    std::string font_name = "DejaVu Sans";
    int font_size = 48;
    double text_r = 1.0, text_g = 1.0, text_b = 1.0;  // White text
    double outline_r = 0.0, outline_g = 0.0, outline_b = 0.0;  // Black outline
    double bg_r = 0.0, bg_g = 0.0, bg_b = 0.0, bg_a = 0.0;  // Background: transparent by default
    int outline_width = 2;
    int padding = 20;
    std::string output_prefix = "output";
    bool verbose = false;  // Added verbose flag
};

void print_final_config(const TextOptions& opts) {
    std::cout << "\n=== Final Configuration ===" << std::endl;
    std::cout << "Font Name: " << opts.font_name << std::endl;
    std::cout << "Font Size: " << opts.font_size << std::endl;
    std::cout << "Text Color: RGB(" 
              << static_cast<int>(opts.text_r * 255) << "," 
              << static_cast<int>(opts.text_g * 255) << "," 
              << static_cast<int>(opts.text_b * 255) << ")" << std::endl;
    std::cout << "Outline Color: RGB(" 
              << static_cast<int>(opts.outline_r * 255) << "," 
              << static_cast<int>(opts.outline_g * 255) << "," 
              << static_cast<int>(opts.outline_b * 255) << ")" << std::endl;
    std::cout << "Outline Width: " << opts.outline_width << std::endl;
    std::cout << "Background Color: RGBA(" 
              << static_cast<int>(opts.bg_r * 255) << "," 
              << static_cast<int>(opts.bg_g * 255) << "," 
              << static_cast<int>(opts.bg_b * 255) << "," 
              << static_cast<int>(opts.bg_a * 255) << ")" << std::endl;
    std::cout << "Padding: " << opts.padding << std::endl;
    std::cout << "Verbose Mode: " << (opts.verbose ? "ON" : "OFF") << std::endl;
    std::cout << "Output Prefix: " << opts.output_prefix << std::endl;
    std::cout << "==========================\n" << std::endl;
}

void list_fonts() {
    // Initialize FontConfig
    FcConfig* config = FcInitLoadConfigAndFonts();
    if (!config) {
        std::cerr << "Could not initialize FontConfig" << std::endl;
        return;
    }

    // Create a pattern to match all fonts
    FcPattern* pattern = FcPatternCreate();
    FcObjectSet* object_set = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_FILE, (void*)0);
    FcFontSet* font_set = FcFontList(config, pattern, object_set);

    if (font_set) {
        // Use a set to avoid duplicates
        std::set<std::string> unique_fonts;
        
        for (int i = 0; i < font_set->nfont; i++) {
            FcChar8* family = nullptr;
            FcPatternGetString(font_set->fonts[i], FC_FAMILY, 0, &family);
            
            if (family) {
                std::string font_name = (char*)family;
                unique_fonts.insert(font_name);
            }
        }
        
        // Print unique font names with indices
        int index = 1;
        for (const auto& font_name : unique_fonts) {
            std::cout << index << ": " << font_name << std::endl;
            index++;
        }
    }

    // Cleanup
    if (font_set) FcFontSetDestroy(font_set);
    if (object_set) FcObjectSetDestroy(object_set);
    if (pattern) FcPatternDestroy(pattern);
    FcConfigDestroy(config);
}

void render_text_to_png(const std::string& text, const std::string& filename, const TextOptions& opts) {
    // Initialize FreeType and FontConfig
    FT_Library ft_library;
    if (FT_Init_FreeType(&ft_library)) {
        std::cerr << "Could not init FreeType" << std::endl;
        return;
    }

    // Find font file using FontConfig
    FcConfig* config = FcInitLoadConfigAndFonts();
    FcPattern* pattern = FcNameParse((const FcChar8*)opts.font_name.c_str());
    FcConfigSubstitute(config, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    
    FcResult result;
    FcPattern* font = FcFontMatch(config, pattern, &result);
    
    char* font_file = nullptr;
    if (font) {
        FcChar8* file_utf8;
        if (FcPatternGetString(font, FC_FILE, 0, &file_utf8) == FcResultMatch) {
            font_file = (char*)file_utf8;
        }
    }
    
    if (!font_file) {
        std::cerr << "Could not find font: " << opts.font_name << std::endl;
        FcPatternDestroy(pattern);
        FcPatternDestroy(font);
        FcConfigDestroy(config);
        FT_Done_FreeType(ft_library);
        return;
    }
    
    // Create font face
    FT_Face ft_face;
    if (FT_New_Face(ft_library, font_file, 0, &ft_face)) {
        std::cerr << "Could not load font file: " << font_file << std::endl;
        FcPatternDestroy(pattern);
        FcPatternDestroy(font);
        FcConfigDestroy(config);
        FT_Done_FreeType(ft_library);
        return;
    }
    
    // Set font size
    FT_Set_Pixel_Sizes(ft_face, 0, opts.font_size);
    
    // Initialize Cairo with FreeType
    cairo_surface_t* ft_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    cairo_t* ft_cr = cairo_create(ft_surface);
    cairo_font_face_t* cairo_ft_face = cairo_ft_font_face_create_for_ft_face(ft_face, 0);
    cairo_set_font_face(ft_cr, cairo_ft_face);
    cairo_font_options_t* font_opts = cairo_font_options_create();
    cairo_font_options_set_antialias(font_opts, CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_font_options(ft_cr, font_opts);
    cairo_set_font_size(ft_cr, opts.font_size);
    
    // Measure text size
    cairo_text_extents_t extents;
    cairo_text_extents(ft_cr, text.c_str(), &extents);
    
    // Calculate image dimensions properly
    // extents.width and extents.height may not include the full character bounds
    // extents.x_advance and y_advance include the advance width
    // For outline text, account for the outline width too
    double text_width = extents.width;
    double text_height = extents.height;
    
    // Calculate proper bounding box including bearing (offsets from origin)
    double bearing_x = extents.x_bearing;
    double bearing_y = extents.y_bearing;
    
    // Determine the full dimensions needed
    double full_width = std::abs(bearing_x) + text_width + opts.padding * 2 + opts.outline_width * 2;
    double full_height = std::abs(bearing_y) + text_height + opts.padding * 2 + opts.outline_width * 2;
    
    // Ensure minimum dimensions (in case of very short text)
    if (full_width < opts.font_size * 1.5) // Just a reasonable minimum
        full_width = opts.font_size * 1.5;
    if (full_height < opts.font_size)
        full_height = opts.font_size * 1.2;
    
    // Create the actual surface for the image
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                                         static_cast<int>(ceil(full_width)), 
                                                         static_cast<int>(ceil(full_height)));
    cairo_t* cr = cairo_create(surface);
    
    // Draw background if not transparent
    if (opts.bg_a > 0.0) {
        cairo_set_source_rgba(cr, opts.bg_r, opts.bg_g, opts.bg_b, opts.bg_a);
        cairo_paint(cr);  // Fill entire surface with background color
    } else {
        // If background is transparent, clear to transparent
        cairo_set_source_rgba(cr, 0, 0, 0, 0);
        cairo_paint(cr);
    }
    
    // Set font and size
    cairo_set_font_face(cr, cairo_ft_face);
    cairo_set_font_size(cr, opts.font_size);
    cairo_set_font_options(cr, font_opts);
    
    // Draw outline and text - proper approach
    // Position the text with proper alignment in the center of the image
    double x_pos = opts.padding + opts.outline_width * 2 - bearing_x;  // Adjust for possible large outline
    double y_pos = opts.padding + opts.outline_width * 2 - bearing_y + opts.font_size;  // Adjust for font baseline
    
    // Move to the correct position
    cairo_move_to(cr, x_pos, y_pos);
    
    // Create the text path
    cairo_text_path(cr, text.c_str());
    
    // If outline width > 0, stroke with outline color first
    if (opts.outline_width > 0) {
        cairo_set_source_rgb(cr, opts.outline_r, opts.outline_g, opts.outline_b);
        cairo_set_line_width(cr, opts.outline_width);
        cairo_stroke_preserve(cr);  // Stroke the outline and preserve the path for fill
    }
    
    // Fill the text with text color
    cairo_set_source_rgb(cr, opts.text_r, opts.text_g, opts.text_b);
    cairo_fill(cr);
    
    // Write to PNG
    cairo_status_t status = cairo_surface_write_to_png(surface, filename.c_str());
    if (status != CAIRO_STATUS_SUCCESS) {
        std::cerr << "Error writing PNG: " << cairo_status_to_string(status) << std::endl;
    }
    
    // Cleanup
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    cairo_font_options_destroy(font_opts);
    cairo_destroy(ft_cr);
    cairo_surface_destroy(ft_surface);
    cairo_font_face_destroy(cairo_ft_face);
    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_library);
    FcPatternDestroy(pattern);
    FcPatternDestroy(font);
    FcConfigDestroy(config);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_prefix> [options]" << std::endl;
        std::cerr << "   or: " << argv[0] << " --list-fonts" << std::endl;
        std::cerr << "Options:" << std::endl;
        std::cerr << "  --font-name FONT       Font name (default: DejaVu Sans)" << std::endl;
        std::cerr << "  --font-size SIZE       Font size (default: 48)" << std::endl;
        std::cerr << "  --text-color COLOR     Text color (default: #FFFFFF)" << std::endl;
        std::cerr << "  --outline-color COLOR  Outline color (default: #000000)" << std::endl;
        std::cerr << "  --outline-width WIDTH  Outline width (default: 2)" << std::endl;
        std::cerr << "  --bg-color COLOR       Background color (default: transparent, #00000000)" << std::endl;
        std::cerr << "  --padding PADDING      Padding around text (default: 20)" << std::endl;
        std::cerr << "  -v, --verbose          Enable verbose output" << std::endl;
        return 1;
    }
    
    // Check if we're just listing fonts
    if (std::string(argv[1]) == "--list-fonts") {
        list_fonts();
        return 0;
    }
    
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <output_prefix> [options]" << std::endl;
        std::cerr << "   or: " << argv[0] << " --list-fonts" << std::endl;
        std::cerr << "Options:" << std::endl;
        std::cerr << "  --font-name FONT       Font name (default: DejaVu Sans)" << std::endl;
        std::cerr << "  --font-size SIZE       Font size (default: 48)" << std::endl;
        std::cerr << "  --text-color COLOR     Text color (default: #FFFFFF)" << std::endl;
        std::cerr << "  --outline-color COLOR  Outline color (default: #000000)" << std::endl;
        std::cerr << "  --outline-width WIDTH  Outline width (default: 2)" << std::endl;
        std::cerr << "  --bg-color COLOR       Background color (default: transparent, #00000000)" << std::endl;
        std::cerr << "  --padding PADDING      Padding around text (default: 20)" << std::endl;
        std::cerr << "  -v, --verbose          Enable verbose output" << std::endl;
        return 1;
    }
    
    std::string input_file = argv[1];
    std::string output_prefix = argv[2];
    
    TextOptions opts;
    opts.output_prefix = output_prefix;
    
    // Parse additional options
    for (int i = 3; i < argc; i++) {
        std::string opt = argv[i];
        if (opt == "--font-name" && i + 1 < argc) {
            opts.font_name = argv[++i];  // Increment i to skip the value
            if (opts.verbose) {
                std::cout << "Parsed: font-name = " << opts.font_name << std::endl;
            }
        } else if (opt == "--font-size" && i + 1 < argc) {
            opts.font_size = std::stoi(argv[++i]);  // Increment i to skip the value
            if (opts.verbose) {
                std::cout << "Parsed: font-size = " << opts.font_size << std::endl;
            }
        } else if (opt == "--text-color" && i + 1 < argc) {
            std::string color = argv[++i];  // Increment i to skip the value
            if (color[0] == '#') color = color.substr(1);
            if (color.length() == 6) {
                unsigned int r, g, b;
                int matched = sscanf(color.c_str(), "%02x%02x%02x", &r, &g, &b);
                if (matched == 3) {
                    opts.text_r = r / 255.0;
                    opts.text_g = g / 255.0;
                    opts.text_b = b / 255.0;
                    if (opts.verbose) {
                        std::cout << "Parsed: text-color = #" << color << " -> RGB(" << r << "," << g << "," << b << ")" << std::endl;
                    }
                } else {
                    std::cerr << "Warning: Could not parse text color: " << color << std::endl;
                }
            } else {
                std::cerr << "Warning: Invalid text color format: " << color << " (expected #RRGGBB)" << std::endl;
            }
        } else if (opt == "--outline-color" && i + 1 < argc) {
            std::string color = argv[++i];  // Increment i to skip the value
            if (color[0] == '#') color = color.substr(1);
            if (color.length() == 6) {
                unsigned int r, g, b;
                int matched = sscanf(color.c_str(), "%02x%02x%02x", &r, &g, &b);
                if (matched == 3) {
                    opts.outline_r = r / 255.0;
                    opts.outline_g = g / 255.0;
                    opts.outline_b = b / 255.0;
                    if (opts.verbose) {
                        std::cout << "Parsed: outline-color = #" << color << " -> RGB(" << r << "," << g << "," << b << ")" << std::endl;
                    }
                } else {
                    std::cerr << "Warning: Could not parse outline color: " << color << std::endl;
                }
            } else {
                std::cerr << "Warning: Invalid outline color format: " << color << " (expected #RRGGBB)" << std::endl;
            }
        } else if (opt == "--bg-color" && i + 1 < argc) {
            std::string color = argv[++i];  // Increment i to skip the value
            if (color[0] == '#') color = color.substr(1);
            if (color.length() == 6 || color.length() == 8) {  // Support both RGB and RGBA
                unsigned int r, g, b, a = 255;  // Default alpha to 255 (opaque) if not provided
                if (color.length() == 8) {  // RGBA format
                    sscanf(color.c_str(), "%02x%02x%02x%02x", &r, &g, &b, &a);
                } else {  // RGB format
                    sscanf(color.c_str(), "%02x%02x%02x", &r, &g, &b);
                }
                opts.bg_r = r / 255.0;
                opts.bg_g = g / 255.0;
                opts.bg_b = b / 255.0;
                opts.bg_a = a / 255.0;
                if (opts.verbose) {
                    std::cout << "Parsed: bg-color = #" << color << " -> RGB(" << r << "," << g << "," << b << "), A=" << a << std::endl;
                }
            }
        } else if (opt == "--outline-width" && i + 1 < argc) {
            opts.outline_width = std::stoi(argv[++i]);  // Increment i to skip the value
            if (opts.verbose) {
                std::cout << "Parsed: outline-width = " << opts.outline_width << std::endl;
            }
        } else if (opt == "--padding" && i + 1 < argc) {
            opts.padding = std::stoi(argv[++i]);  // Increment i to skip the value
            if (opts.verbose) {
                std::cout << "Parsed: padding = " << opts.padding << std::endl;
            }
        } else if (opt == "-v" || opt == "--verbose") {
            opts.verbose = true;
            std::cout << "Verbose mode enabled" << std::endl;
        }
        // For options that don't take a parameter (like -v/--verbose), no extra increment is needed
    }
    
    // Print final configuration in verbose mode
    if (opts.verbose) {
        print_final_config(opts);
    }
    
    // Read input file
    std::ifstream file(input_file);
    if (!file.is_open()) {
        std::cerr << "Could not open input file: " << input_file << std::endl;
        return 1;
    }
    
    std::string line;
    int line_number = 1;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            std::string output_filename = opts.output_prefix + std::to_string(line_number) + ".png";
            render_text_to_png(line, output_filename, opts);
            std::cout << "Created: " << output_filename << std::endl;
            line_number++;
        }
    }
    
    file.close();
    return 0;
}