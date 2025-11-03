#pragma once

#include <string>

#include "isocline/isocline.h"

extern bool g_completion_case_sensitive;
void cjsh_command_completer(ic_completion_env_t* cenv, const char* prefix);
void cjsh_history_completer(ic_completion_env_t* cenv, const char* prefix);
void cjsh_filename_completer(ic_completion_env_t* cenv, const char* prefix);
void cjsh_default_completer(ic_completion_env_t* cenv, const char* prefix);
void initialize_completion_system();
void update_completion_frequency(const std::string& command);
void cleanup_completion_system();
void set_completion_case_sensitive(bool case_sensitive);
bool is_completion_case_sensitive();
void set_completion_spell_correction_enabled(bool enabled);
bool is_completion_spell_correction_enabled();
bool set_history_max_entries(long max_entries, std::string* error_message = nullptr);
long get_history_max_entries();
long get_history_default_history_limit();
long get_history_min_history_limit();
long get_history_max_history_limit();
