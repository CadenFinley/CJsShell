#include "colors.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <cstdlib>
#include <cctype>
#include <climits>

namespace colors {

// Initialize global variable
ColorCapability g_color_capability = ColorCapability::BASIC_COLOR;

ColorCapability detect_color_capability() {
    // Check environment variables to determine color support
    const char* colorterm = std::getenv("COLORTERM");
    const char* term = std::getenv("TERM");
    const char* no_color = std::getenv("NO_COLOR");
    const char* force_color = std::getenv("FORCE_COLOR");
    
    // If NO_COLOR is set (any value), disable colors
    if (no_color && no_color[0] != '\0') {
        return ColorCapability::NO_COLOR;
    }
    
    // If FORCE_COLOR is explicitly set to "true", force true color support
    if (force_color && std::string(force_color) == "true") {
        return ColorCapability::TRUE_COLOR;
    }
    
    // Check for truecolor/24bit support
    if (colorterm) {
        std::string colortermStr = colorterm;
        std::transform(colortermStr.begin(), colortermStr.end(), colortermStr.begin(),
                      [](unsigned char c){ return std::tolower(c); });
        
        if (colortermStr.find("truecolor") != std::string::npos || 
            colortermStr.find("24bit") != std::string::npos) {
            return ColorCapability::TRUE_COLOR;
        }
    }
    
    // Check if terminal supports 256 colors
    if (term) {
        std::string termStr = term;
        if (termStr.find("256") != std::string::npos || 
            termStr.find("xterm") != std::string::npos) {
            return ColorCapability::XTERM_256_COLOR;
        }
    }
    
    // Default to basic color support
    return ColorCapability::BASIC_COLOR;
}

void initialize_color_support() {
    g_color_capability = detect_color_capability();
}

// Helper function to get the closest basic ANSI color for an RGB color
uint8_t get_closest_ansi_color(const RGB& color) {
    // Basic ANSI colors (0-15)
    const std::array<RGB, 16> basic_colors = {{
        {0, 0, 0},       // Black
        {170, 0, 0},     // Red
        {0, 170, 0},     // Green
        {170, 85, 0},    // Yellow
        {0, 0, 170},     // Blue
        {170, 0, 170},   // Magenta
        {0, 170, 170},   // Cyan
        {170, 170, 170}, // White
        {85, 85, 85},    // Bright Black
        {255, 85, 85},   // Bright Red
        {85, 255, 85},   // Bright Green
        {255, 255, 85},  // Bright Yellow
        {85, 85, 255},   // Bright Blue
        {255, 85, 255},  // Bright Magenta
        {85, 255, 255},  // Bright Cyan
        {255, 255, 255}  // Bright White
    }};
    
    uint8_t closest_index = 0;
    int closest_distance = INT_MAX;
    
    for (size_t i = 0; i < basic_colors.size(); i++) {
        const RGB& c = basic_colors[i];
        int distance = (c.r - color.r) * (c.r - color.r) +
                      (c.g - color.g) * (c.g - color.g) +
                      (c.b - color.b) * (c.b - color.b);
        
        if (distance < closest_distance) {
            closest_distance = distance;
            closest_index = i;
        }
    }
    
    return closest_index;
}

HSL rgb_to_hsl(const RGB& rgb) {
    float r = rgb.r / 255.0f;
    float g = rgb.g / 255.0f;
    float b = rgb.b / 255.0f;
    
    float max = std::max(std::max(r, g), b);
    float min = std::min(std::min(r, g), b);
    float h = 0, s = 0, l = (max + min) / 2.0f;
    
    if (max != min) {
        float d = max - min;
        s = l > 0.5f ? d / (2.0f - max - min) : d / (max + min);
        
        if (max == r) {
            h = (g - b) / d + (g < b ? 6.0f : 0.0f);
        } else if (max == g) {
            h = (b - r) / d + 2.0f;
        } else {
            h = (r - g) / d + 4.0f;
        }
        
        h /= 6.0f;
    }
    
    return HSL(h * 360.0f, s, l);
}

RGB hsl_to_rgb(const HSL& hsl) {
    float h = hsl.h / 360.0f;
    float s = hsl.s;
    float l = hsl.l;
    
    if (s == 0) {
        // Achromatic (grey)
        int gray = static_cast<int>(l * 255);
        return RGB(gray, gray, gray);
    }
    
    auto hue_to_rgb = [](float p, float q, float t) {
        if (t < 0) t += 1;
        if (t > 1) t -= 1;
        if (t < 1.0f/6.0f) return p + (q - p) * 6.0f * t;
        if (t < 1.0f/2.0f) return q;
        if (t < 2.0f/3.0f) return p + (q - p) * (2.0f/3.0f - t) * 6.0f;
        return p;
    };
    
    float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
    float p = 2 * l - q;
    
    float r = hue_to_rgb(p, q, h + 1.0f/3.0f);
    float g = hue_to_rgb(p, q, h);
    float b = hue_to_rgb(p, q, h - 1.0f/3.0f);
    
    return RGB(
        static_cast<uint8_t>(r * 255),
        static_cast<uint8_t>(g * 255),
        static_cast<uint8_t>(b * 255)
    );
}

std::string fg_color(const RGB& color) {
    std::stringstream ss;
    
    switch (g_color_capability) {
        case ColorCapability::NO_COLOR:
            return "";
            
        case ColorCapability::BASIC_COLOR: {
            uint8_t ansi_color = get_closest_ansi_color(color);
            if (ansi_color < 8) {
                ss << "\033[3" << static_cast<int>(ansi_color) << "m";
            } else {
                ss << "\033[9" << static_cast<int>(ansi_color - 8) << "m";
            }
            break;
        }
            
        case ColorCapability::XTERM_256_COLOR:
            ss << "\033[38;5;" << static_cast<int>(rgb_to_xterm256(color)) << "m";
            break;
            
        case ColorCapability::TRUE_COLOR:
            ss << "\033[38;2;" << static_cast<int>(color.r) << ";" 
               << static_cast<int>(color.g) << ";" 
               << static_cast<int>(color.b) << "m";
            break;
    }
    
    return ss.str();
}

std::string bg_color(const RGB& color) {
    std::stringstream ss;
    
    switch (g_color_capability) {
        case ColorCapability::NO_COLOR:
            return "";
            
        case ColorCapability::BASIC_COLOR: {
            uint8_t ansi_color = get_closest_ansi_color(color);
            if (ansi_color < 8) {
                ss << "\033[4" << static_cast<int>(ansi_color) << "m";
            } else {
                ss << "\033[10" << static_cast<int>(ansi_color - 8) << "m";
            }
            break;
        }
            
        case ColorCapability::XTERM_256_COLOR:
            ss << "\033[48;5;" << static_cast<int>(rgb_to_xterm256(color)) << "m";
            break;
            
        case ColorCapability::TRUE_COLOR:
            ss << "\033[48;2;" << static_cast<int>(color.r) << ";" 
               << static_cast<int>(color.g) << ";" 
               << static_cast<int>(color.b) << "m";
            break;
    }
    
    return ss.str();
}

std::string fg_color(uint8_t index) {
    if (g_color_capability == ColorCapability::NO_COLOR) {
        return "";
    }
    
    if (g_color_capability == ColorCapability::BASIC_COLOR && index >= 16) {
        // Map to closest basic color
        return fg_color(xterm256_to_rgb(index));
    }
    
    std::stringstream ss;
    ss << "\033[38;5;" << static_cast<int>(index) << "m";
    return ss.str();
}

std::string bg_color(uint8_t index) {
    if (g_color_capability == ColorCapability::NO_COLOR) {
        return "";
    }
    
    if (g_color_capability == ColorCapability::BASIC_COLOR && index >= 16) {
        // Map to closest basic color
        return bg_color(xterm256_to_rgb(index));
    }
    
    std::stringstream ss;
    ss << "\033[48;5;" << static_cast<int>(index) << "m";
    return ss.str();
}

std::string style(const std::string& text, const RGB& fg) {
    return fg_color(fg) + text + ansi::RESET;
}

std::string style(const std::string& text, const RGB& fg, const RGB& bg) {
    return fg_color(fg) + bg_color(bg) + text + ansi::RESET;
}

std::string style_bold(const std::string& text) {
    return ansi::BOLD + text + ansi::RESET;
}

std::string style_italic(const std::string& text) {
    return ansi::ITALIC + text + ansi::RESET;
}

std::string style_underline(const std::string& text) {
    return ansi::UNDERLINE + text + ansi::RESET;
}

std::string style_blink(const std::string& text) {
    return ansi::BLINK + text + ansi::RESET;
}

std::string style_reverse(const std::string& text) {
    return ansi::REVERSE + text + ansi::RESET;
}

std::string style_hidden(const std::string& text) {
    return ansi::HIDDEN + text + ansi::RESET;
}

std::string style_reset() {
    return ansi::RESET;
}

RGB blend(const RGB& color1, const RGB& color2, float factor) {
    return RGB(
        static_cast<uint8_t>(color1.r * (1 - factor) + color2.r * factor),
        static_cast<uint8_t>(color1.g * (1 - factor) + color2.g * factor),
        static_cast<uint8_t>(color1.b * (1 - factor) + color2.b * factor)
    );
}

std::vector<RGB> gradient(const RGB& start, const RGB& end, size_t steps) {
    std::vector<RGB> result;
    result.reserve(steps);
    
    for (size_t i = 0; i < steps; ++i) {
        float factor = static_cast<float>(i) / (steps - 1);
        result.push_back(blend(start, end, factor));
    }
    
    return result;
}

std::string gradient_text(const std::string& text, const RGB& start, const RGB& end) {
    if (text.empty()) return "";
    if (g_color_capability == ColorCapability::NO_COLOR) return text;
    
    std::string result;
    size_t steps = text.length();
    
    // Handle single character case to prevent division by zero in gradient function
    if (steps == 1) {
        return fg_color(start) + text + (g_color_capability != ColorCapability::NO_COLOR ? ansi::RESET : "");
    }
    
    // For basic color capability, use a simpler approach with fewer distinct colors
    if (g_color_capability == ColorCapability::BASIC_COLOR) {
        // Use just start color for first half, end color for second half
        size_t halfway = steps / 2;
        for (size_t i = 0; i < steps; ++i) {
            if (i < halfway) {
                result += fg_color(start) + text.substr(i, 1);
            } else {
                result += fg_color(end) + text.substr(i, 1);
            }
        }
    } else {
        // Full gradient for terminals that support it
        std::vector<RGB> colors = gradient(start, end, steps);
        for (size_t i = 0; i < steps; ++i) {
            result += fg_color(colors[i]) + text.substr(i, 1);
        }
    }
    
    if (g_color_capability != ColorCapability::NO_COLOR) {
        result += ansi::RESET;
    }
    return result;
}

uint8_t rgb_to_xterm256(const RGB& color) {
    // Simplified conversion algorithm
    // For more accurate conversion, a lookup table would be better
    
    // 16-231: 6x6x6 RGB color cube
    int r = static_cast<int>(round(color.r / 255.0 * 5.0));
    int g = static_cast<int>(round(color.g / 255.0 * 5.0));
    int b = static_cast<int>(round(color.b / 255.0 * 5.0));
    
    return static_cast<uint8_t>(16 + 36 * r + 6 * g + b);
}

RGB xterm256_to_rgb(uint8_t index) {
    // Handle basic 16 colors
    if (index < 16) {
        // Return appropriate ANSI color
        switch (index) {
            case 0: return RGB(0, 0, 0);       // Black
            case 1: return RGB(170, 0, 0);     // Red
            case 2: return RGB(0, 170, 0);     // Green
            case 3: return RGB(170, 85, 0);    // Yellow
            case 4: return RGB(0, 0, 170);     // Blue
            case 5: return RGB(170, 0, 170);   // Magenta
            case 6: return RGB(0, 170, 170);   // Cyan
            case 7: return RGB(170, 170, 170); // White
            case 8: return RGB(85, 85, 85);    // Bright Black
            case 9: return RGB(255, 85, 85);   // Bright Red
            case 10: return RGB(85, 255, 85);  // Bright Green
            case 11: return RGB(255, 255, 85); // Bright Yellow
            case 12: return RGB(85, 85, 255);  // Bright Blue
            case 13: return RGB(255, 85, 255); // Bright Magenta
            case 14: return RGB(85, 255, 255); // Bright Cyan
            case 15: return RGB(255, 255, 255);// Bright White
        }
    }
    
    // Handle 6x6x6 color cube (16-231)
    if (index >= 16 && index <= 231) {
        index -= 16;
        int r = (index / 36) * 51;
        int g = ((index % 36) / 6) * 51;
        int b = (index % 6) * 51;
        return RGB(r, g, b);
    }
    
    // Handle grayscale (232-255)
    if (index >= 232) {
        int gray = (index - 232) * 10 + 8;
        return RGB(gray, gray, gray);
    }
    
    // Default fallback
    return RGB(0, 0, 0);
}

//Add a new function to get color by name
RGB get_color_by_name(const std::string& name) {
    // Convert to uppercase for case-insensitive comparison
    std::string upper_name = name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper);
    
    // Map of color names to RGB values
    static const std::unordered_map<std::string, RGB> color_map = {
        {"ALICE_BLUE", named::ALICE_BLUE},
        {"ANTIQUE_WHITE", named::ANTIQUE_WHITE},
        {"AQUA", named::AQUA},
        {"AQUAMARINE", named::AQUAMARINE},
        {"AZURE", named::AZURE},
        {"BEIGE", named::BEIGE},
        {"BISQUE", named::BISQUE},
        {"BLANCHED_ALMOND", named::BLANCHED_ALMOND},
        {"BLUE_VIOLET", named::BLUE_VIOLET},
        {"BROWN", named::BROWN},
        {"BURLYWOOD", named::BURLYWOOD},
        {"CADET_BLUE", named::CADET_BLUE},
        {"CHARTREUSE", named::CHARTREUSE},
        {"CHOCOLATE", named::CHOCOLATE},
        {"CORAL", named::CORAL},
        {"CORNFLOWER_BLUE", named::CORNFLOWER_BLUE},
        {"CORNSILK", named::CORNSILK},
        {"CRIMSON", named::CRIMSON},
        {"DARK_BLUE", named::DARK_BLUE},
        {"DARK_CYAN", named::DARK_CYAN},
        {"DARK_GOLDENROD", named::DARK_GOLDENROD},
        {"DARK_GRAY", named::DARK_GRAY},
        {"DARK_GREEN", named::DARK_GREEN},
        {"DARK_KHAKI", named::DARK_KHAKI},
        {"DARK_MAGENTA", named::DARK_MAGENTA},
        {"DARK_OLIVE_GREEN", named::DARK_OLIVE_GREEN},
        {"DARK_ORANGE", named::DARK_ORANGE},
        {"DARK_ORCHID", named::DARK_ORCHID},
        {"DARK_RED", named::DARK_RED},
        {"DARK_SALMON", named::DARK_SALMON},
        {"DARK_SEA_GREEN", named::DARK_SEA_GREEN},
        {"DARK_SLATE_BLUE", named::DARK_SLATE_BLUE},
        {"DARK_SLATE_GRAY", named::DARK_SLATE_GRAY},
        {"DARK_TURQUOISE", named::DARK_TURQUOISE},
        {"DARK_VIOLET", named::DARK_VIOLET},
        {"DEEP_PINK", named::DEEP_PINK},
        {"DEEP_SKY_BLUE", named::DEEP_SKY_BLUE},
        {"DIM_GRAY", named::DIM_GRAY},
        {"DODGER_BLUE", named::DODGER_BLUE},
        {"FIREBRICK", named::FIREBRICK},
        {"FOREST_GREEN", named::FOREST_GREEN},
        {"GAINSBORO", named::GAINSBORO},
        {"GOLD", named::GOLD},
        {"GOLDENROD", named::GOLDENROD},
        {"GRAY", named::GRAY},
        {"HOT_PINK", named::HOT_PINK},
        {"INDIAN_RED", named::INDIAN_RED},
        {"INDIGO", named::INDIGO},
        {"IVORY", named::IVORY},
        {"KHAKI", named::KHAKI},
        {"LAVENDER", named::LAVENDER},
        {"LAVENDER_BLUSH", named::LAVENDER_BLUSH},
        {"LAWN_GREEN", named::LAWN_GREEN},
        {"LEMON_CHIFFON", named::LEMON_CHIFFON},
        {"LIGHT_BLUE", named::LIGHT_BLUE},
        {"LIGHT_CORAL", named::LIGHT_CORAL},
        {"LIGHT_CYAN", named::LIGHT_CYAN},
        {"LIGHT_GOLDENROD", named::LIGHT_GOLDENROD},
        {"LIGHT_GRAY", named::LIGHT_GRAY},
        {"LIGHT_GREEN", named::LIGHT_GREEN},
        {"LIGHT_PINK", named::LIGHT_PINK},
        {"LIGHT_SALMON", named::LIGHT_SALMON},
        {"LIGHT_SEA_GREEN", named::LIGHT_SEA_GREEN},
        {"LIGHT_SKY_BLUE", named::LIGHT_SKY_BLUE},
        {"LIGHT_SLATE_GRAY", named::LIGHT_SLATE_GRAY},
        {"LIGHT_STEEL_BLUE", named::LIGHT_STEEL_BLUE},
        {"LIGHT_YELLOW", named::LIGHT_YELLOW},
        {"LIME", named::LIME},
        {"LIME_GREEN", named::LIME_GREEN},
        {"LINEN", named::LINEN},
        {"MAROON", named::MAROON},
        {"MEDIUM_AQUAMARINE", named::MEDIUM_AQUAMARINE},
        {"MEDIUM_BLUE", named::MEDIUM_BLUE},
        {"MEDIUM_ORCHID", named::MEDIUM_ORCHID},
        {"MEDIUM_PURPLE", named::MEDIUM_PURPLE},
        {"MEDIUM_SEA_GREEN", named::MEDIUM_SEA_GREEN},
        {"MEDIUM_SLATE_BLUE", named::MEDIUM_SLATE_BLUE},
        {"MEDIUM_SPRING_GREEN", named::MEDIUM_SPRING_GREEN},
        {"MEDIUM_TURQUOISE", named::MEDIUM_TURQUOISE},
        {"MEDIUM_VIOLET_RED", named::MEDIUM_VIOLET_RED},
        {"MIDNIGHT_BLUE", named::MIDNIGHT_BLUE},
        {"MINT_CREAM", named::MINT_CREAM},
        {"MISTY_ROSE", named::MISTY_ROSE},
        {"MOCCASIN", named::MOCCASIN},
        {"NAVAJO_WHITE", named::NAVAJO_WHITE},
        {"NAVY", named::NAVY},
        {"OLD_LACE", named::OLD_LACE},
        {"OLIVE", named::OLIVE},
        {"OLIVE_DRAB", named::OLIVE_DRAB},
        {"ORANGE", named::ORANGE},
        {"ORANGE_RED", named::ORANGE_RED},
        {"ORCHID", named::ORCHID},
        {"PALE_GOLDENROD", named::PALE_GOLDENROD},
        {"PALE_GREEN", named::PALE_GREEN},
        {"PALE_TURQUOISE", named::PALE_TURQUOISE},
        {"PALE_VIOLET_RED", named::PALE_VIOLET_RED},
        {"PAPAYA_WHIP", named::PAPAYA_WHIP},
        {"PEACH_PUFF", named::PEACH_PUFF},
        {"PERU", named::PERU},
        {"PINK", named::PINK},
        {"PLUM", named::PLUM},
        {"POWDER_BLUE", named::POWDER_BLUE},
        {"PURPLE", named::PURPLE},
        {"REBECCA_PURPLE", named::REBECCA_PURPLE},
        {"ROSY_BROWN", named::ROSY_BROWN},
        {"ROYAL_BLUE", named::ROYAL_BLUE},
        {"SADDLE_BROWN", named::SADDLE_BROWN},
        {"SALMON", named::SALMON},
        {"SANDY_BROWN", named::SANDY_BROWN},
        {"SEA_GREEN", named::SEA_GREEN},
        {"SEASHELL", named::SEASHELL},
        {"SIENNA", named::SIENNA},
        {"SILVER", named::SILVER},
        {"SKY_BLUE", named::SKY_BLUE},
        {"SLATE_BLUE", named::SLATE_BLUE},
        {"SLATE_GRAY", named::SLATE_GRAY},
        {"SNOW", named::SNOW},
        {"SPRING_GREEN", named::SPRING_GREEN},
        {"STEEL_BLUE", named::STEEL_BLUE},
        {"TAN", named::TAN},
        {"TEAL", named::TEAL},
        {"THISTLE", named::THISTLE},
        {"TOMATO", named::TOMATO},
        {"TURQUOISE", named::TURQUOISE},
        {"VIOLET", named::VIOLET},
        {"WHEAT", named::WHEAT},
        {"WHITE_SMOKE", named::WHITE_SMOKE},
        {"YELLOW_GREEN", named::YELLOW_GREEN},
        {"ELECTRIC_PURPLE", named::ELECTRIC_PURPLE}
    };
    
    auto it = color_map.find(upper_name);
    if (it != color_map.end()) {
        return it->second;
    }
    
    // Return black if color name not found
    return RGB(0, 0, 0);
}

std::unordered_map<std::string, std::string> get_color_map() {
    std::unordered_map<std::string, std::string> color_map = {
        // Basic ANSI colors
        {"BLACK", ansi::FG_BLACK},
        {"RED", ansi::FG_RED},
        {"GREEN", ansi::FG_GREEN},
        {"YELLOW", ansi::FG_YELLOW},
        {"BLUE", ansi::FG_BLUE},
        {"MAGENTA", ansi::FG_MAGENTA},
        {"CYAN", ansi::FG_CYAN},
        {"WHITE", ansi::FG_WHITE},
        {"BLACK_BRIGHT", ansi::FG_BRIGHT_BLACK},
        {"RED_BRIGHT", ansi::FG_BRIGHT_RED},
        {"GREEN_BRIGHT", ansi::FG_BRIGHT_GREEN},
        {"YELLOW_BRIGHT", ansi::FG_BRIGHT_YELLOW},
        {"BLUE_BRIGHT", ansi::FG_BRIGHT_BLUE},
        {"MAGENTA_BRIGHT", ansi::FG_BRIGHT_MAGENTA},
        {"CYAN_BRIGHT", ansi::FG_BRIGHT_CYAN},
        {"WHITE_BRIGHT", ansi::FG_BRIGHT_WHITE},
        
        // Pastel color palette
        {"PASTEL_BLUE", "\033[38;5;117m"},
        {"PASTEL_PEACH", "\033[38;5;222m"},
        {"PASTEL_CYAN", "\033[38;5;159m"},
        {"PASTEL_MINT", "\033[38;5;122m"},
        {"PASTEL_LAVENDER", "\033[38;5;183m"},
        {"PASTEL_CORAL", "\033[38;5;203m"},
        
        // Named colors from colors.h
        {"ALICE_BLUE", fg_color(named::ALICE_BLUE)},
        {"ANTIQUE_WHITE", fg_color(named::ANTIQUE_WHITE)},
        {"AQUA", fg_color(named::AQUA)},
        {"AQUAMARINE", fg_color(named::AQUAMARINE)},
        {"AZURE", fg_color(named::AZURE)},
        {"BEIGE", fg_color(named::BEIGE)},
        {"BISQUE", fg_color(named::BISQUE)},
        {"BLANCHED_ALMOND", fg_color(named::BLANCHED_ALMOND)},
        {"BLUE_VIOLET", fg_color(named::BLUE_VIOLET)},
        {"BROWN", fg_color(named::BROWN)},
        {"BURLYWOOD", fg_color(named::BURLYWOOD)},
        {"CADET_BLUE", fg_color(named::CADET_BLUE)},
        {"CHARTREUSE", fg_color(named::CHARTREUSE)},
        {"CHOCOLATE", fg_color(named::CHOCOLATE)},
        {"CORAL", fg_color(named::CORAL)},
        {"CORNFLOWER_BLUE", fg_color(named::CORNFLOWER_BLUE)},
        {"CORNSILK", fg_color(named::CORNSILK)},
        {"CRIMSON", fg_color(named::CRIMSON)},
        {"DARK_BLUE", fg_color(named::DARK_BLUE)},
        {"DARK_CYAN", fg_color(named::DARK_CYAN)},
        {"DARK_GOLDENROD", fg_color(named::DARK_GOLDENROD)},
        {"DARK_GRAY", fg_color(named::DARK_GRAY)},
        {"DARK_GREEN", fg_color(named::DARK_GREEN)},
        {"DARK_KHAKI", fg_color(named::DARK_KHAKI)},
        {"DARK_MAGENTA", fg_color(named::DARK_MAGENTA)},
        {"DARK_OLIVE_GREEN", fg_color(named::DARK_OLIVE_GREEN)},
        {"DARK_ORANGE", fg_color(named::DARK_ORANGE)},
        {"DARK_ORCHID", fg_color(named::DARK_ORCHID)},
        {"DARK_RED", fg_color(named::DARK_RED)},
        {"DARK_SALMON", fg_color(named::DARK_SALMON)},
        {"DARK_SEA_GREEN", fg_color(named::DARK_SEA_GREEN)},
        {"DARK_SLATE_BLUE", fg_color(named::DARK_SLATE_BLUE)},
        {"DARK_SLATE_GRAY", fg_color(named::DARK_SLATE_GRAY)},
        {"DARK_TURQUOISE", fg_color(named::DARK_TURQUOISE)},
        {"DARK_VIOLET", fg_color(named::DARK_VIOLET)},
        {"DEEP_PINK", fg_color(named::DEEP_PINK)},
        {"DEEP_SKY_BLUE", fg_color(named::DEEP_SKY_BLUE)},
        {"DIM_GRAY", fg_color(named::DIM_GRAY)},
        {"DODGER_BLUE", fg_color(named::DODGER_BLUE)},
        {"FIREBRICK", fg_color(named::FIREBRICK)},
        {"FOREST_GREEN", fg_color(named::FOREST_GREEN)},
        {"GAINSBORO", fg_color(named::GAINSBORO)},
        {"GOLD", fg_color(named::GOLD)},
        {"GOLDENROD", fg_color(named::GOLDENROD)},
        {"GRAY", fg_color(named::GRAY)},
        {"HOT_PINK", fg_color(named::HOT_PINK)},
        {"INDIAN_RED", fg_color(named::INDIAN_RED)},
        {"INDIGO", fg_color(named::INDIGO)},
        {"IVORY", fg_color(named::IVORY)},
        {"KHAKI", fg_color(named::KHAKI)},
        {"LAVENDER", fg_color(named::LAVENDER)},
        {"LAVENDER_BLUSH", fg_color(named::LAVENDER_BLUSH)},
        {"LAWN_GREEN", fg_color(named::LAWN_GREEN)},
        {"LEMON_CHIFFON", fg_color(named::LEMON_CHIFFON)},
        {"LIGHT_BLUE", fg_color(named::LIGHT_BLUE)},
        {"LIGHT_CORAL", fg_color(named::LIGHT_CORAL)},
        {"LIGHT_CYAN", fg_color(named::LIGHT_CYAN)},
        {"LIGHT_GOLDENROD", fg_color(named::LIGHT_GOLDENROD)},
        {"LIGHT_GRAY", fg_color(named::LIGHT_GRAY)},
        {"LIGHT_GREEN", fg_color(named::LIGHT_GREEN)},
        {"LIGHT_PINK", fg_color(named::LIGHT_PINK)},
        {"LIGHT_SALMON", fg_color(named::LIGHT_SALMON)},
        {"LIGHT_SEA_GREEN", fg_color(named::LIGHT_SEA_GREEN)},
        {"LIGHT_SKY_BLUE", fg_color(named::LIGHT_SKY_BLUE)},
        {"LIGHT_SLATE_GRAY", fg_color(named::LIGHT_SLATE_GRAY)},
        {"LIGHT_STEEL_BLUE", fg_color(named::LIGHT_STEEL_BLUE)},
        {"LIGHT_YELLOW", fg_color(named::LIGHT_YELLOW)},
        {"LIME", fg_color(named::LIME)},
        {"LIME_GREEN", fg_color(named::LIME_GREEN)},
        {"LINEN", fg_color(named::LINEN)},
        {"MAROON", fg_color(named::MAROON)},
        {"MEDIUM_AQUAMARINE", fg_color(named::MEDIUM_AQUAMARINE)},
        {"MEDIUM_BLUE", fg_color(named::MEDIUM_BLUE)},
        {"MEDIUM_ORCHID", fg_color(named::MEDIUM_ORCHID)},
        {"MEDIUM_PURPLE", fg_color(named::MEDIUM_PURPLE)},
        {"MEDIUM_SEA_GREEN", fg_color(named::MEDIUM_SEA_GREEN)},
        {"MEDIUM_SLATE_BLUE", fg_color(named::MEDIUM_SLATE_BLUE)},
        {"MEDIUM_SPRING_GREEN", fg_color(named::MEDIUM_SPRING_GREEN)},
        {"MEDIUM_TURQUOISE", fg_color(named::MEDIUM_TURQUOISE)},
        {"MEDIUM_VIOLET_RED", fg_color(named::MEDIUM_VIOLET_RED)},
        {"MIDNIGHT_BLUE", fg_color(named::MIDNIGHT_BLUE)},
        {"MINT_CREAM", fg_color(named::MINT_CREAM)},
        {"MISTY_ROSE", fg_color(named::MISTY_ROSE)},
        {"MOCCASIN", fg_color(named::MOCCASIN)},
        {"NAVAJO_WHITE", fg_color(named::NAVAJO_WHITE)},
        {"NAVY", fg_color(named::NAVY)},
        {"OLD_LACE", fg_color(named::OLD_LACE)},
        {"OLIVE", fg_color(named::OLIVE)},
        {"OLIVE_DRAB", fg_color(named::OLIVE_DRAB)},
        {"ORANGE", fg_color(named::ORANGE)},
        {"ORANGE_RED", fg_color(named::ORANGE_RED)},
        {"ORCHID", fg_color(named::ORCHID)},
        {"PALE_GOLDENROD", fg_color(named::PALE_GOLDENROD)},
        {"PALE_GREEN", fg_color(named::PALE_GREEN)},
        {"PALE_TURQUOISE", fg_color(named::PALE_TURQUOISE)},
        {"PALE_VIOLET_RED", fg_color(named::PALE_VIOLET_RED)},
        {"PAPAYA_WHIP", fg_color(named::PAPAYA_WHIP)},
        {"PEACH_PUFF", fg_color(named::PEACH_PUFF)},
        {"PERU", fg_color(named::PERU)},
        {"PINK", fg_color(named::PINK)},
        {"PLUM", fg_color(named::PLUM)},
        {"POWDER_BLUE", fg_color(named::POWDER_BLUE)},
        {"PURPLE", fg_color(named::PURPLE)},
        {"REBECCA_PURPLE", fg_color(named::REBECCA_PURPLE)},
        {"ROSY_BROWN", fg_color(named::ROSY_BROWN)},
        {"ROYAL_BLUE", fg_color(named::ROYAL_BLUE)},
        {"SADDLE_BROWN", fg_color(named::SADDLE_BROWN)},
        {"SALMON", fg_color(named::SALMON)},
        {"SANDY_BROWN", fg_color(named::SANDY_BROWN)},
        {"SEA_GREEN", fg_color(named::SEA_GREEN)},
        {"SEASHELL", fg_color(named::SEASHELL)},
        {"SIENNA", fg_color(named::SIENNA)},
        {"SILVER", fg_color(named::SILVER)},
        {"SKY_BLUE", fg_color(named::SKY_BLUE)},
        {"SLATE_BLUE", fg_color(named::SLATE_BLUE)},
        {"SLATE_GRAY", fg_color(named::SLATE_GRAY)},
        {"SNOW", fg_color(named::SNOW)},
        {"SPRING_GREEN", fg_color(named::SPRING_GREEN)},
        {"STEEL_BLUE", fg_color(named::STEEL_BLUE)},
        {"TAN", fg_color(named::TAN)},
        {"TEAL", fg_color(named::TEAL)},
        {"THISTLE", fg_color(named::THISTLE)},
        {"TOMATO", fg_color(named::TOMATO)},
        {"TURQUOISE", fg_color(named::TURQUOISE)},
        {"VIOLET", fg_color(named::VIOLET)},
        {"WHEAT", fg_color(named::WHEAT)},
        {"WHITE_SMOKE", fg_color(named::WHITE_SMOKE)},
        {"YELLOW_GREEN", fg_color(named::YELLOW_GREEN)},
        {"ELECTRIC_PURPLE", fg_color(named::ELECTRIC_PURPLE)},

        // Style tags
        {"BOLD", ansi::BOLD},
        {"ITALIC", ansi::ITALIC},
        {"UNDERLINE", ansi::UNDERLINE},
        {"BLINK", ansi::BLINK},
        {"REVERSE", ansi::REVERSE},
        {"HIDDEN", ansi::HIDDEN},
        {"RESET", ansi::RESET}
    };
    
    return color_map;
}
} // namespace colors
