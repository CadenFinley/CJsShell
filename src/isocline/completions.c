/* ----------------------------------------------------------------------------
  Copyright (c) 2021, Daan Leijen
  Largely Modified by Caden Finley 2025 for CJ's Shell
  This is free software; you can redistribute it and/or modify it
  under the terms of the MIT License.

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
-----------------------------------------------------------------------------*/
#include "completions.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "env.h"
#include "isocline.h"
#include "stringbuf.h"

//-------------------------------------------------------------
// Completions
//-------------------------------------------------------------

typedef struct completion_s {
    const char* replacement;
    const char* display;
    const char* help;
    const char* source;
    ssize_t delete_before;
    ssize_t delete_after;
} completion_t;

struct completions_s {
    ic_completer_fun_t* completer;
    void* completer_arg;
    ssize_t completer_max;
    ssize_t count;
    ssize_t len;
    completion_t* elems;
    alloc_t* mem;
};

static void default_filename_completer(ic_completion_env_t* cenv, const char* prefix);

ic_private completions_t* completions_new(alloc_t* mem) {
    completions_t* cms = mem_zalloc_tp(mem, completions_t);
    if (cms == NULL)
        return NULL;
    cms->mem = mem;
    cms->completer = &default_filename_completer;
    return cms;
}

ic_private void completions_free(completions_t* cms) {
    if (cms == NULL)
        return;
    completions_clear(cms);
    if (cms->elems != NULL) {
        mem_free(cms->mem, cms->elems);
        cms->elems = NULL;
        cms->count = 0;
        cms->len = 0;
    }
    mem_free(cms->mem, cms);  // free ourselves
}

// Escape bbcode control characters so completion metadata cannot inject styles.
static char* completions_escape_bbcode(alloc_t* mem, const char* text) {
    if (text != NULL && *text == IC_COMPLETION_DISPLAY_TRUSTED_PREFIX) {
        return mem_strdup(mem, text + 1);
    }
    if (text == NULL)
        return NULL;
    const ssize_t len = ic_strlen(text);
    if (len <= 0)
        return mem_strdup(mem, text);

    ssize_t extra = 0;
    for (ssize_t i = 0; i < len; i++) {
        char ch = text[i];
        if (ch == '[' || ch == '\\') {
            extra++;
        }
    }

    if (extra == 0)
        return mem_strdup(mem, text);

    char* escaped = mem_malloc_tp_n(mem, char, len + extra + 1);
    if (escaped == NULL)
        return NULL;

    char* dest = escaped;
    for (ssize_t i = 0; i < len; i++) {
        unsigned char uch = (unsigned char)text[i];
        char ch = (char)uch;
        if (uch < 0x20 || uch == 0x7F) {
            ch = ' ';
        }
        if (ch == '[' || ch == '\\') {
            *dest++ = '\\';
        }
        *dest++ = ch;
    }
    *dest = '\0';
    return escaped;
}

ic_private void completions_clear(completions_t* cms) {
    while (cms->count > 0) {
        completion_t* cm = cms->elems + cms->count - 1;
        mem_free(cms->mem, cm->display);
        mem_free(cms->mem, cm->replacement);
        mem_free(cms->mem, cm->help);
        mem_free(cms->mem, cm->source);
        memset(cm, 0, sizeof(*cm));
        cms->count--;
    }
}

static bool completions_set_entry(completions_t* cms, completion_t* cm, const char* replacement,
                                  const char* display, const char* help, const char* source,
                                  ssize_t delete_before, ssize_t delete_after) {
    char* new_replacement = NULL;
    char* new_display = NULL;
    char* new_help = NULL;
    char* new_source = NULL;

    if (replacement != NULL) {
        new_replacement = mem_strdup(cms->mem, replacement);
        if (new_replacement == NULL)
            goto fail;
    }
    const char* display_text = (display != NULL ? display : replacement);
    if (display_text != NULL) {
        new_display = completions_escape_bbcode(cms->mem, display_text);
        if (new_display == NULL)
            goto fail;
    }
    if (help != NULL) {
        new_help = completions_escape_bbcode(cms->mem, help);
        if (new_help == NULL)
            goto fail;
    }
    if (source != NULL) {
        new_source = completions_escape_bbcode(cms->mem, source);
        if (new_source == NULL)
            goto fail;
    }

    mem_free(cms->mem, cm->replacement);
    mem_free(cms->mem, cm->display);
    mem_free(cms->mem, cm->help);
    mem_free(cms->mem, cm->source);

    cm->replacement = (replacement != NULL ? new_replacement : NULL);
    cm->display = new_display;
    cm->help = (help != NULL ? new_help : NULL);
    cm->source = (source != NULL ? new_source : NULL);
    cm->delete_before = delete_before;
    cm->delete_after = delete_after;
    return true;

fail:
    mem_free(cms->mem, new_replacement);
    mem_free(cms->mem, new_display);
    mem_free(cms->mem, new_help);
    mem_free(cms->mem, new_source);
    return false;
}

static bool completions_push(completions_t* cms, const char* replacement, const char* display,
                             const char* help, const char* source, ssize_t delete_before,
                             ssize_t delete_after) {
    if (cms->count >= cms->len) {
        ssize_t newlen = (cms->len <= 0 ? 32 : cms->len * 2);
        completion_t* newelems = mem_realloc_tp(cms->mem, completion_t, cms->elems, newlen);
        if (newelems == NULL)
            return false;
        cms->elems = newelems;
        cms->len = newlen;
    }
    assert(cms->count < cms->len);
    completion_t* cm = cms->elems + cms->count;
    memset(cm, 0, sizeof(*cm));
    if (!completions_set_entry(cms, cm, replacement, display, help, source, delete_before,
                               delete_after)) {
        memset(cm, 0, sizeof(*cm));
        return false;
    }
    cms->count++;
    return true;
}

ic_private ssize_t completions_count(completions_t* cms) {
    return cms->count;
}
ic_private bool completions_add(completions_t* cms, const char* replacement, const char* display,
                                const char* help, const char* source, ssize_t delete_before,
                                ssize_t delete_after) {
    if (cms->completer_max <= 0)
        return false;

    cms->completer_max--;

    if (replacement != NULL) {
        for (ssize_t i = 0; i < cms->count; i++) {
            const completion_t* existing = cms->elems + i;
            if (existing->replacement != NULL && strcmp(replacement, existing->replacement) == 0) {
                return true;
            }
        }
    }

    if (!completions_push(cms, replacement, display, help, source, delete_before, delete_after)) {
        cms->completer_max++;
        return false;
    }
    return true;
}

static completion_t* completions_get(completions_t* cms, ssize_t index) {
    if (index < 0 || cms->count <= 0 || index >= cms->count)
        return NULL;
    return &cms->elems[index];
}

ic_private const char* completions_get_display(completions_t* cms, ssize_t index,
                                               const char** help) {
    if (help != NULL) {
        *help = NULL;
    }
    completion_t* cm = completions_get(cms, index);
    if (cm == NULL)
        return NULL;
    if (help != NULL) {
        *help = cm->help;
    }
    return (cm->display != NULL ? cm->display : cm->replacement);
}

ic_private const char* completions_get_replacement(completions_t* cms, ssize_t index) {
    completion_t* cm = completions_get(cms, index);
    if (cm == NULL)
        return NULL;
    return cm->replacement;
}

ic_private const char* completions_get_source(completions_t* cms, ssize_t index) {
    completion_t* cm = completions_get(cms, index);
    if (cm == NULL)
        return NULL;
    return cm->source;
}

ic_private const char* completions_get_hint(completions_t* cms, ssize_t index, const char** help) {
    if (help != NULL) {
        *help = NULL;
    }
    completion_t* cm = completions_get(cms, index);
    if (cm == NULL)
        return NULL;
    ssize_t len = ic_strlen(cm->replacement);
    if (len < cm->delete_before)
        return NULL;
    const char* hint = (cm->replacement + cm->delete_before);
    if (*hint == 0 || utf8_is_cont((uint8_t)(*hint)))
        return NULL;  // utf8 boundary?
    if (help != NULL) {
        *help = cm->help;
    }
    return hint;
}

ic_private void completions_set_completer(completions_t* cms, ic_completer_fun_t* completer,
                                          void* arg) {
    cms->completer = completer;
    cms->completer_arg = arg;
}

ic_private void completions_get_completer(completions_t* cms, ic_completer_fun_t** completer,
                                          void** arg) {
    *completer = cms->completer;
    *arg = cms->completer_arg;
}

ic_public void* ic_completion_arg(const ic_completion_env_t* cenv) {
    return (cenv == NULL ? NULL : cenv->env->completions->completer_arg);
}

ic_public bool ic_has_completions(const ic_completion_env_t* cenv) {
    return (cenv == NULL ? false : cenv->env->completions->count > 0);
}

ic_public bool ic_stop_completing(const ic_completion_env_t* cenv) {
    return (cenv == NULL ? true : cenv->env->completions->completer_max <= 0);
}

static ssize_t completion_apply(completion_t* cm, stringbuf_t* sbuf, ssize_t pos) {
    if (cm == NULL)
        return -1;
    debug_msg("completion: apply: %s at %zd\n", cm->replacement, pos);
    ssize_t start = pos - cm->delete_before;
    if (start < 0)
        start = 0;
    ssize_t n = cm->delete_before + cm->delete_after;
    if (ic_strlen(cm->replacement) == n &&
        strncmp(sbuf_string_at(sbuf, start), cm->replacement, to_size_t(n)) == 0) {
        // no changes
        return -1;
    } else {
        sbuf_delete_from_to(sbuf, start, pos + cm->delete_after);
        return sbuf_insert_at(sbuf, cm->replacement, start);
    }
}

ic_private ssize_t completions_apply(completions_t* cms, ssize_t index, stringbuf_t* sbuf,
                                     ssize_t pos) {
    completion_t* cm = completions_get(cms, index);
    return completion_apply(cm, sbuf, pos);
}

ic_private void completions_sort(completions_t* cms) {
    (void)cms;  // preserve insertion order; no sorting required
}

#define IC_MAX_PREFIX (256)

// find longest common prefix (considering full replacements) and complete with that.
ic_private ssize_t completions_apply_longest_prefix(completions_t* cms, stringbuf_t* sbuf,
                                                    ssize_t pos) {
    if (cms->count <= 1) {
        return completions_apply(cms, 0, sbuf, pos);
    }

    if (pos < 0)
        return -1;

    size_t prefix_len = (size_t)pos;
    if (prefix_len >= IC_MAX_PREFIX)
        return -1;  // avoid overrunning our working buffer

    size_t buffer_len = (size_t)sbuf_len(sbuf);
    if (prefix_len > buffer_len)
        return -1;

    char* original_prefix = NULL;
    if (prefix_len > 0) {
        original_prefix = (char*)malloc(prefix_len + 1);
        if (original_prefix == NULL)
            return -1;
        memcpy(original_prefix, sbuf_string(sbuf), prefix_len);
        original_prefix[prefix_len] = '\0';
    }

    char common[IC_MAX_PREFIX + 1] = {0};
    size_t common_len = 0;
    bool common_initialized = false;

    for (ssize_t i = 0; i < cms->count; i++) {
        completion_t* cm = completions_get(cms, i);
        if (cm == NULL || cm->replacement == NULL)
            continue;
        if (cm->delete_before < 0)
            continue;
        size_t delete_before = (size_t)cm->delete_before;
        if (delete_before > prefix_len)
            continue;

        size_t keep_len = prefix_len - delete_before;
        const char* replacement = cm->replacement;
        size_t replacement_len = ic_strlen(replacement);
        size_t final_len = keep_len + replacement_len;
        if (final_len <= prefix_len)
            continue;  // nothing new to add

        size_t capped_final_len = (final_len > IC_MAX_PREFIX ? IC_MAX_PREFIX : final_len);
        char final_prefix[IC_MAX_PREFIX + 1];
        size_t idx = 0;

        if (keep_len > 0 && original_prefix != NULL) {
            size_t copy_len = keep_len;
            if (copy_len > capped_final_len)
                copy_len = capped_final_len;
            memcpy(final_prefix, original_prefix, copy_len);
            idx = copy_len;
        }

        if (idx < capped_final_len) {
            size_t remaining = capped_final_len - idx;
            size_t repl_copy = (replacement_len > remaining ? remaining : replacement_len);
            memcpy(final_prefix + idx, replacement, repl_copy);
            idx += repl_copy;
        }

        final_prefix[idx] = '\0';

        if (!common_initialized) {
            memcpy(common, final_prefix, idx + 1);
            common_len = idx;
            common_initialized = true;
        } else {
            size_t limit = (common_len < idx ? common_len : idx);
            size_t new_common_len = 0;
            while (new_common_len < limit &&
                   common[new_common_len] == final_prefix[new_common_len]) {
                new_common_len++;
            }
            common_len = new_common_len;
            common[common_len] = '\0';
        }

        if (common_len <= prefix_len) {
            common_len = prefix_len;
            break;
        }
    }

    if (original_prefix != NULL) {
        free(original_prefix);
    }

    if (!common_initialized || common_len <= prefix_len) {
        return -1;
    }

    size_t insert_len = common_len - prefix_len;
    if (insert_len > IC_MAX_PREFIX)
        insert_len = IC_MAX_PREFIX;

    char insert_text[IC_MAX_PREFIX + 1];
    for (size_t j = 0; j < insert_len; j++) {
        insert_text[j] = common[prefix_len + j];
    }
    insert_text[insert_len] = '\0';

    completion_t cprefix;
    memset(&cprefix, 0, sizeof(cprefix));
    cprefix.delete_before = 0;
    cprefix.replacement = insert_text;

    ssize_t newpos = completion_apply(&cprefix, sbuf, pos);
    if (newpos < 0)
        return newpos;

    for (ssize_t i = 0; i < cms->count; i++) {
        completion_t* cm = completions_get(cms, i);
        if (cm != NULL) {
            cm->delete_before += (ssize_t)insert_len;
        }
    }

    return newpos;
}

//-------------------------------------------------------------
// Completer functions
//-------------------------------------------------------------

ic_public bool ic_add_completions(ic_completion_env_t* cenv, const char* prefix,
                                  const char** completions) {
    for (const char** pc = completions; *pc != NULL; pc++) {
        if (ic_istarts_with(*pc, prefix)) {
            if (!ic_add_completion_ex(cenv, *pc, NULL, NULL))
                return false;
        }
    }
    return true;
}

ic_public bool ic_add_completion(ic_completion_env_t* cenv, const char* replacement) {
    return ic_add_completion_ex(cenv, replacement, NULL, NULL);
}

ic_public bool ic_add_completion_ex(ic_completion_env_t* cenv, const char* replacement,
                                    const char* display, const char* help) {
    return ic_add_completion_prim(cenv, replacement, display, help, 0, 0);
}

ic_public bool ic_add_completion_ex_with_source(ic_completion_env_t* cenv, const char* replacement,
                                                const char* display, const char* help,
                                                const char* source) {
    return ic_add_completion_prim_with_source(cenv, replacement, display, help, source, 0, 0);
}

ic_public bool ic_add_completion_prim(ic_completion_env_t* cenv, const char* replacement,
                                      const char* display, const char* help, long delete_before,
                                      long delete_after) {
    return (*cenv->complete)(cenv->env, cenv->closure, replacement, display, help, delete_before,
                             delete_after);
}

ic_public bool ic_add_completion_prim_with_source(ic_completion_env_t* cenv,
                                                  const char* replacement, const char* display,
                                                  const char* help, const char* source,
                                                  long delete_before, long delete_after) {
    return (*cenv->complete_with_source)(cenv->env, cenv->closure, replacement, display, help,
                                         source, delete_before, delete_after);
}

static bool prim_add_completion(ic_env_t* env, void* funenv, const char* replacement,
                                const char* display, const char* help, long delete_before,
                                long delete_after) {
    ic_unused(funenv);
    return completions_add(env->completions, replacement, display, help, NULL, delete_before,
                           delete_after);
}

static bool prim_add_completion_with_source(ic_env_t* env, void* funenv, const char* replacement,
                                            const char* display, const char* help,
                                            const char* source, long delete_before,
                                            long delete_after) {
    ic_unused(funenv);
    return completions_add(env->completions, replacement, display, help, source, delete_before,
                           delete_after);
}

ic_public void ic_set_default_completer(ic_completer_fun_t* completer, void* arg) {
    ic_env_t* env = ic_get_env();
    if (env == NULL)
        return;
    completions_set_completer(env->completions, completer, arg);
}

ic_private ssize_t completions_generate(struct ic_env_s* env, completions_t* cms, const char* input,
                                        ssize_t pos, ssize_t max) {
    completions_clear(cms);
    if (cms->completer == NULL || input == NULL || ic_strlen(input) < pos)
        return 0;

    // set up env
    ic_completion_env_t cenv;
    cenv.env = env;
    cenv.input = input, cenv.cursor = (long)pos;
    cenv.arg = cms->completer_arg;
    cenv.complete = &prim_add_completion;
    cenv.complete_with_source = &prim_add_completion_with_source;
    cenv.closure = NULL;
    const char* prefix_alloc = mem_strndup(cms->mem, input, pos);
    const char* prefix = prefix_alloc;
    if (prefix == NULL) {
        if (pos != 0)
            return 0;
        prefix = "";
    }
    cms->completer_max = max;

    // and complete
    cms->completer(&cenv, prefix);

    // restore
    if (prefix_alloc != NULL) {
        mem_free(cms->mem, prefix_alloc);
    }
    return completions_count(cms);
}

// The default completer is no completion is set
static void default_filename_completer(ic_completion_env_t* cenv, const char* prefix) {
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    ic_complete_filename(cenv, prefix, sep, ".", NULL);
}
