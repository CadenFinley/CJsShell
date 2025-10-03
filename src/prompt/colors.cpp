#include "colors.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace colors {

ColorCapability g_color_capability = ColorCapability::BASIC_COLOR;
static std::unordered_map<std::string, RGB> g_custom_colors;

ColorCapability detect_color_capability() {
    const char* colorterm = std::getenv("COLORTERM");
    const char* term = std::getenv("TERM");
    const char* no_color = std::getenv("NO_COLOR");
    const char* force_color = std::getenv("FORCE_COLOR");

    if (no_color && no_color[0] != '\0') {
        return ColorCapability::NO_COLOR;
    }

    if (force_color && std::string(force_color) == "true") {
        return ColorCapability::TRUE_COLOR;
    }
    if (colorterm) {
        std::string colortermStr = colorterm;
        std::transform(colortermStr.begin(), colortermStr.end(), colortermStr.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (colortermStr.find("truecolor") != std::string::npos ||
            colortermStr.find("24bit") != std::string::npos) {
            return ColorCapability::TRUE_COLOR;
        }
    }

    if (term) {
        std::string termStr = term;
        if (termStr.find("256") != std::string::npos ||
            termStr.find("xterm") != std::string::npos) {
            return ColorCapability::XTERM_256_COLOR;
        }
    }

    return ColorCapability::BASIC_COLOR;
}

void initialize_color_support(bool enabled) {
    if (!enabled) {
        g_color_capability = ColorCapability::NO_COLOR;
        return;
    }

    g_color_capability = detect_color_capability();
}

std::string get_color_capability_string(ColorCapability capability) {
    switch (capability) {
        case ColorCapability::NO_COLOR:
            return "No Color";
        case ColorCapability::BASIC_COLOR:
            return "Basic ANSI Colors (16 colors)";
        case ColorCapability::XTERM_256_COLOR:
            return "256 Colors";
        case ColorCapability::TRUE_COLOR:
            return "True Color (24-bit RGB)";
        default:
            return "Unknown";
    }
}

uint8_t get_closest_ansi_color(const RGB& color) {
    const std::array<RGB, 16> basic_colors = {{{0, 0, 0},
                                               {170, 0, 0},
                                               {0, 170, 0},
                                               {170, 85, 0},
                                               {0, 0, 170},
                                               {170, 0, 170},
                                               {0, 170, 170},
                                               {170, 170, 170},
                                               {85, 85, 85},
                                               {255, 85, 85},
                                               {85, 255, 85},
                                               {255, 255, 85},
                                               {85, 85, 255},
                                               {255, 85, 255},
                                               {85, 255, 255},
                                               {255, 255, 255}}};

    uint8_t closest_index = 0;
    int closest_distance = INT_MAX;

    for (size_t i = 0; i < basic_colors.size(); i++) {
        const RGB& c = basic_colors[i];
        int distance = (c.r - color.r) * (c.r - color.r) + (c.g - color.g) * (c.g - color.g) +
                       (c.b - color.b) * (c.b - color.b);

        if (distance < closest_distance) {
            closest_distance = distance;
            closest_index = i;
        }
    }

    return closest_index;
}

constexpr HSL rgb_to_hsl(const RGB& rgb) {
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

constexpr RGB hsl_to_rgb(const HSL& hsl) {
    float h = hsl.h / 360.0f;
    float s = hsl.s;
    float l = hsl.l;

    if (s == 0) {
        int gray = static_cast<int>(l * 255);
        return RGB(gray, gray, gray);
    }

    auto hue_to_rgb = [](float p, float q, float t) {
        if (t < 0)
            t += 1;
        if (t > 1)
            t -= 1;
        if (t < 1.0f / 6.0f)
            return p + (q - p) * 6.0f * t;
        if (t < 1.0f / 2.0f)
            return q;
        if (t < 2.0f / 3.0f)
            return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
        return p;
    };

    float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
    float p = 2 * l - q;

    float r = hue_to_rgb(p, q, h + 1.0f / 3.0f);
    float g = hue_to_rgb(p, q, h);
    float b = hue_to_rgb(p, q, h - 1.0f / 3.0f);

    return RGB(static_cast<uint8_t>(r * 255), static_cast<uint8_t>(g * 255),
               static_cast<uint8_t>(b * 255));
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
            ss << "\033[38;2;" << static_cast<int>(color.r) << ";" << static_cast<int>(color.g)
               << ";" << static_cast<int>(color.b) << "m";
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
            ss << "\033[48;2;" << static_cast<int>(color.r) << ";" << static_cast<int>(color.g)
               << ";" << static_cast<int>(color.b) << "m";
            break;
    }

    return ss.str();
}

std::string fg_color(uint8_t index) {
    if (g_color_capability == ColorCapability::NO_COLOR) {
        return "";
    }

    if (g_color_capability == ColorCapability::BASIC_COLOR && index >= 16) {
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
        return bg_color(xterm256_to_rgb(index));
    }

    std::stringstream ss;
    ss << "\033[48;5;" << static_cast<int>(index) << "m";
    return ss.str();
}

std::string style(const std::string& text, const RGB& fg) {
    return fg_color(fg) + text + std::string(ansi::RESET);
}

std::string style(const std::string& text, const RGB& fg, const RGB& bg) {
    return fg_color(fg) + bg_color(bg) + text + std::string(ansi::RESET);
}

std::string style_bold(const std::string& text) {
    return std::string(ansi::BOLD) + text + std::string(ansi::RESET);
}

std::string style_italic(const std::string& text) {
    return std::string(ansi::ITALIC) + text + std::string(ansi::RESET);
}

std::string style_underline(const std::string& text) {
    return std::string(ansi::UNDERLINE) + text + std::string(ansi::RESET);
}

std::string style_blink(const std::string& text) {
    return std::string(ansi::BLINK) + text + std::string(ansi::RESET);
}

std::string style_reverse(const std::string& text) {
    return std::string(ansi::REVERSE) + text + std::string(ansi::RESET);
}

std::string style_hidden(const std::string& text) {
    return std::string(ansi::HIDDEN) + text + std::string(ansi::RESET);
}

std::string style_reset() {
    return std::string(ansi::RESET);
}

RGB blend(const RGB& color1, const RGB& color2, float factor) {
    return RGB(static_cast<uint8_t>(color1.r * (1 - factor) + color2.r * factor),
               static_cast<uint8_t>(color1.g * (1 - factor) + color2.g * factor),
               static_cast<uint8_t>(color1.b * (1 - factor) + color2.b * factor));
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
    if (text.empty())
        return "";
    if (g_color_capability == ColorCapability::NO_COLOR)
        return text;

    std::string result;
    size_t steps = text.length();

    if (steps == 1) {
        return fg_color(start) + text +
               (g_color_capability != ColorCapability::NO_COLOR ? ansi::RESET : "");
    }

    if (g_color_capability == ColorCapability::BASIC_COLOR) {
        size_t halfway = steps / 2;
        for (size_t i = 0; i < steps; ++i) {
            if (i < halfway) {
                result += fg_color(start) + text.substr(i, 1);
            } else {
                result += fg_color(end) + text.substr(i, 1);
            }
        }
    } else {
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

std::string gradient_fg(const std::string& text, const GradientSpec& spec) {
    return gradient_text(text, spec.start, spec.end);
}

std::string gradient_bg(const std::string& text, const GradientSpec& spec) {
    if (text.empty())
        return "";
    if (g_color_capability == ColorCapability::NO_COLOR)
        return text;

    std::string result;
    size_t steps = text.length();

    if (steps == 1) {
        return bg_color(spec.start) + text +
               (g_color_capability != ColorCapability::NO_COLOR ? ansi::BG_RESET : "");
    }

    if (g_color_capability == ColorCapability::BASIC_COLOR) {
        size_t halfway = steps / 2;
        for (size_t i = 0; i < steps; ++i) {
            if (i < halfway) {
                result += bg_color(spec.start) + text.substr(i, 1);
            } else {
                result += bg_color(spec.end) + text.substr(i, 1);
            }
        }
    } else {
        std::vector<RGB> colors = gradient(spec.start, spec.end, steps);
        for (size_t i = 0; i < steps; ++i) {
            result += bg_color(colors[i]) + text.substr(i, 1);
        }
    }

    if (g_color_capability != ColorCapability::NO_COLOR) {
        result += ansi::BG_RESET;
    }
    return result;
}

std::string gradient_bg_with_fg(const std::string& text, const GradientSpec& bg_spec,
                                const RGB& fg_rgb) {
    if (text.empty())
        return "";
    if (g_color_capability == ColorCapability::NO_COLOR)
        return text;

    std::string result;
    size_t steps = text.length();
    std::string fg_code = fg_color(fg_rgb);

    if (steps == 1) {
        return bg_color(bg_spec.start) + fg_code + text +
               (g_color_capability != ColorCapability::NO_COLOR ? ansi::BG_RESET : "");
    }

    if (g_color_capability == ColorCapability::BASIC_COLOR) {
        size_t halfway = steps / 2;
        for (size_t i = 0; i < steps; ++i) {
            if (i < halfway) {
                result += bg_color(bg_spec.start) + fg_code + text.substr(i, 1);
            } else {
                result += bg_color(bg_spec.end) + fg_code + text.substr(i, 1);
            }
        }
    } else {
        std::vector<RGB> bg_colors = gradient(bg_spec.start, bg_spec.end, steps);
        for (size_t i = 0; i < steps; ++i) {
            result += bg_color(bg_colors[i]) + fg_code + text.substr(i, 1);
        }
    }

    if (g_color_capability != ColorCapability::NO_COLOR) {
        result += ansi::RESET;
    }
    return result;
}

std::string apply_gradient_bg_with_fg(const std::string& text, const std::string& bg_value,
                                      const std::string& fg_value) {
    if (is_gradient_value(bg_value)) {
        GradientSpec bg_spec = parse_gradient_value(bg_value);
        RGB fg_rgb = (fg_value == "RESET") ? RGB(255, 255, 255) : parse_color_value(fg_value);
        return gradient_bg_with_fg(text, bg_spec, fg_rgb);
    } else {
        return apply_color_or_gradient(text, fg_value, true);
    }
}

GradientSpec parse_gradient_value(const std::string& value) {
    std::string trimmed_value = value;
    trimmed_value.erase(0, trimmed_value.find_first_not_of(" \t\n\r\f\v"));
    trimmed_value.erase(trimmed_value.find_last_not_of(" \t\n\r\f\v") + 1);

    std::regex gradient_regex(
        "gradient\\s*\\(\\s*([^,]+)\\s*,\\s*([^,)]+)(?:\\s*,\\s*([^)]+))?\\s*"
        "\\)",
        std::regex_constants::icase);
    std::smatch gradient_match;

    if (std::regex_match(trimmed_value, gradient_match, gradient_regex)) {
        RGB start_color = parse_color_value(gradient_match[1].str());
        RGB end_color = parse_color_value(gradient_match[2].str());
        std::string direction = "horizontal";

        if (gradient_match[3].matched) {
            direction = gradient_match[3].str();

            direction.erase(0, direction.find_first_not_of(" \t\n\r\f\v"));
            direction.erase(direction.find_last_not_of(" \t\n\r\f\v") + 1);

            std::transform(direction.begin(), direction.end(), direction.begin(), ::tolower);
        }

        return GradientSpec(start_color, end_color, direction);
    }

    RGB color = parse_color_value(trimmed_value);
    return GradientSpec(color, color, "horizontal");
}

bool is_gradient_value(const std::string& value) {
    std::string trimmed_value = value;
    trimmed_value.erase(0, trimmed_value.find_first_not_of(" \t\n\r\f\v"));
    trimmed_value.erase(trimmed_value.find_last_not_of(" \t\n\r\f\v") + 1);

    std::regex gradient_check("gradient\\s*\\(", std::regex_constants::icase);
    return std::regex_search(trimmed_value, gradient_check);
}

std::string apply_color_or_gradient(const std::string& text, const std::string& color_value,
                                    bool is_foreground) {
    if (color_value == "RESET") {
        return is_foreground ? "" : colors::ansi::BG_RESET;
    }

    if (is_gradient_value(color_value)) {
        GradientSpec gradient_spec = parse_gradient_value(color_value);
        return is_foreground ? gradient_fg(text, gradient_spec) : gradient_bg(text, gradient_spec);
    } else {
        RGB color = parse_color_value(color_value);
        return is_foreground ? fg_color(color) : bg_color(color);
    }
}

uint8_t rgb_to_xterm256(const RGB& color) {
    int r = static_cast<int>(round(color.r / 255.0 * 5.0));
    int g = static_cast<int>(round(color.g / 255.0 * 5.0));
    int b = static_cast<int>(round(color.b / 255.0 * 5.0));

    return static_cast<uint8_t>(16 + 36 * r + 6 * g + b);
}

constexpr RGB xterm256_to_rgb(uint8_t index) {
    if (index < 16) {
        switch (index) {
            case 0:
                return RGB(0, 0, 0);
            case 1:
                return RGB(170, 0, 0);
            case 2:
                return RGB(0, 170, 0);
            case 3:
                return RGB(170, 85, 0);
            case 4:
                return RGB(0, 0, 170);
            case 5:
                return RGB(170, 0, 170);
            case 6:
                return RGB(0, 170, 170);
            case 7:
                return RGB(170, 170, 170);
            case 8:
                return RGB(85, 85, 85);
            case 9:
                return RGB(255, 85, 85);
            case 10:
                return RGB(85, 255, 85);
            case 11:
                return RGB(255, 255, 85);
            case 12:
                return RGB(85, 85, 255);
            case 13:
                return RGB(255, 85, 255);
            case 14:
                return RGB(85, 255, 255);
            case 15:
                return RGB(255, 255, 255);
        }
    }

    if (index >= 16 && index <= 231) {
        index -= 16;
        int r = (index / 36) * 51;
        int g = ((index % 36) / 6) * 51;
        int b = (index % 6) * 51;
        return RGB(r, g, b);
    }

    if (index >= 232) {
        int gray = (index - 232) * 10 + 8;
        return RGB(gray, gray, gray);
    }

    return RGB(0, 0, 0);
}

RGB parse_color_value(const std::string& value) {
    std::string trimmed_value = value;
    trimmed_value.erase(0, trimmed_value.find_first_not_of(" \t\n\r\f\v"));
    trimmed_value.erase(trimmed_value.find_last_not_of(" \t\n\r\f\v") + 1);

    if (trimmed_value[0] == '#') {
        std::string hex = trimmed_value.substr(1);

        if (hex.length() == 3) {
            std::string expanded;
            for (char c : hex) {
                expanded += c;
                expanded += c;
            }
            hex = expanded;
        }

        if (hex.length() == 6) {
            try {
                int r = std::stoi(hex.substr(0, 2), nullptr, 16);
                int g = std::stoi(hex.substr(2, 2), nullptr, 16);
                int b = std::stoi(hex.substr(4, 2), nullptr, 16);
                return RGB(r, g, b);
            } catch (const std::exception& e) {
                return RGB(255, 255, 255);
            }
        }
    }

    std::regex rgb_regex("rgb\\s*\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*\\)",
                         std::regex_constants::icase);
    std::smatch rgb_match;
    if (std::regex_match(trimmed_value, rgb_match, rgb_regex)) {
        try {
            int r = std::clamp(std::stoi(rgb_match[1]), 0, 255);
            int g = std::clamp(std::stoi(rgb_match[2]), 0, 255);
            int b = std::clamp(std::stoi(rgb_match[3]), 0, 255);
            return RGB(r, g, b);
        } catch (const std::exception& e) {
            return RGB(255, 255, 255);
        }
    }

    std::string upper_value = trimmed_value;
    std::transform(upper_value.begin(), upper_value.end(), upper_value.begin(), ::toupper);
    return get_color_by_name(upper_value);
}

std::unordered_map<std::string, RGB> get_custom_colors() {
    return g_custom_colors;
}

RGB get_color_by_name(const std::string& name) {
    if (g_color_capability == ColorCapability::NO_COLOR) {
        return RGB(255, 255, 255);
    }

    std::string upper_name = name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), ::toupper);

    auto custom_it = g_custom_colors.find(upper_name);
    if (custom_it != g_custom_colors.end()) {
        return custom_it->second;
    }

    for (const auto& named_color : g_basic_colors) {
        if (named_color.name == upper_name) {
            return named_color.color;
        }
    }

    return RGB(0, 0, 0);
}

std::unordered_map<std::string, std::string> get_color_map() {
    std::unordered_map<std::string, std::string> color_map;

    color_map["BOLD"] = ansi::BOLD;
    color_map["ITALIC"] = ansi::ITALIC;
    color_map["UNDERLINE"] = ansi::UNDERLINE;
    color_map["BLINK"] = ansi::BLINK;
    color_map["REVERSE"] = ansi::REVERSE;
    color_map["HIDDEN"] = ansi::HIDDEN;
    color_map["RESET"] = ansi::RESET;

    if (g_color_capability != ColorCapability::NO_COLOR) {
        for (const auto& [name, rgb] : g_custom_colors) {
            color_map[name] = fg_color(rgb);
        }
    } else {
        for (const auto& [name, rgb] : g_custom_colors) {
            color_map[name] = ansi::RESET;
        }
    }

    return color_map;
}
}  // namespace colors
