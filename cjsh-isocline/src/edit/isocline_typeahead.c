/*
  isocline_typeahead.c

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
    Typeahead capture and replay support.
-----------------------------------------------------------------------------*/

#include "isocline_typeahead.h"

#include <stdbool.h>
#include <string.h>

#include "common.h"
#include "env.h"
#include "isocline.h"
#include "stringbuf.h"
#include "term.h"
#include "tty.h"

static bool typeahead_capture_allowed(ic_env_t* env) {
    if (env == NULL || !env->typeahead_enabled) {
        return false;
    }
    if (env->typeahead_capture_allowed_callback == NULL) {
        return true;
    }
    return env->typeahead_capture_allowed_callback(env->typeahead_capture_allowed_arg);
}

static bool typeahead_has_buffers(ic_env_t* env) {
    return (env != NULL && env->typeahead_input_buffer != NULL &&
            env->typeahead_pending_raw_bytes != NULL);
}

static bool byte_is_raw_control(unsigned char ch) {
    return (ch < 0x20 || ch == 0x7F);
}

static bool bytes_have_control(const char* bytes, ssize_t len) {
    if (bytes == NULL || len <= 0) {
        return false;
    }
    for (ssize_t i = 0; i < len; ++i) {
        if (byte_is_raw_control((unsigned char)bytes[i])) {
            return true;
        }
    }
    return false;
}

static bool bytes_have_visible_typeahead(const uint8_t* bytes, size_t len) {
    if (bytes == NULL || len == 0) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (bytes[i] != '\n' && bytes[i] != '\r') {
            return true;
        }
    }
    return false;
}

static bool buffer_all_newlines(const char* bytes, ssize_t len) {
    if (bytes == NULL || len <= 0) {
        return false;
    }
    for (ssize_t i = 0; i < len; ++i) {
        if (bytes[i] != '\n') {
            return false;
        }
    }
    return true;
}

ic_private void ic_typeahead_filter_escape_sequences_into(const char* input, size_t input_len,
                                                          stringbuf_t* output) {
    if (output == NULL) {
        return;
    }

    sbuf_clear(output);
    if (input == NULL || input_len == 0) {
        return;
    }

    for (size_t i = 0; i < input_len; ++i) {
        unsigned char ch = (unsigned char)input[i];

        if (ch == '\x1b') {
            ssize_t esc_len = 0;
            if (i + 1 < input_len && input[i + 1] >= '0' && input[i + 1] <= '9') {
                i += 1;
                while (i + 1 < input_len && input[i + 1] >= '0' && input[i + 1] <= '9') {
                    i++;
                }
            } else if (skip_esc(input + i, (ssize_t)(input_len - i), &esc_len) && esc_len > 0) {
                i += (size_t)esc_len - 1;
            } else if (i + 1 < input_len) {
                // Match isocline's existing "ESC + following byte" treatment for
                // incomplete or unrecognized escape prefixes.
                i += 1;
            }
        } else if (ch == '\x07' || (ch < 0x20 && ch != '\t' && ch != '\n' && ch != '\r')) {
            continue;
        } else {
            sbuf_append_char(output, (char)ch);
        }
    }
}

ic_private void ic_typeahead_normalize_line_edit_sequences_into(const char* input, size_t input_len,
                                                                stringbuf_t* output) {
    if (output == NULL) {
        return;
    }

    sbuf_clear(output);
    if (input == NULL || input_len == 0) {
        return;
    }

    for (size_t i = 0; i < input_len; ++i) {
        unsigned char ch = (unsigned char)input[i];
        switch (ch) {
            case '\b':
            case 0x7F: {
                ssize_t len = sbuf_len(output);
                if (len > 0) {
                    (void)sbuf_delete_char_before(output, len);
                }
                break;
            }
            case 0x15: {
                ssize_t len = sbuf_len(output);
                if (len > 0) {
                    ssize_t start = sbuf_find_line_start(output, len);
                    sbuf_delete_from_to(output, start, len);
                }
                break;
            }
            case 0x17: {
                while (sbuf_len(output) > 0) {
                    ssize_t len = sbuf_len(output);
                    ssize_t prev = sbuf_prev(output, len, NULL);
                    if (prev < 0 || prev >= len) {
                        break;
                    }
                    char c = sbuf_char_at(output, prev);
                    if (len - prev != 1 || (c != ' ' && c != '\t')) {
                        break;
                    }
                    (void)sbuf_delete_char_before(output, len);
                }
                while (sbuf_len(output) > 0) {
                    ssize_t len = sbuf_len(output);
                    ssize_t prev = sbuf_prev(output, len, NULL);
                    if (prev < 0 || prev >= len) {
                        break;
                    }
                    char c = sbuf_char_at(output, prev);
                    if (len - prev == 1 && (c == ' ' || c == '\t' || c == '\n')) {
                        break;
                    }
                    (void)sbuf_delete_char_before(output, len);
                }
                break;
            }
            default:
                sbuf_append_char(output, (char)ch);
                break;
        }
    }
}

static void assign_input_view(ic_env_t* env, const char* data, ssize_t length) {
    if (!typeahead_has_buffers(env)) {
        return;
    }

    sbuf_clear(env->typeahead_input_buffer);
    if (data != NULL && length > 0) {
        sbuf_append_n(env->typeahead_input_buffer, data, length);
    }
}

static void assign_last_pending_segment(ic_env_t* env, stringbuf_t* normalized) {
    if (!typeahead_has_buffers(env) || normalized == NULL) {
        return;
    }

    const ssize_t len = sbuf_len(normalized);
    const char* data = sbuf_string(normalized);
    if (data == NULL || len <= 0) {
        assign_input_view(env, NULL, 0);
        return;
    }

    if (data[len - 1] == '\n') {
        ssize_t last_non_newline = -1;
        for (ssize_t i = len - 1; i >= 0; --i) {
            if (data[i] != '\n') {
                last_non_newline = i;
                break;
            }
        }

        ssize_t segment_start = 0;
        if (last_non_newline >= 0) {
            for (ssize_t i = last_non_newline; i >= 0; --i) {
                if (data[i] == '\n') {
                    segment_start = i + 1;
                    break;
                }
            }
        }
        assign_input_view(env, data + segment_start, len - segment_start);
    } else {
        ssize_t last_newline = -1;
        for (ssize_t i = len - 1; i >= 0; --i) {
            if (data[i] == '\n') {
                last_newline = i;
                break;
            }
        }
        if (last_newline < 0) {
            assign_input_view(env, data, len);
        } else {
            assign_input_view(env, data + last_newline + 1, len - (last_newline + 1));
        }
    }

    const ssize_t pending_len = sbuf_len(env->typeahead_input_buffer);
    const char* pending = sbuf_string(env->typeahead_input_buffer);
    if (buffer_all_newlines(pending, pending_len)) {
        sbuf_clear(env->typeahead_input_buffer);
    }
}

ic_private bool ic_typeahead_ingest_raw_input(const uint8_t* data, size_t length) {
    ic_env_t* env = ic_get_env();
    if (!typeahead_has_buffers(env) || data == NULL || length == 0 || !env->typeahead_enabled) {
        return false;
    }

    const bool has_visible_typeahead = bytes_have_visible_typeahead(data, length);
    stringbuf_t* combined = sbuf_new(env->mem);
    stringbuf_t* sanitized = sbuf_new(env->mem);
    stringbuf_t* crlf_normalized = sbuf_new(env->mem);
    stringbuf_t* normalized = sbuf_new(env->mem);
    if (combined == NULL || sanitized == NULL || crlf_normalized == NULL || normalized == NULL) {
        sbuf_free(combined);
        sbuf_free(sanitized);
        sbuf_free(crlf_normalized);
        sbuf_free(normalized);
        return false;
    }

    sbuf_append_n(env->typeahead_pending_raw_bytes, (const char*)data, (ssize_t)length);

    const ssize_t existing_len = sbuf_len(env->typeahead_input_buffer);
    const char* existing = sbuf_string(env->typeahead_input_buffer);
    if (existing != NULL && existing_len > 0) {
        sbuf_append_n(combined, existing, existing_len);
    }
    sbuf_append_n(combined, (const char*)data, (ssize_t)length);

    const char* combined_data = sbuf_string(combined);
    ssize_t combined_len = sbuf_len(combined);
    ic_typeahead_filter_escape_sequences_into(combined_data, (size_t)combined_len, sanitized);

    const char* sanitized_data = sbuf_string(sanitized);
    const ssize_t sanitized_len = sbuf_len(sanitized);
    for (ssize_t i = 0; i < sanitized_len; ++i) {
        char ch = sanitized_data[i];
        sbuf_append_char(crlf_normalized, ch == '\r' ? '\n' : ch);
    }

    const char* crlf_data = sbuf_string(crlf_normalized);
    const ssize_t crlf_len = sbuf_len(crlf_normalized);
    ic_typeahead_normalize_line_edit_sequences_into(crlf_data, (size_t)crlf_len, normalized);

    if (sbuf_len(normalized) <= 0) {
        sbuf_clear(env->typeahead_input_buffer);
        if (has_visible_typeahead && env->term != NULL) {
            term_mark_line_visible(env->term, true);
        }
        sbuf_free(combined);
        sbuf_free(sanitized);
        sbuf_free(crlf_normalized);
        sbuf_free(normalized);
        return true;
    }

    assign_last_pending_segment(env, normalized);

    if (sbuf_len(env->typeahead_input_buffer) > 0 && env->term != NULL) {
        term_mark_line_visible(env->term, true);
    }

    sbuf_free(combined);
    sbuf_free(sanitized);
    sbuf_free(crlf_normalized);
    sbuf_free(normalized);
    return true;
}

ic_public bool ic_enable_typeahead(bool enable) {
    ic_env_t* env = ic_get_env();
    if (env == NULL) {
        return false;
    }

    bool previous = env->typeahead_enabled;
    env->typeahead_enabled = enable;
    if (enable != previous || !enable) {
        ic_typeahead_clear();
    }
    return previous;
}

ic_public bool ic_typeahead_is_enabled(void) {
    ic_env_t* env = ic_get_env();
    return (env != NULL && env->typeahead_enabled);
}

ic_public void ic_set_typeahead_capture_allowed_callback(
    ic_typeahead_capture_allowed_fun_t* callback, void* arg) {
    ic_env_t* env = ic_get_env();
    if (env == NULL) {
        return;
    }
    env->typeahead_capture_allowed_callback = callback;
    env->typeahead_capture_allowed_arg = arg;
}

ic_public bool ic_typeahead_capture_available_input(void) {
    ic_env_t* env = ic_get_env();
    if (!typeahead_has_buffers(env) || !typeahead_capture_allowed(env)) {
        return false;
    }

    stringbuf_t* captured = sbuf_new(env->mem);
    if (captured == NULL) {
        return false;
    }

    bool captured_any = tty_capture_pending_raw(env->tty, captured);
    if (captured_any) {
        const char* bytes = sbuf_string(captured);
        ssize_t len = sbuf_len(captured);
        captured_any = ic_typeahead_ingest_raw_input((const uint8_t*)bytes, (size_t)len);
    }

    sbuf_free(captured);
    return captured_any;
}

static void typeahead_flush_pending_raw(ic_env_t* env) {
    if (!typeahead_has_buffers(env)) {
        return;
    }

    const ssize_t raw_len = sbuf_len(env->typeahead_pending_raw_bytes);
    const char* raw = sbuf_string(env->typeahead_pending_raw_bytes);
    if (raw == NULL || raw_len <= 0) {
        return;
    }

    if (bytes_have_control(raw, raw_len)) {
        if (ic_push_raw_input((const uint8_t*)raw, (size_t)raw_len)) {
            sbuf_clear(env->typeahead_pending_raw_bytes);
            sbuf_clear(env->typeahead_input_buffer);
        } else {
            sbuf_clear(env->typeahead_pending_raw_bytes);
        }
    } else {
        sbuf_clear(env->typeahead_pending_raw_bytes);
    }
}

ic_private void ic_typeahead_prepare_for_readline(ic_env_t* env) {
    if (!typeahead_has_buffers(env) || !env->typeahead_enabled) {
        return;
    }

    (void)ic_typeahead_capture_available_input();
    typeahead_flush_pending_raw(env);
}

ic_private const char* ic_typeahead_pending_initial_input(ic_env_t* env) {
    if (!typeahead_has_buffers(env) || !env->typeahead_enabled ||
        sbuf_len(env->typeahead_input_buffer) <= 0) {
        return NULL;
    }
    return sbuf_string(env->typeahead_input_buffer);
}

ic_private ssize_t ic_typeahead_pending_initial_input_len(ic_env_t* env) {
    if (!typeahead_has_buffers(env)) {
        return 0;
    }
    return sbuf_len(env->typeahead_input_buffer);
}

ic_private ssize_t ic_typeahead_pending_raw_byte_count(ic_env_t* env) {
    if (!typeahead_has_buffers(env)) {
        return 0;
    }
    return sbuf_len(env->typeahead_pending_raw_bytes);
}

ic_public void ic_typeahead_clear(void) {
    ic_env_t* env = ic_get_env();
    if (!typeahead_has_buffers(env)) {
        return;
    }
    sbuf_clear(env->typeahead_input_buffer);
    sbuf_clear(env->typeahead_pending_raw_bytes);
}
