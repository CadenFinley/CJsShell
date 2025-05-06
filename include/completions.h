#pragma once

#include "../vendor/isocline/include/isocline.h"

void cjsh_command_completer(ic_completion_env_t* cenv, const char* prefix);
void cjsh_history_completer(ic_completion_env_t* cenv, const char* prefix);
void cjsh_filename_completer(ic_completion_env_t* cenv, const char* prefix);
void cjsh_default_completer(ic_completion_env_t* cenv, const char* prefix);
void initialize_completion_system();
