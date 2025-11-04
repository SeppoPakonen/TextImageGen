// txt2png.cpp
// Build: g++ -std=gnu++17 -O2 -Wall -Wextra -o txt2png txt2png.cpp
// Requires: ImageMagick CLI ("magick" or "convert") available in PATH.
// Purpose: Read a text file and render each non-empty line to a transparent PNG.
// Features:
//  - Choose font by name or index (with --list-fonts to show available fonts)
//  - Text color, outline (stroke) color, and outline thickness
//  - Point size control
//  - Output file pattern: "<prefix><line_no>.png" (1-based), e.g., lyrics-12.png
//  - Uses ImageMagick "label:" rendering so the canvas auto-sizes to fit the text.
//
// Note on outline: We use ImageMagick's native stroke for quality & speed.
// If you *really* want the "offset halo" method, see --outline-method=offset (experimental).
// That method renders angular duplicates around the text. It is slower.
//
// License: MIT

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <regex>
#include <cmath>

#if defined(_WIN32)
#error "This tool targets Linux/Unix environments."
#endif

namespace {

// Try "magick", then "convert". Return the executable name that works, or empty string.
std::string detect_imagemagick() {
    auto try_cmd = [](const char* exe) -> bool {
        std::string cmd = std::string(exe) + " -version > /dev/null 2>&1";
        int rc = std::system(cmd.c_str());
        return rc == 0;
    };
    if (try_cmd("magick")) return "magick";
    if (try_cmd("convert")) return "convert";
    return "";
}

// Run a command and capture stdout as string. Returns empty string on failure.
std::string run_and_capture(const std::string& cmd) {
    std::array<char, 4096> buf{};
    std::string out;
    FILE* pipe = popen((cmd + " 2>/dev/null").c_str(), "r");
    if (!pipe) return out;
    while (true) {
        size_t n = fread(buf.data(), 1, buf.size(), pipe);
        if (n == 0) break;
        out.append(buf.data(), n);
    }
    pclose(pipe);
    return out;
}

// Trim helpers
static inline std::string ltrim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch){ return !std::isspace(ch); }));
    return s;
}
static inline std::string rtrim(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch){ return !std::isspace(ch); }).base(), s.end());
    return s;
}
static inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }

// Escape for ImageMagick label:"..."
// We keep it simple: escape backslashes and double-quotes.
std::string escape_for_label(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        if (c == '\\' || c == '\"') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

// Parse ImageMagick font list (magick -list font). Extract "Font:" names.
// Fallback: fc-list : family
std::vector<std::string> collect_fonts(const std::string& im_exe) {
    std::vector<std::string> fonts;

    // First try ImageMagick
    {
        std::string out = run_and_capture(im_exe + " -list font");
        if (!out.empty()) {
            std::regex re("^\\s*Font:\\s*(.+)\\s*$");
            std::istringstream iss(out);
            std::string line;
            while (std::getline(iss, line)) {
                std::smatch m;
                if (std::regex_search(line, m, re)) {
                    std::string name = trim(m[1].str());
                    if (!name.empty()) fonts.push_back(name);
                }
            }
        }
    }

    // If IM didn't give anything useful, fall back to fontconfig
    if (fonts.empty()) {
        std::string out = run_and_capture("fc-list : family");
        if (!out.empty()) {
            std::istringstream iss(out);
            std::string line;
            while (std::getline(iss, line)) {
                // fc-list can output "DejaVu Sans:style=Book"
                auto pos = line.find(':');
                std::string fam = (pos == std::string::npos) ? line : line.substr(0, pos);
                fam = trim(fam);
                if (!fam.empty()) fonts.push_back(fam);
            }
            // dedup while preserving order
            std::vector<std::string> dedup;
            for (auto &f : fonts) {
                if (std::find(dedup.begin(), dedup.end(), f) == dedup.end()) dedup.push_back(f);
            }
            fonts.swap(dedup);
        }
    }

    return fonts;
}

struct Options {
    std::string input_path;
    std::string prefix = "";
    int start_index = 1;
    std::string font_name = "";
    int font_index = -1;
    int point_size = 64;
    std::string fill_color = "#FFFFFF";
    std::string outline_color = "#000000";
    int outline_thickness = 4;
    bool list_fonts = false;
    bool use_offset_outline = false;
    int offset_directions = 36; // for experimental offset method
    bool dry_run = false;
    std::string im_exe = ""; // detected at runtime
};

void print_help(const char* argv0) {
    std::cout << "Usage:\n"
              << "  " << argv0 << " --input FILE [options]\n\n"
              << "Options:\n"
              << "  --input FILE            Input text file (one image per non-empty line)\n"
              << "  --prefix STR            Output prefix. Files are '<prefix><N>.png' (default: none)\n"
              << "  --start-index N         First line number for filenames (default: 1)\n"
              << "  --font NAME             Font family/name to use\n"
              << "  --font-index N          Pick font by 1-based index from --list-fonts\n"
              << "  --size N                Point size (default: 64)\n"
              << "  --color HEX             Text fill color (default: #FFFFFF)\n"
              << "  --outline-color HEX     Outline (stroke) color (default: #000000)\n"
              << "  --outline N             Outline thickness in px (default: 4)\n"
              << "  --list-fonts            Print available fonts with indices and exit\n"
              << "  --outline-method METHOD Outline method: 'stroke' (default) or 'offset'\n"
              << "  --offset-directions N   Directions for 'offset' halo (default: 36)\n"
              << "  --dry-run               Show commands but do not execute\n"
              << "  --help                  Show this help\n\n"
              << "Notes:\n"
              << "  * Requires ImageMagick CLI ('magick' or 'convert') in PATH.\n"
              << "  * PNGs are written with transparent background (PNG32:).\n"
              << "  * The 'offset' method duplicates the text in a ring to fake an outline. Slower.\n";
}

bool parse_args(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need_val = [&](const char* flag) {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << flag << "\n";
                return false;
            }
            return true;
        };

        if (a == "--help" || a == "-h") { print_help(argv[0]); std::exit(0); }
        else if (a == "--input") { if (!need_val("--input")) return false; opt.input_path = argv[++i]; }
        else if (a == "--prefix") { if (!need_val("--prefix")) return false; opt.prefix = argv[++i]; }
        else if (a == "--start-index") { if (!need_val("--start-index")) return false; opt.start_index = std::stoi(argv[++i]); }
        else if (a == "--font") { if (!need_val("--font")) return false; opt.font_name = argv[++i]; }
        else if (a == "--font-index") { if (!need_val("--font-index")) return false; opt.font_index = std::stoi(argv[++i]); }
        else if (a == "--size") { if (!need_val("--size")) return false; opt.point_size = std::stoi(argv[++i]); }
        else if (a == "--color") { if (!need_val("--color")) return false; opt.fill_color = argv[++i]; }
        else if (a == "--outline-color") { if (!need_val("--outline-color")) return false; opt.outline_color = argv[++i]; }
        else if (a == "--outline") { if (!need_val("--outline")) return false; opt.outline_thickness = std::stoi(argv[++i]); }
        else if (a == "--list-fonts") { opt.list_fonts = true; }
        else if (a == "--outline-method") {
            if (!need_val("--outline-method")) return false;
            std::string m = argv[++i];
            if (m == "stroke") opt.use_offset_outline = false;
            else if (m == "offset") opt.use_offset_outline = true;
            else { std::cerr << "Unknown outline method: " << m << "\n"; return false; }
        }
        else if (a == "--offset-directions") { if (!need_val("--offset-directions")) return false; opt.offset_directions = std::stoi(argv[++i]); }
        else if (a == "--dry-run") { opt.dry_run = true; }
        else {
            std::cerr << "Unknown option: " << a << "\n";
            return false;
        }
    }
    if (opt.input_path.empty() && !opt.list_fonts) {
        std::cerr << "Error: --input FILE required (unless using --list-fonts)\n";
        return false;
    }
    return true;
}

// Build IM command using "stroke" method.
std::string build_cmd_stroke(const std::string& im_exe,
                             const std::string& text,
                             const std::string& font,
                             int point_size,
                             const std::string& fill_color,
                             const std::string& outline_color,
                             int outline_thickness,
                             const std::string& out_path) {
    std::ostringstream cmd;
    cmd << im_exe
        << " -background none"
        << " -fill \"" << fill_color << "\""
        << " -stroke \"" << outline_color << "\""
        << " -strokewidth " << outline_thickness;
    if (!font.empty()) cmd << " -font \"" << font << "\"";
    cmd << " -pointsize " << point_size
        << " label:\"" << escape_for_label(text) << "\""
        << " PNG32:\"" << out_path << "\"";
    return cmd.str();
}

// Build IM command using "offset halo" method: draw ring of shifted texts + main text.
std::string build_cmd_offset(const std::string& im_exe,
                             const std::string& text,
                             const std::string& font,
                             int point_size,
                             const std::string& fill_color,
                             const std::string& outline_color,
                             int outline_thickness,
                             int directions,
                             const std::string& out_path) {
    // Strategy: create base label once, then compose shifted layers.
    // We render a text-only image (no stroke), then use -shadow-like offset clones.
    // Implementation: use mpr: to store the base, then build a montage via -gravity center -geometry +dx+dy -background none -layers merge
    // Note: This is slower but matches the "cheap" requirement.
    std::ostringstream cmd;
    const std::string esc = escape_for_label(text);
    cmd << im_exe
        << " -background none";
    if (!font.empty()) cmd << " -font \"" << font << "\"";
    cmd << " -pointsize " << point_size
        << " -fill \"" << outline_color << "\""
        << " label:\"" << esc << "\""
        << " -write mpr:outline +delete "
        << " mpr:outline";
    // Generate shifted outline copies
    double r = static_cast<double>(outline_thickness);
    for (int k = 0; k < directions; ++k) {
        double angle = (2.0 * 3.14159265358979323846 * k) / directions;
        int dx = static_cast<int>(std::round(r * std::cos(angle)));
        int dy = static_cast<int>(std::round(r * std::sin(angle)));
        cmd << " mpr:outline -background none -gravity center -geometry +" << dx << "+" << dy << " -compose over -composite";
    }
    // Now draw the main text on top in fill color
    cmd << " ( -background none";
    if (!font.empty()) cmd << " -font \"" << font << "\"";
    cmd << " -pointsize " << point_size
        << " -fill \"" << fill_color << "\""
        << " label:\"" << esc << "\" ) -gravity center -compose over -composite"
        << " PNG32:\"" << out_path << "\"";
    return cmd.str();
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parse_args(argc, argv, opt)) {
        std::cerr << "Use --help for usage.\n";
        return 2;
    }

    opt.im_exe = detect_imagemagick();
    if (opt.im_exe.empty()) {
        std::cerr << "Error: Could not find ImageMagick CLI ('magick' or 'convert') in PATH.\n";
        return 3;
    }

    // Fonts
    auto fonts = collect_fonts(opt.im_exe);
    if (opt.list_fonts) {
        if (fonts.empty()) {
            std::cerr << "No fonts found via ImageMagick or fontconfig.\n";
            return 4;
        }
        for (size_t i = 0; i < fonts.size(); ++i) {
            std::cout << (i + 1) << ": " << fonts[i] << "\n";
        }
        return 0;
    }

    // Resolve font by index if provided
    std::string font = opt.font_name;
    if (opt.font_index > 0) {
        if (fonts.empty()) fonts = collect_fonts(opt.im_exe);
        if (opt.font_index < 1 || static_cast<size_t>(opt.font_index) > fonts.size()) {
            std::cerr << "Invalid --font-index " << opt.font_index << " (have " << fonts.size() << " fonts)\n";
            return 5;
        }
        font = fonts[static_cast<size_t>(opt.font_index - 1)];
    }

    // Read input file
    std::ifstream in(opt.input_path);
    if (!in) {
        std::cerr << "Error: cannot open input file: " << opt.input_path << "\n";
        return 6;
    }

    std::string line;
    int lineno = opt.start_index;
    int made = 0;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty()) { ++lineno; continue; }

        std::ostringstream outname;
        outname << opt.prefix << lineno << ".png";
        std::string out_path = outname.str();

        std::string cmd;
        if (!opt.use_offset_outline) {
            cmd = build_cmd_stroke(opt.im_exe, t, font, opt.point_size, opt.fill_color, opt.outline_color, opt.outline_thickness, out_path);
        } else {
            cmd = build_cmd_offset(opt.im_exe, t, font, opt.point_size, opt.fill_color, opt.outline_color, opt.outline_thickness, opt.offset_directions, out_path);
        }

        if (opt.dry_run) {
            std::cout << cmd << "\n";
        } else {
            int rc = std::system(cmd.c_str());
            if (rc != 0) {
                std::cerr << "Command failed (rc=" << rc << "): " << cmd << "\n";
                return 7;
            }
            ++made;
        }

        ++lineno;
    }

    if (!opt.dry_run) {
        std::cerr << "Wrote " << made << " PNG files.\n";
    }
    return 0;
}
