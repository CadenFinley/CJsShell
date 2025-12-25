#pragma once

#include "isocline/isocline.h"

class SyntaxHighlighter {
   public:
    static void highlight(ic_highlight_env_t* henv, const char* input, void* arg);
    static void initialize_syntax_highlighting();
};
