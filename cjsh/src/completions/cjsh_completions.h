/*
  cjsh_completions.h

  This file is part of cjsh, CJ's Shell

  MIT License

  Copyright (c) 2026 Caden Finley

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#pragma once

#include <string>

#include "isocline/isocline.h"

void cjsh_command_completer(ic_completion_env_t* cenv, const char* prefix);
void cjsh_history_completer(ic_completion_env_t* cenv, const char* prefix);
void cjsh_filename_completer(ic_completion_env_t* cenv, const char* prefix);
void cjsh_default_completer(ic_completion_env_t* cenv, const char* prefix);
void initialize_completion_system();
void set_completion_case_sensitive(bool case_sensitive);
bool is_completion_case_sensitive();
void set_completion_spell_correction_enabled(bool enabled);
bool is_completion_spell_correction_enabled();
bool set_completion_max_results(long max_results, std::string* error_message = nullptr);
long get_completion_max_results();
long get_completion_default_max_results();
long get_completion_min_allowed_results();
bool set_history_max_entries(long max_entries, std::string* error_message = nullptr);
long get_history_max_entries();
long get_history_default_history_limit();
long get_history_min_history_limit();
