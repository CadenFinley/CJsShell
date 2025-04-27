#include "colors.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace colors {

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
    ss << "\033[38;2;" << static_cast<int>(color.r) << ";" 
       << static_cast<int>(color.g) << ";" 
       << static_cast<int>(color.b) << "m";
    return ss.str();
}

std::string bg_color(const RGB& color) {
    std::stringstream ss;
    ss << "\033[48;2;" << static_cast<int>(color.r) << ";" 
       << static_cast<int>(color.g) << ";" 
       << static_cast<int>(color.b) << "m";
    return ss.str();
}

std::string fg_color(uint8_t index) {
    std::stringstream ss;
    ss << "\033[38;5;" << static_cast<int>(index) << "m";
    return ss.str();
}

std::string bg_color(uint8_t index) {
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
    
    std::string result;
    size_t steps = text.length();
    std::vector<RGB> colors = gradient(start, end, steps);
    
    for (size_t i = 0; i < steps; ++i) {
        result += fg_color(colors[i]) + text.substr(i, 1);
    }
    
    result += ansi::RESET;
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
    if (index >= 232 && index <= 255) {
        int gray = (index - 232) * 10 + 8;
        return RGB(gray, gray, gray);
    }
    
    // Default fallback
    return RGB(0, 0, 0);
}

} // namespace colors
