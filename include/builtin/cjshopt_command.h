#pragma once

#include <string>
#include <unordered_map>
#include <vector>

int cjshopt_command(const std::vector<std::string>& args);

int startup_flag_command(const std::vector<std::string>& args);
int completion_case_command(const std::vector<std::string>& args);
int completion_spell_command(const std::vector<std::string>& args);
int style_def_command(const std::vector<std::string>& args);

void reset_to_default_styles();
void load_custom_styles_from_config();
void apply_custom_style(const std::string& token_type, const std::string& style);
const std::unordered_map<std::string, std::string>& get_custom_styles();
int generate_profile_command(const std::vector<std::string>& args);
int generate_rc_command(const std::vector<std::string>& args);
int generate_logout_command(const std::vector<std::string>& args);
int keybind_command(const std::vector<std::string>& args);
int set_max_bookmarks_command(const std::vector<std::string>& args);
int set_history_max_command(const std::vector<std::string>& args);
int bookmark_blacklist_command(const std::vector<std::string>& args);