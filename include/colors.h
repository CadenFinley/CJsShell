#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace colors {

// Terminal color capability levels
enum class ColorCapability {
  NO_COLOR,         // Terminal doesn't support colors
  BASIC_COLOR,      // Basic 8/16 ANSI colors
  XTERM_256_COLOR,  // 256 color mode
  TRUE_COLOR        // 24-bit true color (RGB)
};

// Global variable to store detected color capability
extern ColorCapability g_color_capability;

// Function to detect and initialize terminal color capabilities
ColorCapability detect_color_capability();
void initialize_color_support(bool enabled);

// Color structure for RGB colors
struct RGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;

  RGB() : r(0), g(0), b(0) {}
  RGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}

  // Operator overloads for color manipulation
  RGB operator+(const RGB& other) const {
    return RGB(std::min(255, int(r) + int(other.r)),
               std::min(255, int(g) + int(other.g)),
               std::min(255, int(b) + int(other.b)));
  }

  RGB operator-(const RGB& other) const {
    return RGB(std::max(0, int(r) - int(other.r)),
               std::max(0, int(g) - int(other.g)),
               std::max(0, int(b) - int(other.b)));
  }

  RGB operator*(float factor) const {
    return RGB(std::clamp(int(std::round(r * factor)), 0, 255),
               std::clamp(int(std::round(g * factor)), 0, 255),
               std::clamp(int(std::round(b * factor)), 0, 255));
  }
};

// HSL (Hue, Saturation, Lightness) color model
struct HSL {
  float h;  // Hue [0-360]
  float s;  // Saturation [0-1]
  float l;  // Lightness [0-1]

  HSL() : h(0), s(0), l(0) {}
  HSL(float h, float s, float l) : h(h), s(s), l(l) {}
};

// Color conversion functions
HSL rgb_to_hsl(const RGB& rgb);
RGB hsl_to_rgb(const HSL& hsl);

// Standard ANSI 16 colors
namespace basic {
const RGB BLACK(0, 0, 0);
const RGB RED(170, 0, 0);
const RGB GREEN(0, 170, 0);
const RGB YELLOW(170, 85, 0);
const RGB BLUE(0, 0, 170);
const RGB MAGENTA(170, 0, 170);
const RGB CYAN(0, 170, 170);
const RGB WHITE(170, 170, 170);

// Bright versions
const RGB BRIGHT_BLACK(85, 85, 85);
const RGB BRIGHT_RED(255, 85, 85);
const RGB BRIGHT_GREEN(85, 255, 85);
const RGB BRIGHT_YELLOW(255, 255, 85);
const RGB BRIGHT_BLUE(85, 85, 255);
const RGB BRIGHT_MAGENTA(255, 85, 255);
const RGB BRIGHT_CYAN(85, 255, 255);
const RGB BRIGHT_WHITE(255, 255, 255);
}  // namespace basic

// Color generation functions
std::string fg_color(const RGB& color);  // Foreground color
std::string bg_color(const RGB& color);  // Background color
std::string fg_color(uint8_t index);     // 256-color mode foreground
std::string bg_color(uint8_t index);     // 256-color mode background

// Style functions
std::string style(const std::string& text, const RGB& fg);
std::string style(const std::string& text, const RGB& fg, const RGB& bg);
std::string style_bold(const std::string& text);
std::string style_italic(const std::string& text);
std::string style_underline(const std::string& text);
std::string style_blink(const std::string& text);
std::string style_reverse(const std::string& text);
std::string style_hidden(const std::string& text);
std::string style_reset();

// Color blending
RGB blend(const RGB& color1, const RGB& color2, float factor);

// Gradient generation
std::vector<RGB> gradient(const RGB& start, const RGB& end, size_t steps);
std::string gradient_text(const std::string& text, const RGB& start,
                          const RGB& end);

// Xterm 256 color palette helpers
uint8_t rgb_to_xterm256(const RGB& color);
RGB xterm256_to_rgb(uint8_t index);

// Get a color by its name
RGB get_color_by_name(const std::string& name);
std::unordered_map<std::string, std::string> get_color_map();

// Convert color capability enum to string representation
std::string get_color_capability_string(ColorCapability capability);

// ANSI escape sequence constants
namespace ansi {
const std::string ESC = "\033[";
const std::string RESET = "\033[0m";
const std::string BG_RESET = "\033[49m";  // Reset only the background color
const std::string FG_RESET = "\033[39m";  // Reset only the foreground color
const std::string BOLD = "\033[1m";
const std::string DIM = "\033[2m";
const std::string ITALIC = "\033[3m";
const std::string UNDERLINE = "\033[4m";
const std::string BLINK = "\033[5m";
const std::string REVERSE = "\033[7m";
const std::string HIDDEN = "\033[8m";
const std::string STRIKETHROUGH = "\033[9m";

// Normal foreground colors (30-37)
const std::string FG_BLACK = "\033[30m";
const std::string FG_RED = "\033[31m";
const std::string FG_GREEN = "\033[32m";
const std::string FG_YELLOW = "\033[33m";
const std::string FG_BLUE = "\033[34m";
const std::string FG_MAGENTA = "\033[35m";
const std::string FG_CYAN = "\033[36m";
const std::string FG_WHITE = "\033[37m";

// Bright foreground colors (90-97)
const std::string FG_BRIGHT_BLACK = "\033[90m";
const std::string FG_BRIGHT_RED = "\033[91m";
const std::string FG_BRIGHT_GREEN = "\033[92m";
const std::string FG_BRIGHT_YELLOW = "\033[93m";
const std::string FG_BRIGHT_BLUE = "\033[94m";
const std::string FG_BRIGHT_MAGENTA = "\033[95m";
const std::string FG_BRIGHT_CYAN = "\033[96m";
const std::string FG_BRIGHT_WHITE = "\033[97m";

// Normal background colors (40-47)
const std::string BG_BLACK = "\033[40m";
const std::string BG_RED = "\033[41m";
const std::string BG_GREEN = "\033[42m";
const std::string BG_YELLOW = "\033[43m";
const std::string BG_BLUE = "\033[44m";
const std::string BG_MAGENTA = "\033[45m";
const std::string BG_CYAN = "\033[46m";
const std::string BG_WHITE = "\033[47m";

// Bright background colors (100-107)
const std::string BG_BRIGHT_BLACK = "\033[100m";
const std::string BG_BRIGHT_RED = "\033[101m";
const std::string BG_BRIGHT_GREEN = "\033[102m";
const std::string BG_BRIGHT_YELLOW = "\033[103m";
const std::string BG_BRIGHT_BLUE = "\033[104m";
const std::string BG_BRIGHT_MAGENTA = "\033[105m";
const std::string BG_BRIGHT_CYAN = "\033[106m";
const std::string BG_BRIGHT_WHITE = "\033[107m";
}  // namespace ansi
RGB parse_color_value(const std::string& value);
}  // namespace colors
