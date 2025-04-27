#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <cmath>
#include <algorithm>

namespace colors {

// Color structure for RGB colors
struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    
    RGB() : r(0), g(0), b(0) {}
    RGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
    
    // Operator overloads for color manipulation
    RGB operator+(const RGB& other) const {
        return RGB(
            std::min(255, int(r) + int(other.r)),
            std::min(255, int(g) + int(other.g)),
            std::min(255, int(b) + int(other.b))
        );
    }
    
    RGB operator-(const RGB& other) const {
        return RGB(
            std::max(0, int(r) - int(other.r)),
            std::max(0, int(g) - int(other.g)),
            std::max(0, int(b) - int(other.b))
        );
    }
    
    RGB operator*(float factor) const {
        return RGB(
            std::clamp(int(std::round(r * factor)), 0, 255),
            std::clamp(int(std::round(g * factor)), 0, 255),
            std::clamp(int(std::round(b * factor)), 0, 255)
        );
    }
};

// HSL (Hue, Saturation, Lightness) color model
struct HSL {
    float h; // Hue [0-360]
    float s; // Saturation [0-1]
    float l; // Lightness [0-1]
    
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
}

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
std::string gradient_text(const std::string& text, const RGB& start, const RGB& end);

// Xterm 256 color palette helpers
uint8_t rgb_to_xterm256(const RGB& color);
RGB xterm256_to_rgb(uint8_t index);

// Get a color by its name
RGB get_color_by_name(const std::string& name);

// Named color constants - Extended palette of common web colors
namespace named {
    const RGB ALICE_BLUE(240, 248, 255);
    const RGB ANTIQUE_WHITE(250, 235, 215);
    const RGB AQUA(0, 255, 255);
    const RGB AQUAMARINE(127, 255, 212);
    const RGB AZURE(240, 255, 255);
    const RGB BEIGE(245, 245, 220);
    const RGB BISQUE(255, 228, 196);
    const RGB BLANCHED_ALMOND(255, 235, 205);
    const RGB BLUE_VIOLET(138, 43, 226);
    const RGB BROWN(165, 42, 42);
    const RGB BURLYWOOD(222, 184, 135);
    const RGB CADET_BLUE(95, 158, 160);
    const RGB CHARTREUSE(127, 255, 0);
    const RGB CHOCOLATE(210, 105, 30);
    const RGB CORAL(255, 127, 80);
    const RGB CORNFLOWER_BLUE(100, 149, 237);
    const RGB CORNSILK(255, 248, 220);
    const RGB CRIMSON(220, 20, 60);
    const RGB DARK_BLUE(0, 0, 139);
    const RGB DARK_CYAN(0, 139, 139);
    const RGB DARK_GOLDENROD(184, 134, 11);
    const RGB DARK_GRAY(169, 169, 169);
    const RGB DARK_GREEN(0, 100, 0);
    const RGB DARK_KHAKI(189, 183, 107);
    const RGB DARK_MAGENTA(139, 0, 139);
    const RGB DARK_OLIVE_GREEN(85, 107, 47);
    const RGB DARK_ORANGE(255, 140, 0);
    const RGB DARK_ORCHID(153, 50, 204);
    const RGB DARK_RED(139, 0, 0);
    const RGB DARK_SALMON(233, 150, 122);
    const RGB DARK_SEA_GREEN(143, 188, 143);
    const RGB DARK_SLATE_BLUE(72, 61, 139);
    const RGB DARK_SLATE_GRAY(47, 79, 79);
    const RGB DARK_TURQUOISE(0, 206, 209);
    const RGB DARK_VIOLET(148, 0, 211);
    const RGB DEEP_PINK(255, 20, 147);
    const RGB DEEP_SKY_BLUE(0, 191, 255);
    const RGB DIM_GRAY(105, 105, 105);
    const RGB DODGER_BLUE(30, 144, 255);
    const RGB FIREBRICK(178, 34, 34);
    const RGB FOREST_GREEN(34, 139, 34);
    const RGB GOLD(255, 215, 0);
    const RGB GOLDENROD(218, 165, 32);
    const RGB GRAY(128, 128, 128);
    const RGB HOT_PINK(255, 105, 180);
    const RGB INDIAN_RED(205, 92, 92);
    const RGB INDIGO(75, 0, 130);
    const RGB IVORY(255, 255, 240);
    const RGB KHAKI(240, 230, 140);
    const RGB LAVENDER(230, 230, 250);
    const RGB LAVENDER_BLUSH(255, 240, 245);
    const RGB LAWN_GREEN(124, 252, 0);
    const RGB LEMON_CHIFFON(255, 250, 205);
    const RGB LIGHT_BLUE(173, 216, 230);
    const RGB LIGHT_CORAL(240, 128, 128);
    const RGB LIGHT_CYAN(224, 255, 255);
    const RGB LIGHT_GOLDENROD(250, 250, 210);
    const RGB LIGHT_GRAY(211, 211, 211);
    const RGB LIGHT_GREEN(144, 238, 144);
    const RGB LIGHT_PINK(255, 182, 193);
    const RGB LIGHT_SALMON(255, 160, 122);
    const RGB LIGHT_SEA_GREEN(32, 178, 170);
    const RGB LIGHT_SKY_BLUE(135, 206, 250);
    const RGB LIGHT_SLATE_GRAY(119, 136, 153);
    const RGB LIGHT_STEEL_BLUE(176, 196, 222);
    const RGB LIGHT_YELLOW(255, 255, 224);
    const RGB LIME(0, 255, 0);
    const RGB LIME_GREEN(50, 205, 50);
    const RGB LINEN(250, 240, 230);
    const RGB MAROON(128, 0, 0);
    const RGB MEDIUM_AQUAMARINE(102, 205, 170);
    const RGB MEDIUM_BLUE(0, 0, 205);
    const RGB MEDIUM_ORCHID(186, 85, 211);
    const RGB MEDIUM_PURPLE(147, 112, 219);
    const RGB MEDIUM_SEA_GREEN(60, 179, 113);
    const RGB MEDIUM_SLATE_BLUE(123, 104, 238);
    const RGB MEDIUM_SPRING_GREEN(0, 250, 154);
    const RGB MEDIUM_TURQUOISE(72, 209, 204);
    const RGB MEDIUM_VIOLET_RED(199, 21, 133);
    const RGB MIDNIGHT_BLUE(25, 25, 112);
    const RGB MINT_CREAM(245, 255, 250);
    const RGB MISTY_ROSE(255, 228, 225);
    const RGB MOCCASIN(255, 228, 181);
    const RGB NAVAJO_WHITE(255, 222, 173);
    const RGB NAVY(0, 0, 128);
    const RGB OLD_LACE(253, 245, 230);
    const RGB OLIVE(128, 128, 0);
    const RGB OLIVE_DRAB(107, 142, 35);
    const RGB ORANGE(255, 165, 0);
    const RGB ORANGE_RED(255, 69, 0);
    const RGB ORCHID(218, 112, 214);
    const RGB PALE_GOLDENROD(238, 232, 170);
    const RGB PALE_GREEN(152, 251, 152);
    const RGB PALE_TURQUOISE(175, 238, 238);
    const RGB PALE_VIOLET_RED(219, 112, 147);
    const RGB PAPAYA_WHIP(255, 239, 213);
    const RGB PEACH_PUFF(255, 218, 185);
    const RGB PERU(205, 133, 63);
    const RGB PINK(255, 192, 203);
    const RGB PLUM(221, 160, 221);
    const RGB POWDER_BLUE(176, 224, 230);
    const RGB PURPLE(128, 0, 128);
    const RGB REBECCA_PURPLE(102, 51, 153);
    const RGB ROSY_BROWN(188, 143, 143);
    const RGB ROYAL_BLUE(65, 105, 225);
    const RGB SADDLE_BROWN(139, 69, 19);
    const RGB SALMON(250, 128, 114);
    const RGB SANDY_BROWN(244, 164, 96);
    const RGB SEA_GREEN(46, 139, 87);
    const RGB SEASHELL(255, 245, 238);
    const RGB SIENNA(160, 82, 45);
    const RGB SILVER(192, 192, 192);
    const RGB SKY_BLUE(135, 206, 235);
    const RGB SLATE_BLUE(106, 90, 205);
    const RGB SLATE_GRAY(112, 128, 144);
    const RGB SNOW(255, 250, 250);
    const RGB SPRING_GREEN(0, 255, 127);
    const RGB STEEL_BLUE(70, 130, 180);
    const RGB TAN(210, 180, 140);
    const RGB TEAL(0, 128, 128);
    const RGB THISTLE(216, 191, 216);
    const RGB TOMATO(255, 99, 71);
    const RGB TURQUOISE(64, 224, 208);
    const RGB VIOLET(238, 130, 238);
    const RGB WHEAT(245, 222, 179);
    const RGB WHITE_SMOKE(245, 245, 245);
    const RGB YELLOW_GREEN(154, 205, 50);
    const RGB ELECTRIC_PURPLE(191, 0, 255);
    const RGB GAINSBORO(220, 220, 220);
}

// ANSI escape sequence constants
namespace ansi {
    const std::string ESC = "\033[";
    const std::string RESET = "\033[0m";
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
}

} // namespace colors
