#pragma once

#include <string>
#include <unordered_map>
#include <vector>

using ic_keycode_t = unsigned int;
extern std::unordered_map<ic_keycode_t, std::string> g_custom_keybindings;

int cjshopt_command(const std::vector<std::string>& args);
int keybind_ext_command(const std::vector<std::string>& args);
int startup_flag_command(const std::vector<std::string>& args);
int completion_case_command(const std::vector<std::string>& args);
int completion_spell_command(const std::vector<std::string>& args);
int completion_learning_command(const std::vector<std::string>& args);
int line_numbers_command(const std::vector<std::string>& args);
int line_numbers_continuation_command(const std::vector<std::string>& args);
int line_numbers_replace_prompt_command(const std::vector<std::string>& args);
int current_line_number_highlight_command(const std::vector<std::string>& args);
int hint_delay_command(const std::vector<std::string>& args);
int completion_preview_command(const std::vector<std::string>& args);
int visible_whitespace_command(const std::vector<std::string>& args);
int hint_command(const std::vector<std::string>& args);
int multiline_indent_command(const std::vector<std::string>& args);
int multiline_command(const std::vector<std::string>& args);
int multiline_start_lines_command(const std::vector<std::string>& args);
int inline_help_command(const std::vector<std::string>& args);
int status_hints_command(const std::vector<std::string>& args);
int auto_tab_command(const std::vector<std::string>& args);
int prompt_newline_command(const std::vector<std::string>& args);
int prompt_cleanup_command(const std::vector<std::string>& args);
int prompt_cleanup_newline_command(const std::vector<std::string>& args);
int prompt_cleanup_empty_line_command(const std::vector<std::string>& args);
int prompt_cleanup_truncate_command(const std::vector<std::string>& args);
int right_prompt_follow_cursor_command(const std::vector<std::string>& args);
int style_def_command(const std::vector<std::string>& args);
void apply_custom_style(const std::string& token_type, const std::string& style);
int generate_profile_command(const std::vector<std::string>& args);
int generate_rc_command(const std::vector<std::string>& args);
int generate_logout_command(const std::vector<std::string>& args);
int keybind_command(const std::vector<std::string>& args);
int set_history_max_command(const std::vector<std::string>& args);
int set_completion_max_command(const std::vector<std::string>& args);
std::string get_custom_keybinding(ic_keycode_t key);
bool has_custom_keybinding(ic_keycode_t key);
void set_custom_keybinding(ic_keycode_t key, const std::string& command);
void clear_custom_keybinding(ic_keycode_t key);
void clear_all_custom_keybindings();
