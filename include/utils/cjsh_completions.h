#pragma once

#include <string>

#include "isocline/isocline.h"

extern std::string g_previous_directory;
extern bool g_completion_case_sensitive;

void cjsh_command_completer(ic_completion_env_t* cenv, const char* prefix);
void cjsh_history_completer(ic_completion_env_t* cenv, const char* prefix);
void cjsh_filename_completer(ic_completion_env_t* cenv, const char* prefix);
void cjsh_default_completer(ic_completion_env_t* cenv, const char* prefix);
void initialize_completion_system();
void refresh_cached_executables();
void update_completion_frequency(const std::string& command);
void cleanup_completion_system();
void update_previous_directory(const std::string& old_dir);
void set_completion_case_sensitive(bool case_sensitive);
bool is_completion_case_sensitive();
