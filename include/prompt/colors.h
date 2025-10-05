#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace colors {

enum class ColorCapability : std::uint8_t {
    NO_COLOR,
    BASIC_COLOR,
    XTERM_256_COLOR,
    TRUE_COLOR
};

extern ColorCapability g_color_capability;

ColorCapability detect_color_capability();
void initialize_color_support(bool enabled);

struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    constexpr RGB() noexcept : r(0), g(0), b(0) {
    }
    constexpr RGB(uint8_t r, uint8_t g, uint8_t b) noexcept : r(r), g(g), b(b) {
    }

    constexpr RGB operator+(const RGB& other) const noexcept {
        return RGB(std::min(255, int(r) + int(other.r)), std::min(255, int(g) + int(other.g)),
                   std::min(255, int(b) + int(other.b)));
    }

    constexpr RGB operator-(const RGB& other) const noexcept {
        return RGB(std::max(0, int(r) - int(other.r)), std::max(0, int(g) - int(other.g)),
                   std::max(0, int(b) - int(other.b)));
    }

    constexpr RGB operator*(float factor) const noexcept {
        return RGB(std::clamp(int(std::round(r * factor)), 0, 255),
                   std::clamp(int(std::round(g * factor)), 0, 255),
                   std::clamp(int(std::round(b * factor)), 0, 255));
    }
};

namespace basic {
inline constexpr RGB BLACK(0, 0, 0);
inline constexpr RGB RED(170, 0, 0);
inline constexpr RGB GREEN(0, 170, 0);
inline constexpr RGB YELLOW(170, 85, 0);
inline constexpr RGB BLUE(0, 0, 170);
inline constexpr RGB MAGENTA(170, 0, 170);
inline constexpr RGB CYAN(0, 170, 170);
inline constexpr RGB WHITE(170, 170, 170);

inline constexpr RGB BRIGHT_BLACK(85, 85, 85);
inline constexpr RGB BRIGHT_RED(255, 85, 85);
inline constexpr RGB BRIGHT_GREEN(85, 255, 85);
inline constexpr RGB BRIGHT_YELLOW(255, 255, 85);
inline constexpr RGB BRIGHT_BLUE(85, 85, 255);
inline constexpr RGB BRIGHT_MAGENTA(255, 85, 255);
inline constexpr RGB BRIGHT_CYAN(85, 255, 255);
inline constexpr RGB BRIGHT_WHITE(255, 255, 255);
}  // namespace basic

std::string fg_color(const RGB& color);
std::string bg_color(const RGB& color);
std::string fg_color(uint8_t index);
std::string bg_color(uint8_t index);

RGB blend(const RGB& color1, const RGB& color2, float factor);

struct GradientSpec {
    RGB start;
    RGB end;
    std::string direction;

    GradientSpec() : start(RGB(0, 0, 0)), end(RGB(255, 255, 255)), direction("horizontal") {
    }
    GradientSpec(const RGB& start_color, const RGB& end_color,
                 const std::string& dir = "horizontal")
        : start(start_color), end(end_color), direction(dir) {
    }
};

std::vector<RGB> gradient(const RGB& start, const RGB& end, size_t steps);
std::string gradient_text(const std::string& text, const RGB& start, const RGB& end);

std::string gradient_bg(const std::string& text, const GradientSpec& spec);
std::string gradient_fg(const std::string& text, const GradientSpec& spec);
std::string gradient_bg_with_fg(const std::string& text, const GradientSpec& bg_spec,
                                const RGB& fg_rgb);
GradientSpec parse_gradient_value(const std::string& value);

bool is_gradient_value(const std::string& value);
std::string apply_color_or_gradient(const std::string& text, const std::string& color_value,
                                    bool is_foreground);
std::string apply_gradient_bg_with_fg(const std::string& text, const std::string& bg_value,
                                      const std::string& fg_value);

uint8_t rgb_to_xterm256(const RGB& color);
constexpr RGB xterm256_to_rgb(uint8_t index);

RGB get_color_by_name(const std::string& name);

struct NamedColor {
    std::string_view name;
    RGB color;
};

inline constexpr std::array<NamedColor, 16> g_basic_colors = {
    {{"BLACK", RGB(0, 0, 0)},
     {"RED", RGB(170, 0, 0)},
     {"GREEN", RGB(0, 170, 0)},
     {"YELLOW", RGB(170, 85, 0)},
     {"BLUE", RGB(0, 0, 170)},
     {"MAGENTA", RGB(170, 0, 170)},
     {"CYAN", RGB(0, 170, 170)},
     {"WHITE", RGB(170, 170, 170)},
     {"BRIGHT_BLACK", RGB(85, 85, 85)},
     {"BRIGHT_RED", RGB(255, 85, 85)},
     {"BRIGHT_GREEN", RGB(85, 255, 85)},
     {"BRIGHT_YELLOW", RGB(255, 255, 85)},
     {"BRIGHT_BLUE", RGB(85, 85, 255)},
     {"BRIGHT_MAGENTA", RGB(255, 85, 255)},
     {"BRIGHT_CYAN", RGB(85, 255, 255)},
     {"BRIGHT_WHITE", RGB(255, 255, 255)}}};

std::unordered_map<std::string, std::string> get_color_map();

std::string get_color_capability_string(ColorCapability capability);

namespace ansi {

inline constexpr const char* ESC = "\033[";
inline constexpr const char* RESET = "\033[0m";
inline constexpr const char* BG_RESET = "\033[49m";
inline constexpr const char* FG_RESET = "\033[39m";
inline constexpr const char* BOLD = "\033[1m";
inline constexpr const char* DIM = "\033[2m";
inline constexpr const char* ITALIC = "\033[3m";
inline constexpr const char* UNDERLINE = "\033[4m";
inline constexpr const char* BLINK = "\033[5m";

inline constexpr const char* HIDDEN = "\033[8m";
inline constexpr const char* STRIKETHROUGH = "\033[9m";

inline constexpr const char* FG_BLACK = "\033[30m";
inline constexpr const char* FG_RED = "\033[31m";
inline constexpr const char* FG_GREEN = "\033[32m";
inline constexpr const char* FG_YELLOW = "\033[33m";
inline constexpr const char* FG_BLUE = "\033[34m";
inline constexpr const char* FG_MAGENTA = "\033[35m";
inline constexpr const char* FG_CYAN = "\033[36m";
inline constexpr const char* FG_WHITE = "\033[37m";

inline constexpr const char* FG_BRIGHT_BLACK = "\033[90m";
inline constexpr const char* FG_BRIGHT_RED = "\033[91m";
inline constexpr const char* FG_BRIGHT_GREEN = "\033[92m";
inline constexpr const char* FG_BRIGHT_YELLOW = "\033[93m";
inline constexpr const char* FG_BRIGHT_BLUE = "\033[94m";
inline constexpr const char* FG_BRIGHT_MAGENTA = "\033[95m";
inline constexpr const char* FG_BRIGHT_CYAN = "\033[96m";
inline constexpr const char* FG_BRIGHT_WHITE = "\033[97m";

inline constexpr const char* BG_BLACK = "\033[40m";
inline constexpr const char* BG_RED = "\033[41m";
inline constexpr const char* BG_GREEN = "\033[42m";
inline constexpr const char* BG_YELLOW = "\033[43m";
inline constexpr const char* BG_BLUE = "\033[44m";
inline constexpr const char* BG_MAGENTA = "\033[45m";
inline constexpr const char* BG_CYAN = "\033[46m";
inline constexpr const char* BG_WHITE = "\033[47m";

inline constexpr const char* BG_BRIGHT_BLACK = "\033[100m";
inline constexpr const char* BG_BRIGHT_RED = "\033[101m";
inline constexpr const char* BG_BRIGHT_GREEN = "\033[102m";
inline constexpr const char* BG_BRIGHT_YELLOW = "\033[103m";
inline constexpr const char* BG_BRIGHT_BLUE = "\033[104m";
inline constexpr const char* BG_BRIGHT_MAGENTA = "\033[105m";
inline constexpr const char* BG_BRIGHT_CYAN = "\033[106m";
inline constexpr const char* BG_BRIGHT_WHITE = "\033[107m";
}  // namespace ansi
RGB parse_color_value(const std::string& value);
}  // namespace colors
