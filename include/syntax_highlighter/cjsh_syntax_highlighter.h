#pragma once

#include <string>

#include "isocline/isocline.h"

class SyntaxHighlighter {
   public:
    static void refresh_executables_cache();
    static void highlight(ic_highlight_env_t* henv, const char* input, void* arg);
    static void initialize_syntax_highlighting();
};
