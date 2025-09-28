#pragma once

#include <string>
#include <unordered_map>
#include <vector>

int style_def_command(const std::vector<std::string>& args);

// Style management functions
void load_custom_styles_from_config();
void apply_custom_style(const std::string& token_type,
                        const std::string& style);
const std::unordered_map<std::string, std::string>& get_custom_styles();
void reset_to_default_styles();