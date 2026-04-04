/*
  isocline_readline.c

  This file is part of isocline

  MIT License

  Copyright (c) 2026 Caden Finley
  Copyright (c) 2021 Daan Leijen
  Largely modified for CJ's Shell

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

/* ----------------------------------------------------------------------------
    Readline front-end extracted from the original isocline.c.
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "env.h"
#include "env_internal.h"
#include "isocline.h"
#include "stringbuf.h"

//-------------------------------------------------------------
// Global variables
//-------------------------------------------------------------

static bool getline_interrupt = false;

static void restore_heredoc_env(ic_env_t* env, bool singleline_only, char multiline_eol,
                                bool prompt_cleanup, bool prompt_cleanup_add_empty_line,
                                bool prompt_cleanup_truncate_multiline,
                                size_t prompt_cleanup_extra_lines) {
    if (env == NULL)
        return;
    env->singleline_only = singleline_only;
    env->multiline_eol = multiline_eol;
    env->prompt_cleanup = prompt_cleanup;
    env->prompt_cleanup_add_empty_line = prompt_cleanup_add_empty_line;
    env->prompt_cleanup_truncate_multiline = prompt_cleanup_truncate_multiline;
    env->prompt_cleanup_extra_lines = prompt_cleanup_extra_lines;
}

//-------------------------------------------------------------
// Fallback getline implementation
//-------------------------------------------------------------

static char* ic_getline(alloc_t* mem) {
    getline_interrupt = false;
    stringbuf_t* sb = sbuf_new(mem);
    int c;
    int last_char = 0;
    while (true) {
        c = fgetc(stdin);
        if (c == EOF || c == '\n') {
            last_char = c;
            break;
        } else {
            sbuf_append_char(sb, (char)c);
        }
        if (getline_interrupt) {
            last_char = 0;
            break;
        }
    }
    if (getline_interrupt && sbuf_len(sb) == 0) {
        sbuf_free(sb);
        return mem_strdup(mem, IC_READLINE_TOKEN_CTRL_C);
    }
    if (last_char == EOF && sbuf_len(sb) == 0) {
        sbuf_free(sb);
        return mem_strdup(mem, IC_READLINE_TOKEN_CTRL_D);
    }
    return sbuf_free_dup(sb);
}

//-------------------------------------------------------------
// Public API
//-------------------------------------------------------------

ic_public char* ic_readline(const char* prompt_text, const char* inline_right_text,
                            const char* initial_input) {
    ic_env_t* env = ic_get_env();
    if (env == NULL) {
        return NULL;
    }

    if (!env->noedit) {
        if (initial_input != NULL) {
            ic_env_set_initial_input(env, initial_input);
        }

        char* result = ic_editline(env, prompt_text, inline_right_text);

        ic_env_clear_initial_input(env);
        return result;
    }

    if (env->tty != NULL && env->term != NULL) {
        term_start_raw(env->term);
        if (prompt_text != NULL) {
            term_write(env->term, prompt_text);
        }
        term_write(env->term, env->prompt_marker);
        term_end_raw(env->term, false);
    }

    return ic_getline(env->mem);
}

ic_public bool ic_async_stop(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return false;
    if (env->tty == NULL)
        return false;
    return tty_async_stop(env->tty);
}

ic_public bool ic_async_interrupt_getline(void) {
    getline_interrupt = true;
    return true;
}

ic_public void ic_print_prompt(const char* prompt_text, bool continuation_line) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || env->term == NULL || env->bbcode == NULL) {
        return;
    }

    term_start_raw(env->term);

    const char* text = (prompt_text != NULL ? prompt_text : "");

    bbcode_style_open(env->bbcode, "ic-prompt");

    if (!continuation_line) {
        bbcode_print(env->bbcode, text);
    } else {
        ic_emit_continuation_indent(env, text);
    }

    bbcode_print(env->bbcode, (continuation_line ? env->cprompt_marker : env->prompt_marker));

    bbcode_style_close(env->bbcode, NULL);
    term_flush(env->term);
}

ic_public char* ic_readline_ex(const char* prompt_text, ic_completer_fun_t* completer,
                               void* completer_arg, ic_highlight_fun_t* highlighter,
                               void* highlighter_arg) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return NULL;
    ic_completer_fun_t* prev_completer;
    void* prev_completer_arg;
    completions_get_completer(env->completions, &prev_completer, &prev_completer_arg);
    ic_highlight_fun_t* prev_highlighter = env->highlighter;
    void* prev_highlighter_arg = env->highlighter_arg;
    if (completer != NULL) {
        ic_set_default_completer(completer, completer_arg);
    }
    if (highlighter != NULL) {
        ic_set_default_highlighter(highlighter, highlighter_arg);
    }
    char* res = ic_readline(prompt_text, NULL, "");
    ic_set_default_completer(prev_completer, prev_completer_arg);
    ic_set_default_highlighter(prev_highlighter, prev_highlighter_arg);
    return res;
}

ic_public char* ic_read_heredoc(const char* delimiter, bool strip_tabs) {
    ic_env_t* env = ic_get_env();
    if (env == NULL || delimiter == NULL)
        return NULL;

    const bool prev_singleline_only = env->singleline_only;
    const char prev_multiline_eol = env->multiline_eol;
    const bool prev_prompt_cleanup = env->prompt_cleanup;
    const bool prev_prompt_cleanup_add_empty_line = env->prompt_cleanup_add_empty_line;
    const bool prev_prompt_cleanup_truncate_multiline = env->prompt_cleanup_truncate_multiline;
    const size_t prev_prompt_cleanup_extra_lines = env->prompt_cleanup_extra_lines;

    env->singleline_only = true;
    env->multiline_eol = 0;
    env->prompt_cleanup = false;
    env->prompt_cleanup_add_empty_line = false;
    env->prompt_cleanup_truncate_multiline = false;
    env->prompt_cleanup_extra_lines = 0;

    stringbuf_t* content = sbuf_new(env->mem);
    if (content == NULL) {
        restore_heredoc_env(env, prev_singleline_only, prev_multiline_eol, prev_prompt_cleanup,
                            prev_prompt_cleanup_add_empty_line,
                            prev_prompt_cleanup_truncate_multiline,
                            prev_prompt_cleanup_extra_lines);
        return NULL;
    }

    size_t line_number = 1;
    const size_t delimiter_len = strlen(delimiter);
    char* result = NULL;

    // Read lines until we encounter the delimiter
    while (true) {
        // Build prompt with line number for heredoc lines
        char prompt[32];
        snprintf(prompt, sizeof(prompt), "%3zu > ", line_number);

        char* line = ic_readline(prompt, NULL, NULL);

        // Check for cancellation (Ctrl-C or Ctrl-D)
        if (line == NULL || strcmp(line, IC_READLINE_TOKEN_CTRL_C) == 0 ||
            strcmp(line, IC_READLINE_TOKEN_CTRL_D) == 0) {
            if (line != NULL) {
                ic_free(line);
            }
            sbuf_free(content);
            restore_heredoc_env(env, prev_singleline_only, prev_multiline_eol, prev_prompt_cleanup,
                                prev_prompt_cleanup_add_empty_line,
                                prev_prompt_cleanup_truncate_multiline,
                                prev_prompt_cleanup_extra_lines);
            return NULL;
        }

        // Check if this line is the delimiter
        const char* check_line = line;

        // If strip_tabs is enabled (<<- syntax), skip leading tabs
        if (strip_tabs) {
            while (*check_line == '\t') {
                check_line++;
            }
        }

        // Trim trailing whitespace from delimiter check
        size_t len = strlen(check_line);
        while (len > 0 && (check_line[len - 1] == ' ' || check_line[len - 1] == '\t' ||
                           check_line[len - 1] == '\r' || check_line[len - 1] == '\n')) {
            len--;
        }

        // Check if trimmed line matches delimiter
        if (len == delimiter_len && strncmp(check_line, delimiter, len) == 0) {
            // Found delimiter, we're done
            ic_free(line);
            break;
        }

        // Not the delimiter, add line to content
        // If strip_tabs, use the tab-stripped version
        if (strip_tabs) {
            sbuf_append(content, check_line);
        } else {
            sbuf_append(content, line);
        }
        sbuf_append_char(content, '\n');

        ic_free(line);
        line_number++;
    }

    result = sbuf_free_dup(content);

    restore_heredoc_env(env, prev_singleline_only, prev_multiline_eol, prev_prompt_cleanup,
                        prev_prompt_cleanup_add_empty_line, prev_prompt_cleanup_truncate_multiline,
                        prev_prompt_cleanup_extra_lines);

    return result;
}
