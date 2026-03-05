#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "completions.h"
#include "env.h"
#include "history.h"
#include "isocline.h"
#include "prompt_line_replacement.h"
#include "stringbuf.h"
#include "unicode.h"

static void expect_safe_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    (void)fputs(buffer, stderr);
    (void)fflush(stderr);
}

#define EXPECT_TRUE(condition, message)                                                        \
    do {                                                                                       \
        if (!(condition)) {                                                                    \
            expect_safe_log("EXPECT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, message); \
            return false;                                                                      \
        }                                                                                      \
    } while (0)

#define EXPECT_FALSE(condition, message) EXPECT_TRUE(!(condition), message)

#define EXPECT_STREQ(actual_expr, expected_expr, message)                                       \
    do {                                                                                        \
        const char* _actual = (actual_expr);                                                    \
        const char* _expected = (expected_expr);                                                \
        bool _match = false;                                                                    \
        if (_actual == NULL && _expected == NULL) {                                             \
            _match = true;                                                                      \
        } else if (_actual != NULL && _expected != NULL && strcmp(_actual, _expected) == 0) {   \
            _match = true;                                                                      \
        }                                                                                       \
        if (!_match) {                                                                          \
            expect_safe_log("EXPECT_STREQ failed at %s:%d: %s\n", __FILE__, __LINE__, message); \
            expect_safe_log("  actual:   %s\n", _actual == NULL ? "(null)" : _actual);          \
            expect_safe_log("  expected: %s\n", _expected == NULL ? "(null)" : _expected);      \
            return false;                                                                       \
        }                                                                                       \
    } while (0)

static ic_env_t* ensure_env(void) {
    ic_env_t* env = ic_get_env();
    if (env == NULL) {
        expect_safe_log("ic_get_env() returned NULL\n");
    }
    return env;
}

static alloc_t* test_allocator(void) {
    ic_env_t* env = ensure_env();
    return (env == NULL ? NULL : env->mem);
}

static void sample_completion_builder(ic_completion_env_t* cenv, const char* prefix) {
    const char* effective_prefix = (prefix == NULL) ? "" : prefix;
    if (effective_prefix[0] != '\0' && effective_prefix[0] != 'a') {
        return;
    }
    (void)ic_add_completion_prim_with_source(cenv, "alpha", "[warn]alpha", "first", "history", 1,
                                             0);
    (void)ic_add_completion_prim_with_source(cenv, "alphabet", NULL, NULL, "history", 1, 0);
    (void)ic_add_completion_prim_with_source(cenv, "alpine", "[note]alpine", "mountain", "files", 1,
                                             0);
}

static stringbuf_t* new_stringbuf(void) {
    alloc_t* mem = test_allocator();
    if (mem == NULL)
        return NULL;
    return sbuf_new(mem);
}

static bool stub_continuation_checker(const char* buffer, void* arg) {
    bool arg_valid = (arg != NULL);
    bool buffer_valid = (buffer != NULL);
    return arg_valid && (buffer_valid || arg_valid);
}

static bool test_multiline_toggle(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    env->singleline_only = true;
    bool was_enabled = ic_enable_multiline(true);
    EXPECT_FALSE(was_enabled, "multiline should report previously disabled state");
    EXPECT_FALSE(env->singleline_only, "enabling multiline should clear singleline_only flag");

    bool was_enabled_before_disable = ic_enable_multiline(false);
    EXPECT_TRUE(was_enabled_before_disable,
                "disabling multiline should report it was previously enabled");
    EXPECT_TRUE(env->singleline_only, "disabling multiline should set singleline_only flag");

    // Restore default state for later tests
    ic_enable_multiline(true);
    return true;
}

static bool test_line_number_modes(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    env->show_line_numbers = true;
    env->relative_line_numbers = false;

    bool prev_state = ic_enable_line_numbers(false);
    EXPECT_TRUE(prev_state, "ic_enable_line_numbers should return previous enabled state");
    EXPECT_FALSE(env->show_line_numbers, "line numbers should be disabled");
    EXPECT_FALSE(env->relative_line_numbers, "disabling line numbers should clear relative flag");

    env->show_line_numbers = false;
    env->relative_line_numbers = false;

    bool prev_relative = ic_enable_relative_line_numbers(true);
    EXPECT_FALSE(prev_relative, "ic_enable_relative_line_numbers should report previous state");
    EXPECT_TRUE(env->relative_line_numbers, "relative line numbers should now be enabled");
    EXPECT_TRUE(env->show_line_numbers,
                "enabling relative numbering should force absolute line numbers on");

    bool prev_relative_disable = ic_enable_relative_line_numbers(false);
    EXPECT_TRUE(prev_relative_disable,
                "disabling relative numbering should report it was previously enabled");
    EXPECT_FALSE(env->relative_line_numbers, "relative line numbers should be disabled");

    return true;
}

static bool test_line_number_continuation_prompt_toggle(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    env->allow_line_numbers_with_continuation_prompt = false;
    bool prev = ic_enable_line_numbers_with_continuation_prompt(true);
    EXPECT_FALSE(
        prev,
        "enabling line numbers with continuation prompts should report previously disabled state");
    EXPECT_TRUE(env->allow_line_numbers_with_continuation_prompt,
                "environment flag should mirror requested enablement");
    EXPECT_TRUE(ic_line_numbers_with_continuation_prompt_are_enabled(),
                "getter should report enabled state");

    bool prev_disable = ic_enable_line_numbers_with_continuation_prompt(false);
    EXPECT_TRUE(
        prev_disable,
        "disabling line numbers with continuation prompts should report prior enabled state");
    EXPECT_FALSE(env->allow_line_numbers_with_continuation_prompt,
                 "environment flag should be cleared after disabling");
    EXPECT_FALSE(ic_line_numbers_with_continuation_prompt_are_enabled(),
                 "getter should report disabled state");

    return true;
}

static bool test_line_number_prompt_replacement_toggle(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    env->replace_prompt_line_with_line_number = false;
    bool prev = ic_enable_line_number_prompt_replacement(true);
    EXPECT_FALSE(prev, "enabling prompt line replacement should report previously disabled state");
    EXPECT_TRUE(env->replace_prompt_line_with_line_number,
                "environment flag should mirror requested enablement");
    EXPECT_TRUE(ic_line_number_prompt_replacement_is_enabled(),
                "getter should report enabled state");

    bool prev_disable = ic_enable_line_number_prompt_replacement(false);
    EXPECT_TRUE(prev_disable,
                "disabling prompt line replacement should report prior enabled state");
    EXPECT_FALSE(env->replace_prompt_line_with_line_number,
                 "environment flag should be cleared after disabling");
    EXPECT_FALSE(ic_line_number_prompt_replacement_is_enabled(),
                 "getter should report disabled state");

    return true;
}

static bool test_prompt_line_replacement_requires_content(void) {
    ic_prompt_line_replacement_state_t predicate = {
        .replace_prompt_line_with_line_number = true,
        .prompt_has_prefix_lines = true,
        .prompt_begins_with_newline = false,
        .line_numbers_enabled = true,
        .input_has_content = true,
    };

    EXPECT_TRUE(ic_prompt_line_replacement_should_activate(&predicate),
                "predicate should activate when buffer contains input");

    predicate.input_has_content = false;
    EXPECT_FALSE(ic_prompt_line_replacement_should_activate(&predicate),
                 "predicate should keep the prompt visible when the buffer is empty");

    return true;
}

static bool test_visible_whitespace_marker(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    env->show_whitespace_characters = false;
    ic_set_whitespace_marker(NULL);

    const char* default_marker = "\xC2\xB7";  // UTF-8 middle dot
    EXPECT_STREQ(ic_get_whitespace_marker(), default_marker, "default whitespace marker mismatch");

    bool prev = ic_enable_visible_whitespace(true);
    EXPECT_FALSE(prev, "visible whitespace should report previously disabled state");
    EXPECT_TRUE(env->show_whitespace_characters,
                "visible whitespace flag should be enabled after calling API");

    const char* custom_marker = "<·>";
    ic_set_whitespace_marker(custom_marker);
    EXPECT_STREQ(ic_get_whitespace_marker(), custom_marker,
                 "custom whitespace marker should be applied verbatim");

    ic_set_whitespace_marker(NULL);
    EXPECT_STREQ(ic_get_whitespace_marker(), default_marker,
                 "resetting whitespace marker should restore default symbol");

    ic_enable_visible_whitespace(false);
    return true;
}

static bool test_prompt_cleanup_modes(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    env->prompt_cleanup = false;
    env->prompt_cleanup_add_empty_line = false;
    env->prompt_cleanup_truncate_multiline = false;
    env->prompt_cleanup_extra_lines = 0;

    bool prev_cleanup = ic_enable_prompt_cleanup(true, 2);
    EXPECT_FALSE(prev_cleanup, "prompt cleanup should report it was previously disabled");
    EXPECT_TRUE(env->prompt_cleanup, "prompt cleanup flag should be enabled");
    EXPECT_TRUE(env->prompt_cleanup_extra_lines == 2,
                "prompt cleanup extra lines should match requested value");

    bool prev_empty = ic_enable_prompt_cleanup_empty_line(true);
    EXPECT_FALSE(prev_empty, "empty-line cleanup should report it was previously disabled");
    EXPECT_TRUE(env->prompt_cleanup_add_empty_line, "empty-line cleanup flag should be enabled");

    bool prev_truncate = ic_enable_prompt_cleanup_truncate_multiline(true);
    EXPECT_FALSE(prev_truncate, "truncate cleanup should report it was previously disabled");
    EXPECT_TRUE(env->prompt_cleanup_truncate_multiline, "truncate cleanup flag should be enabled");

    // Restore defaults
    ic_enable_prompt_cleanup(false, 0);
    ic_enable_prompt_cleanup_empty_line(false);
    ic_enable_prompt_cleanup_truncate_multiline(false);
    return true;
}

static bool test_multiline_start_line_count_clamp(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    env->multiline_start_line_count = 4;

    size_t previous = ic_set_multiline_start_line_count(0);
    EXPECT_TRUE(previous == 4, "ic_set_multiline_start_line_count should return previous value");
    EXPECT_TRUE(env->multiline_start_line_count == 1,
                "multiline start line count should clamp to minimum of 1");

    previous = ic_set_multiline_start_line_count(300);
    EXPECT_TRUE(previous == 1,
                "ic_set_multiline_start_line_count should report most recent stored value");
    EXPECT_TRUE(env->multiline_start_line_count == 256,
                "multiline start line count should clamp to maximum of 256");

    previous = ic_set_multiline_start_line_count(3);
    EXPECT_TRUE(previous == 256, "previous value should reflect clamped maximum");
    EXPECT_TRUE(env->multiline_start_line_count == 3,
                "multiline start line count should accept values within the allowed range");

    return true;
}

static bool test_editline_buffer_api_without_editor(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    env->current_editor = NULL;
    EXPECT_FALSE(ic_set_buffer("demo"), "setting buffer without editor should fail");
    EXPECT_TRUE(ic_get_buffer() == NULL, "get buffer should return NULL without editor");

    size_t pos = 42;
    EXPECT_FALSE(ic_get_cursor_pos(&pos), "cursor query should fail without editor");
    EXPECT_TRUE(pos == 42, "cursor output argument should remain unchanged on failure");

    EXPECT_FALSE(ic_set_cursor_pos(1), "cursor set should fail without editor");
    EXPECT_FALSE(ic_request_submit(), "submit request should fail without editor");
    EXPECT_FALSE(ic_current_loop_reset("buf", "prompt", "inline"),
                 "loop reset should fail without editor");

    return true;
}

static bool test_continuation_callback_registration(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    env->continuation_check_callback = NULL;
    env->continuation_check_arg = NULL;

    ic_set_check_for_continuation_or_return_callback(stub_continuation_checker, (void*)0x1);
    EXPECT_TRUE(env->continuation_check_callback == stub_continuation_checker,
                "setter should store continuation callback pointer");
    EXPECT_TRUE(env->continuation_check_arg == (void*)0x1,
                "setter should store continuation callback argument");

    ic_set_check_for_continuation_or_return_callback(NULL, NULL);
    EXPECT_TRUE(env->continuation_check_callback == NULL,
                "clearing continuation callback should reset pointer");
    EXPECT_TRUE(env->continuation_check_arg == NULL,
                "clearing continuation callback should reset argument");

    return true;
}

static bool test_completion_generation_and_apply(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    ic_completer_fun_t* prev_fun = NULL;
    void* prev_arg = NULL;
    completions_get_completer(env->completions, &prev_fun, &prev_arg);
    completions_set_completer(env->completions, &sample_completion_builder, NULL);

    ssize_t produced = completions_generate(env, env->completions, "a", 1, 8);
    EXPECT_TRUE(produced == 3, "stub completer should generate three entries");
    completions_sort(env->completions);

    const char* help = NULL;
    const char* display0 = completions_get_display(env->completions, 0, &help);
    EXPECT_STREQ(display0, "\\[warn]alpha", "bbcode brackets should be escaped in display");
    EXPECT_STREQ(help, "first", "help metadata should be preserved");

    bool found_alphabet = false;
    bool found_alpine = false;
    const char* alpine_source = NULL;
    for (ssize_t i = 0; i < produced; ++i) {
        const char* replacement = completions_get_replacement(env->completions, i);
        if (replacement == NULL)
            continue;
        if (strcmp(replacement, "alphabet") == 0)
            found_alphabet = true;
        if (strcmp(replacement, "alpine") == 0) {
            found_alpine = true;
            alpine_source = completions_get_source(env->completions, i);
        }
    }
    EXPECT_TRUE(found_alphabet && found_alpine,
                "completions should contain both 'alphabet' and 'alpine'");

    const char* hint0 = completions_get_hint(env->completions, 0, &help);
    EXPECT_STREQ(hint0, "lpha", "hint should expose remaining suffix after delete_before");

    EXPECT_TRUE(alpine_source != NULL && strcmp(alpine_source, "files") == 0,
                "source metadata should be recorded for alpine completion");

    stringbuf_t* sb = new_stringbuf();
    if (sb == NULL)
        return false;
    sbuf_replace(sb, "a");
    ssize_t new_pos = completions_apply(env->completions, 0, sb, 1);
    EXPECT_TRUE(new_pos > 1, "completion apply should advance cursor");
    EXPECT_STREQ(sbuf_string(sb), "alpha", "applying first completion should replace buffer");

    sbuf_replace(sb, "a");
    ssize_t prefix_pos = completions_apply_longest_prefix(env->completions, sb, 1);
    EXPECT_TRUE(prefix_pos >= 2, "longest prefix should extend beyond initial prefix");
    EXPECT_TRUE(strncmp(sbuf_string(sb), "al", 2) == 0,
                "longest common prefix across completions should start with 'al'");

    sbuf_free(sb);
    completions_clear(env->completions);
    completions_set_completer(env->completions, prev_fun, prev_arg);
    return true;
}

static bool test_history_dedup_snapshot(void) {
    ic_env_t* env = ensure_env();
    alloc_t* mem = test_allocator();
    if (env == NULL || mem == NULL)
        return false;

    history_t* history = history_new(mem);
    if (history == NULL)
        return false;

    const char* history_path = "./isocline_history_behavior.log";
    (void)remove(history_path);
    history_load_from(history, history_path, 32);
    history_clear(history);

    history_enable_duplicates(history, false);
    EXPECT_TRUE(history_push(history, "echo hi"), "initial history push should succeed");
    EXPECT_TRUE(history_push(history, "echo hi"), "duplicate push should rewrite last entry");

    history_enable_duplicates(history, true);
    EXPECT_TRUE(history_push_with_exit_code(history, "echo hi", 7),
                "duplicates should be kept once enabled");
    EXPECT_TRUE(history_push(history, "printf bye"), "new unique entry should append");

    history_snapshot_t snap = {0};
    EXPECT_TRUE(history_snapshot_load(history, &snap, false), "snapshot should load from file");
    EXPECT_TRUE(snap.count == 3, "snapshot should contain three entries");

    bool found_printf = false;
    ssize_t echo_instances = 0;
    for (ssize_t i = 0; i < snap.count; ++i) {
        const history_entry_t* entry = history_snapshot_get(&snap, i);
        if (entry == NULL)
            continue;
        if (strcmp(entry->command, "printf bye") == 0)
            found_printf = true;
        if (strcmp(entry->command, "echo hi") == 0)
            echo_instances++;
    }
    EXPECT_TRUE(found_printf, "history snapshot should contain the printf entry");
    EXPECT_TRUE(echo_instances >= 2, "history snapshot should retain duplicate echo entries");

    ssize_t search_idx = -1;
    EXPECT_TRUE(history_search_prefix(history, 0, "printf", true, &search_idx),
                "prefix search should find most recent match");
    EXPECT_TRUE(search_idx >= 0, "search index should be non-negative");
    const char* found_command = history_get(history, search_idx);
    EXPECT_TRUE(found_command != NULL && strcmp(found_command, "printf bye") == 0,
                "prefix search should reference the printf entry");

    history_snapshot_free(history, &snap);
    history_clear(history);
    history_free(history);
    (void)remove(history_path);
    return true;
}

static bool test_history_fuzzy_case_toggle(void) {
    ic_env_t* env = ensure_env();
    alloc_t* mem = test_allocator();
    if (env == NULL || mem == NULL)
        return false;

    history_t* history = history_new(mem);
    if (history == NULL)
        return false;

    const char* history_path = "./isocline_history_case_toggle.log";
    (void)remove(history_path);
    history_load_from(history, history_path, 16);
    history_clear(history);

    EXPECT_TRUE(history_push(history, "ls"), "initial lowercase history entry should persist");
    EXPECT_TRUE(history_push(history, "printf hi"), "second entry should persist for contrast");
    EXPECT_TRUE(history_push(history, "MAX"),
                "uppercase history entry should persist for symmetry tests");

    history_match_t matches[4];
    ssize_t match_count = 0;
    bool exit_filter_applied = false;
    int exit_filter_code = 0;

    history_set_fuzzy_case_sensitive(history, true);
    EXPECT_FALSE(history_fuzzy_search(history, "LS", matches, 4, &match_count, &exit_filter_applied,
                                      &exit_filter_code),
                 "case-sensitive search should not match entries with different casing");
    EXPECT_TRUE(match_count == 0, "case-sensitive mismatch should produce zero matches");

    history_set_fuzzy_case_sensitive(history, false);
    EXPECT_TRUE(history_fuzzy_search(history, "LS", matches, 4, &match_count, &exit_filter_applied,
                                     &exit_filter_code),
                "case-insensitive search should find matching entries regardless of case");
    EXPECT_TRUE(match_count > 0, "case-insensitive mode should yield results");

    ssize_t reverse_match_count = 0;
    EXPECT_TRUE(
        history_fuzzy_search(history, "max", matches, 4, &reverse_match_count, NULL, NULL),
        "case-insensitive search should allow lowercase queries to match uppercase entries");
    EXPECT_TRUE(
        reverse_match_count > 0,
        "lowercase query should match uppercase history entries when case sensitivity is disabled");

    history_clear(history);
    history_free(history);
    (void)remove(history_path);
    return true;
}

static bool test_history_fuzzy_case_toggle_via_api(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    history_t* original = env->history;
    history_t* temp_history = history_new(env->mem);
    if (temp_history == NULL)
        return false;

    const char* history_path = "./isocline_history_case_toggle_env.log";
    (void)remove(history_path);
    history_load_from(temp_history, history_path, 16);
    history_clear(temp_history);

    env->history = temp_history;

    EXPECT_TRUE(history_push(temp_history, "ls"),
                "temporary env history should accept initial lowercase entry");
    EXPECT_TRUE(history_push(temp_history, "printf hi"),
                "temporary env history should accept secondary entry");
    EXPECT_TRUE(history_push(temp_history, "MAX"),
                "temporary env history should accept uppercase entry for symmetry tests");

    history_match_t matches[4];
    ssize_t match_count = 0;

    ic_enable_history_fuzzy_case_sensitive(true);
    EXPECT_FALSE(history_fuzzy_search(temp_history, "LS", matches, 4, &match_count, NULL, NULL),
                 "case-sensitive env history should not match different casing");
    EXPECT_TRUE(match_count == 0, "case-sensitive env history should produce zero matches");

    ic_enable_history_fuzzy_case_sensitive(false);
    EXPECT_TRUE(history_fuzzy_search(temp_history, "LS", matches, 4, &match_count, NULL, NULL),
                "case-insensitive env history should match irrespective of casing");
    EXPECT_TRUE(match_count > 0, "case-insensitive env history should yield matches");

    ssize_t reverse_match_count = 0;
    EXPECT_TRUE(
        history_fuzzy_search(temp_history, "max", matches, 4, &reverse_match_count, NULL, NULL),
        "case-insensitive env history should allow lowercase queries to match uppercase entries");
    EXPECT_TRUE(reverse_match_count > 0,
                "lowercase query should match uppercase entries when toggled globally");

    history_clear(temp_history);
    env->history = original;
    history_free(temp_history);
    (void)remove(history_path);
    ic_enable_history_fuzzy_case_sensitive(true);
    return true;
}

static bool test_line_wrapping_calculations(void) {
    stringbuf_t* sb = new_stringbuf();
    if (sb == NULL)
        return false;

    sbuf_replace(sb, "abcd");
    rowcol_t rc = {0};
    (void)sbuf_get_rc_at_pos(sb, 2, 0, 0, 3, &rc);
    EXPECT_TRUE(rc.row >= 1, "wrapped rows should advance after terminal width");
    EXPECT_TRUE(rc.col >= 0 && rc.col < 2, "column should stay within terminal width bounds");
    ssize_t roundtrip = sbuf_get_pos_at_rc(sb, 2, 0, 0, rc.row, rc.col);
    EXPECT_TRUE(roundtrip == 3, "row/column lookup should round-trip to position");

    sbuf_replace(sb, "line1\nline2");
    rowcol_t multiline = {0};
    (void)sbuf_get_rc_at_pos(sb, 10, 0, 0, 6, &multiline);
    EXPECT_TRUE(multiline.row > 0, "newline should advance to next logical row");

    sbuf_replace(sb, "abcdefghij");
    rowcol_t wide = {0};
    (void)sbuf_get_rc_at_pos(sb, 10, 0, 0, 7, &wide);
    rowcol_t shrink = {0};
    (void)sbuf_get_wrapped_rc_at_pos(sb, 10, 5, 0, 0, 7, &shrink);
    EXPECT_TRUE(shrink.row >= wide.row, "shrinking the terminal should not decrease row index");
    EXPECT_TRUE(shrink.col >= 0 && shrink.col < 5,
                "shrinking the terminal should recompute wrapped columns");

    sbuf_free(sb);
    return true;
}

static bool test_unicode_decode_utf8_valid_sequences(void) {
    typedef struct utf8_case_s {
        const uint8_t* data;
        ssize_t len;
        unicode_codepoint_t expected_codepoint;
        ssize_t expected_bytes;
    } utf8_case_t;

    static const uint8_t one_byte[] = {0x24};                     // U+0024 '$'
    static const uint8_t two_byte[] = {0xC2, 0xA2};               // U+00A2 '¢'
    static const uint8_t three_byte[] = {0xE2, 0x82, 0xAC};       // U+20AC '€'
    static const uint8_t four_byte[] = {0xF0, 0x9F, 0x98, 0x80};  // U+1F600 '😀'

    const utf8_case_t cases[] = {
        {one_byte, (ssize_t)sizeof(one_byte), 0x24, 1},
        {two_byte, (ssize_t)sizeof(two_byte), 0x00A2, 2},
        {three_byte, (ssize_t)sizeof(three_byte), 0x20AC, 3},
        {four_byte, (ssize_t)sizeof(four_byte), 0x1F600, 4},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        unicode_codepoint_t codepoint = 0;
        ssize_t bytes_read = 0;
        bool ok = unicode_decode_utf8(cases[i].data, cases[i].len, &codepoint, &bytes_read);
        EXPECT_TRUE(ok, "unicode_decode_utf8 should accept valid UTF-8 sequences");
        EXPECT_TRUE(codepoint == cases[i].expected_codepoint,
                    "decoded codepoint should match expected value");
        EXPECT_TRUE(bytes_read == cases[i].expected_bytes,
                    "decoded byte count should match expected sequence length");
    }

    return true;
}

static bool test_unicode_decode_utf8_invalid_sequences(void) {
    typedef struct utf8_invalid_case_s {
        const uint8_t* data;
        ssize_t len;
        uint8_t expected_fallback;
    } utf8_invalid_case_t;

    static const uint8_t lone_continuation[] = {0x80};
    static const uint8_t overlong_two[] = {0xC0, 0xAF};
    static const uint8_t overlong_three[] = {0xE0, 0x80, 0x80};
    static const uint8_t overlong_four[] = {0xF0, 0x80, 0x80, 0x80};
    static const uint8_t surrogate_half[] = {0xED, 0xA0, 0x80};
    static const uint8_t too_large[] = {0xF4, 0x90, 0x80, 0x80};
    static const uint8_t bad_continuation[] = {0xE2, 0x28, 0xA1};
    static const uint8_t truncated_three[] = {0xE2};

    const utf8_invalid_case_t cases[] = {
        {lone_continuation, (ssize_t)sizeof(lone_continuation), 0x80},
        {overlong_two, (ssize_t)sizeof(overlong_two), 0xC0},
        {overlong_three, (ssize_t)sizeof(overlong_three), 0xE0},
        {overlong_four, (ssize_t)sizeof(overlong_four), 0xF0},
        {surrogate_half, (ssize_t)sizeof(surrogate_half), 0xED},
        {too_large, (ssize_t)sizeof(too_large), 0xF4},
        {bad_continuation, (ssize_t)sizeof(bad_continuation), 0xE2},
        {truncated_three, (ssize_t)sizeof(truncated_three), 0xE2},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        unicode_codepoint_t codepoint = 0;
        ssize_t bytes_read = 0;
        bool ok = unicode_decode_utf8(cases[i].data, cases[i].len, &codepoint, &bytes_read);
        EXPECT_FALSE(ok, "unicode_decode_utf8 should reject invalid UTF-8 sequences");
        EXPECT_TRUE(codepoint == (unicode_codepoint_t)cases[i].expected_fallback,
                    "invalid UTF-8 should fall back to the first byte value");
        EXPECT_TRUE(bytes_read == 1,
                    "invalid UTF-8 should consume exactly one byte for forward progress");
    }

    unicode_codepoint_t codepoint = 1234;
    ssize_t bytes_read = 99;
    EXPECT_FALSE(unicode_decode_utf8(NULL, 1, &codepoint, &bytes_read),
                 "decode should fail for NULL input");
    EXPECT_TRUE(codepoint == 0, "NULL input should reset decoded codepoint to zero");
    EXPECT_TRUE(bytes_read == 0, "NULL input should report zero bytes consumed");

    codepoint = 1234;
    bytes_read = 99;
    EXPECT_FALSE(unicode_decode_utf8((const uint8_t*)"A", 0, &codepoint, &bytes_read),
                 "decode should fail for zero-length input");
    EXPECT_TRUE(codepoint == 0, "zero-length input should reset decoded codepoint to zero");
    EXPECT_TRUE(bytes_read == 0, "zero-length input should report zero bytes consumed");

    return true;
}

static bool test_unicode_encode_utf8_roundtrip(void) {
    const unicode_codepoint_t cps[] = {0x24, 0x00A2, 0x20AC, 0x1F600};

    for (size_t i = 0; i < sizeof(cps) / sizeof(cps[0]); ++i) {
        uint8_t encoded[4] = {0, 0, 0, 0};
        int encoded_len = unicode_encode_utf8(cps[i], encoded);
        EXPECT_TRUE(encoded_len > 0 && encoded_len <= 4,
                    "unicode_encode_utf8 should encode valid codepoints");

        unicode_codepoint_t decoded = 0;
        ssize_t bytes_read = 0;
        EXPECT_TRUE(unicode_decode_utf8(encoded, encoded_len, &decoded, &bytes_read),
                    "encoded bytes should decode successfully");
        EXPECT_TRUE(decoded == cps[i], "decoded codepoint should match original encoded value");
        EXPECT_TRUE(bytes_read == encoded_len,
                    "decoder byte count should match encoded byte length");
    }

    uint8_t out[4] = {0, 0, 0, 0};
    EXPECT_TRUE(unicode_encode_utf8(0x110000, out) == 0,
                "encoder should reject codepoints above Unicode maximum");
    EXPECT_TRUE(unicode_encode_utf8(0xD800, out) == 0,
                "encoder should reject UTF-16 surrogate codepoints");
    return true;
}

static bool test_unicode_qutf8_raw_byte_roundtrip(void) {
    const uint8_t invalid_utf8[] = {0xFF};
    ssize_t read = 0;
    unicode_t u = unicode_from_qutf8(invalid_utf8, (ssize_t)sizeof(invalid_utf8), &read);

    uint8_t recovered_raw = 0;
    EXPECT_TRUE(read == 1, "qutf8 decoder should consume one byte for invalid UTF-8");
    EXPECT_TRUE(unicode_is_raw(u, &recovered_raw),
                "invalid UTF-8 should map to a raw-plane Unicode sentinel");
    EXPECT_TRUE(recovered_raw == 0xFF,
                "raw-plane sentinel should preserve original invalid byte value");

    uint8_t qutf8_out[5] = {0, 0, 0, 0, 0};
    unicode_to_qutf8(u, qutf8_out);
    EXPECT_TRUE(qutf8_out[0] == 0xFF && qutf8_out[1] == 0,
                "raw-plane Unicode should encode back into the original raw byte");

    static const uint8_t euro[] = {0xE2, 0x82, 0xAC};
    read = 0;
    unicode_t euro_u = unicode_from_qutf8(euro, (ssize_t)sizeof(euro), &read);
    EXPECT_TRUE(read == 3, "valid UTF-8 should decode to Unicode in qutf8 decoder");
    EXPECT_TRUE(euro_u == 0x20AC, "valid UTF-8 should decode to expected codepoint");

    memset(qutf8_out, 0, sizeof(qutf8_out));
    unicode_to_qutf8(euro_u, qutf8_out);
    EXPECT_TRUE(qutf8_out[0] == 0xE2 && qutf8_out[1] == 0x82 && qutf8_out[2] == 0xAC,
                "Unicode codepoint should encode back to canonical UTF-8 bytes");

    return true;
}

static bool test_unicode_width_calculation_with_invalid_and_ansi(void) {
    static const uint8_t mixed[] = {'A', 0xE2, 0x82, 0xAC, 0x80, 'B'};
    size_t width = unicode_calculate_utf8_width((const char*)mixed, sizeof(mixed));
    EXPECT_TRUE(width == 4,
                "utf8 width should count ASCII, valid UTF-8, and invalid bytes deterministically");

    static const uint8_t ansi_and_invalid[] = {'\x1B', '[', '3', '1', 'm', 'A',
                                               '\x1B', '[', '0', 'm', 0x80};
    size_t ansi_chars = 0;
    size_t visible_chars = 0;
    size_t display_width = unicode_calculate_display_width(
        (const char*)ansi_and_invalid, sizeof(ansi_and_invalid), &ansi_chars, &visible_chars);
    EXPECT_TRUE(display_width == 2,
                "display width should ignore ANSI escapes while counting invalid byte fallback");
    EXPECT_TRUE(ansi_chars == 9,
                "ANSI byte counter should include bytes belonging to both escape sequences");
    EXPECT_TRUE(visible_chars == 2,
                "visible character count should include regular glyphs and invalid byte fallback");

    return true;
}

static bool test_str_next_ofs_with_utf8_and_escape_sequences(void) {
    static const char sample[] = {'A', (char)0xE2, (char)0x82, (char)0xAC, '\x1B', '[',
                                  '3', '1',        'm',        'B',        0};
    ssize_t width = 0;

    ssize_t ofs_a = str_next_ofs(sample, 10, 0, &width);
    EXPECT_TRUE(ofs_a == 1, "ASCII codepoint should consume one byte");
    EXPECT_TRUE(width == 1, "ASCII width should be one column");

    ssize_t ofs_euro = str_next_ofs(sample, 10, 1, &width);
    EXPECT_TRUE(ofs_euro == 3, "UTF-8 multibyte sequence should consume all continuation bytes");
    EXPECT_TRUE(width == 1, "Euro sign should render as single column");

    ssize_t ofs_esc = str_next_ofs(sample, 10, 4, &width);
    EXPECT_TRUE(ofs_esc == 5, "CSI escape should be treated as one logical sequence");

    ssize_t ofs_b = str_next_ofs(sample, 10, 9, &width);
    EXPECT_TRUE(ofs_b == 1, "trailing ASCII character should consume one byte");

    ssize_t ofs_end = str_next_ofs(sample, 10, 10, &width);
    EXPECT_TRUE(ofs_end == 0, "str_next_ofs should return zero at end of buffer");

    static const char continuation_cluster[] = {'x', (char)0x80, (char)0x81, 'y', 0};
    ssize_t ofs_cluster = str_next_ofs(continuation_cluster, 4, 1, NULL);
    EXPECT_TRUE(ofs_cluster == 2,
                "invalid leading byte should still advance through contiguous continuation bytes");

    return true;
}

static bool test_stringbuf_utf8_navigation_and_deletion(void) {
    stringbuf_t* sb = new_stringbuf();
    if (sb == NULL)
        return false;

    sbuf_replace(sb,
                 "a\xE2\x82\xAC"
                 "b");
    EXPECT_TRUE(sbuf_len(sb) == 5, "buffer should contain one ASCII, one UTF-8 glyph, one ASCII");

    ssize_t width = 0;
    ssize_t pos = sbuf_next(sb, 0, &width);
    EXPECT_TRUE(pos == 1 && width == 1, "moving over ASCII should advance by one byte");

    pos = sbuf_next(sb, pos, &width);
    EXPECT_TRUE(pos == 4 && width == 1,
                "moving over UTF-8 glyph should advance by full codepoint byte length");

    pos = sbuf_next(sb, pos, &width);
    EXPECT_TRUE(pos == 5 && width == 1, "moving over final ASCII byte should reach buffer end");

    pos = sbuf_prev(sb, pos, &width);
    EXPECT_TRUE(pos == 4 && width == 1, "reverse movement should step back over ASCII byte");

    pos = sbuf_prev(sb, pos, &width);
    EXPECT_TRUE(pos == 1 && width == 1,
                "reverse movement should step back over entire UTF-8 glyph atomically");

    ssize_t new_pos = sbuf_delete_char_before(sb, 4);
    EXPECT_TRUE(new_pos == 1,
                "deleting before UTF-8 boundary should land at start of deleted glyph");
    EXPECT_STREQ(sbuf_string(sb), "ab",
                 "deleting UTF-8 glyph should remove all bytes of codepoint");

    sbuf_free(sb);
    return true;
}

static bool test_push_raw_input_preconditions(void) {
    static const uint8_t bytes[] = {'a', 'b', 'c'};
    EXPECT_TRUE(ic_push_raw_input(bytes, 0), "zero-length raw input should be accepted as no-op");

    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    tty_t* saved_tty = env->tty;
    env->tty = NULL;
    EXPECT_FALSE(ic_push_raw_input(bytes, sizeof(bytes)),
                 "raw input push should fail when no TTY backend is available");
    EXPECT_FALSE(ic_push_key_event(KEY_ENTER),
                 "single key event push should fail when no TTY backend is available");
    EXPECT_FALSE(ic_push_key_sequence((const ic_keycode_t[]){KEY_ENTER, KEY_TAB}, 2),
                 "key sequence push should fail when no TTY backend is available");
    env->tty = saved_tty;

    return true;
}

struct tty_s {
    int fd_in;
    bool raw_enabled;
    bool is_utf8;
    bool has_term_resize_event;
    bool term_resize_event;
    bool lost_terminal;
    alloc_t* mem;
    code_t pushbuf[32];
    ssize_t push_count;
    uint8_t cpushbuf[32];
    ssize_t cpush_count;
};

static bool test_tty_character_pushback_capacity_guard(void) {
    struct tty_s tty_probe;
    memset(&tty_probe, 0, sizeof(tty_probe));

    for (size_t i = 0; i < 33; ++i) {
        tty_cpush_char((tty_t*)&tty_probe, (uint8_t)('a' + (i % 26)));
    }

    EXPECT_TRUE(tty_probe.cpush_count == 32,
                "character pushback buffer should clamp at 32 entries");

    size_t pops = 0;
    uint8_t c = 0;
    while (tty_cpop((tty_t*)&tty_probe, &c)) {
        pops++;
    }
    EXPECT_TRUE(pops == 32, "character pushback pop count should match clamped capacity");

    return true;
}

static bool test_push_raw_input_null_pointer_rejected(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    struct tty_s tty_probe;
    memset(&tty_probe, 0, sizeof(tty_probe));

    tty_t* saved_tty = env->tty;
    env->tty = (tty_t*)&tty_probe;

    EXPECT_FALSE(ic_push_raw_input(NULL, 3),
                 "non-empty raw input push should reject NULL data buffers");

    env->tty = saved_tty;
    return true;
}

static bool test_prompt_marker_roundtrip(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    ic_set_prompt_marker("=> ", ".. ");
    EXPECT_STREQ(ic_get_prompt_marker(), "=> ", "primary prompt marker should round-trip");
    EXPECT_STREQ(ic_get_continuation_prompt_marker(), ".. ",
                 "continuation prompt marker should round-trip");

    ic_set_prompt_marker(":: ", NULL);
    EXPECT_STREQ(ic_get_prompt_marker(), ":: ", "primary marker should update to custom value");
    EXPECT_STREQ(ic_get_continuation_prompt_marker(),
                 ":: ", "NULL continuation marker should mirror primary marker");

    ic_set_prompt_marker(NULL, NULL);
    EXPECT_STREQ(ic_get_prompt_marker(), "> ", "NULL primary marker should restore default marker");
    EXPECT_STREQ(ic_get_continuation_prompt_marker(), "> ",
                 "NULL continuation marker should restore default continuation marker");

    return true;
}

static bool test_hint_delay_clamps(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    env->hint_delay = 250;
    long previous = ic_set_hint_delay(-1);
    EXPECT_TRUE(previous == 250, "hint delay setter should return previous value");
    EXPECT_TRUE(env->hint_delay == 0, "negative hint delay should clamp to zero");

    previous = ic_set_hint_delay(99999);
    EXPECT_TRUE(previous == 0, "hint delay setter should return prior clamped value");
    EXPECT_TRUE(env->hint_delay == 5000, "hint delay should clamp to documented maximum");

    previous = ic_set_hint_delay(73);
    EXPECT_TRUE(previous == 5000, "hint delay setter should return previous maximum clamp");
    EXPECT_TRUE(env->hint_delay == 73, "hint delay should accept in-range values unchanged");

    return true;
}

static bool test_status_hint_mode_validation(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    env->status_hint_mode = IC_STATUS_HINT_PERSISTENT;
    ic_status_hint_mode_t prev = ic_set_status_hint_mode((ic_status_hint_mode_t)99);
    EXPECT_TRUE(prev == IC_STATUS_HINT_PERSISTENT,
                "invalid status hint mode should report previous configured mode");
    EXPECT_TRUE(ic_get_status_hint_mode() == IC_STATUS_HINT_NORMAL,
                "invalid status hint mode should normalize to NORMAL");

    prev = ic_set_status_hint_mode(IC_STATUS_HINT_OFF);
    EXPECT_TRUE(prev == IC_STATUS_HINT_NORMAL,
                "setting OFF should report normalized previous mode");
    EXPECT_TRUE(ic_get_status_hint_mode() == IC_STATUS_HINT_OFF,
                "status hint getter should expose OFF mode");

    (void)ic_set_status_hint_mode(IC_STATUS_HINT_NORMAL);
    return true;
}

static bool test_option_toggle_consistency(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    env->complete_autotab = false;
    EXPECT_FALSE(ic_enable_auto_tab(true), "auto-tab should report previously disabled state");
    EXPECT_TRUE(env->complete_autotab, "auto-tab flag should become enabled");

    env->complete_nopreview = false;
    EXPECT_TRUE(ic_enable_completion_preview(false),
                "completion preview disable should report previously enabled state");
    EXPECT_TRUE(env->complete_nopreview, "preview disable should invert internal flag");
    EXPECT_FALSE(ic_enable_completion_preview(true),
                 "completion preview enable should report previously disabled state");
    EXPECT_FALSE(env->complete_nopreview, "preview enable should clear inverted flag");

    env->no_multiline_indent = false;
    EXPECT_TRUE(ic_enable_multiline_indent(false),
                "multiline indent disable should report previously enabled state");
    EXPECT_TRUE(env->no_multiline_indent, "multiline indent disable should set inverted flag");

    env->no_hint = false;
    EXPECT_TRUE(ic_enable_hint(false), "hint disable should report previously enabled state");
    EXPECT_TRUE(env->no_hint, "hint disable should set inverted no_hint flag");

    env->spell_correct = true;
    EXPECT_TRUE(ic_enable_spell_correct(false),
                "spell-correct disable should report previously enabled state");
    EXPECT_FALSE(env->spell_correct, "spell-correct disable should clear flag");

    env->no_highlight = false;
    EXPECT_TRUE(ic_enable_highlight(false),
                "highlight disable should report previously enabled state");
    EXPECT_TRUE(env->no_highlight, "highlight disable should set inverted flag");

    env->no_help = false;
    EXPECT_TRUE(ic_enable_inline_help(false),
                "inline help disable should report previously enabled state");
    EXPECT_TRUE(env->no_help, "inline help disable should set inverted flag");

    env->highlight_current_line_number = true;
    EXPECT_TRUE(ic_enable_current_line_number_highlight(false),
                "current line number highlight disable should report previous state");
    EXPECT_FALSE(ic_current_line_number_highlight_is_enabled(),
                 "current line number highlight getter should mirror state");

    env->inline_right_prompt_follows_cursor = false;
    EXPECT_FALSE(ic_enable_inline_right_prompt_cursor_follow(true),
                 "inline right prompt follow should report previous disabled state");
    EXPECT_TRUE(ic_inline_right_prompt_follows_cursor(),
                "inline right prompt follow getter should mirror state");

    env->no_bracematch = false;
    EXPECT_TRUE(ic_enable_brace_matching(false),
                "brace matching disable should report previously enabled state");
    EXPECT_TRUE(env->no_bracematch, "brace matching disable should set inverted flag");

    env->no_autobrace = false;
    EXPECT_TRUE(ic_enable_brace_insertion(false),
                "brace insertion disable should report previously enabled state");
    EXPECT_TRUE(env->no_autobrace, "brace insertion disable should set inverted flag");

    (void)ic_enable_multiline_indent(true);
    (void)ic_enable_hint(true);
    (void)ic_enable_spell_correct(true);
    (void)ic_enable_highlight(true);
    (void)ic_enable_inline_help(true);
    (void)ic_enable_current_line_number_highlight(true);
    (void)ic_enable_inline_right_prompt_cursor_follow(false);
    (void)ic_enable_brace_matching(true);
    (void)ic_enable_brace_insertion(true);
    return true;
}

static bool test_prompt_cleanup_newline_and_getters(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    env->prompt_cleanup = false;
    env->prompt_cleanup_extra_lines = 0;
    env->prompt_cleanup_newline_after_execution = false;

    EXPECT_FALSE(ic_prompt_cleanup_is_enabled(),
                 "prompt cleanup getter should report disabled state");
    EXPECT_TRUE(ic_prompt_cleanup_extra_lines() == 0,
                "prompt cleanup extra lines getter should report current value");

    EXPECT_FALSE(ic_enable_prompt_cleanup(true, 3),
                 "prompt cleanup enable should report previously disabled state");
    EXPECT_TRUE(ic_prompt_cleanup_is_enabled(),
                "prompt cleanup getter should report enabled state");
    EXPECT_TRUE(ic_prompt_cleanup_extra_lines() == 3,
                "prompt cleanup extra lines should match configured value");

    EXPECT_FALSE(ic_enable_prompt_cleanup_newline(true),
                 "prompt cleanup newline enable should report previously disabled state");
    EXPECT_TRUE(ic_prompt_cleanup_newline_is_enabled(),
                "prompt cleanup newline getter should mirror enabled state");

    EXPECT_TRUE(ic_enable_prompt_cleanup_newline(false),
                "prompt cleanup newline disable should report previous enabled state");
    EXPECT_FALSE(ic_prompt_cleanup_newline_is_enabled(),
                 "prompt cleanup newline getter should mirror disabled state");

    (void)ic_enable_prompt_cleanup(false, 0);
    return true;
}

static bool test_brace_pair_setters_validation(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    ic_set_matching_braces("()<>[]");
    EXPECT_STREQ(ic_env_get_match_braces(env), "()<>[]",
                 "even-length matching brace string should be stored as-is");

    ic_set_matching_braces("(");
    EXPECT_STREQ(ic_env_get_match_braces(env), "()[]{}",
                 "odd-length matching brace string should fall back to default");

    ic_set_matching_braces(NULL);
    EXPECT_STREQ(ic_env_get_match_braces(env), "()[]{}",
                 "NULL matching brace string should fall back to default");

    ic_set_insertion_braces("{}\"\"");
    EXPECT_STREQ(ic_env_get_auto_braces(env), "{}\"\"",
                 "even-length insertion brace string should be stored as-is");

    ic_set_insertion_braces("{");
    EXPECT_STREQ(ic_env_get_auto_braces(env), "()[]{}\"\"''",
                 "odd-length insertion brace string should fall back to default");

    ic_set_insertion_braces(NULL);
    EXPECT_STREQ(ic_env_get_auto_braces(env), "()[]{}\"\"''",
                 "NULL insertion brace string should fall back to default");

    return true;
}

static bool test_abbreviation_management(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    ic_clear_abbreviations();
    EXPECT_TRUE(env->abbreviation_count == 0, "clear abbreviations should reset entry count");

    EXPECT_FALSE(ic_add_abbreviation(NULL, "noop"), "NULL trigger should be rejected");
    EXPECT_FALSE(ic_add_abbreviation("", "noop"), "empty trigger should be rejected");
    EXPECT_FALSE(ic_add_abbreviation("bad key", "noop"),
                 "whitespace in trigger should be rejected");
    EXPECT_FALSE(ic_add_abbreviation("ok", NULL), "NULL expansion should be rejected");

    EXPECT_TRUE(ic_add_abbreviation("gc", "git commit"), "valid abbreviation should be inserted");
    EXPECT_TRUE(env->abbreviation_count == 1, "abbreviation count should increase after insertion");
    EXPECT_STREQ(env->abbreviations[0].trigger, "gc", "trigger should be copied into env storage");
    EXPECT_STREQ(env->abbreviations[0].expansion, "git commit",
                 "expansion should be copied into env storage");

    EXPECT_TRUE(ic_add_abbreviation("gc", "git commit --verbose"),
                "adding existing trigger should update expansion in-place");
    EXPECT_TRUE(env->abbreviation_count == 1,
                "updating an existing trigger should not duplicate entries");
    EXPECT_STREQ(env->abbreviations[0].expansion, "git commit --verbose",
                 "existing expansion should be replaced when trigger already exists");

    EXPECT_FALSE(ic_remove_abbreviation("missing"),
                 "removing unknown trigger should report failure");
    EXPECT_TRUE(ic_remove_abbreviation("gc"), "removing existing trigger should succeed");
    EXPECT_TRUE(env->abbreviation_count == 0,
                "abbreviation count should decrease after successful removal");

    EXPECT_TRUE(ic_add_abbreviation("ga", "git add"), "should allow adding first abbreviation");
    EXPECT_TRUE(ic_add_abbreviation("gs", "git status"), "should allow adding second abbreviation");
    ic_clear_abbreviations();
    EXPECT_TRUE(env->abbreviation_count == 0 && env->abbreviation_capacity == 0,
                "clear should reset abbreviation storage metadata");
    EXPECT_TRUE(env->abbreviations == NULL, "clear should release abbreviation array");

    return true;
}

static bool test_key_spec_parse_and_format_roundtrip(void) {
    ic_keycode_t key = IC_KEY_NONE;
    EXPECT_TRUE(ic_parse_key_spec("ctrl+k", &key), "ctrl+k key spec should parse");
    EXPECT_TRUE(key == IC_KEY_CTRL_K, "ctrl+k should map to ASCII control keycode");

    EXPECT_TRUE(ic_parse_key_spec("alt+shift+f12", &key),
                "combined modifier function-key spec should parse");
    EXPECT_TRUE(key == IC_KEY_WITH_ALT(IC_KEY_WITH_SHIFT(IC_KEY_F12)),
                "combined modifiers should be encoded in keycode flags");

    EXPECT_TRUE(ic_parse_key_spec("ctrl+@", &key), "ctrl+@ should parse as ctrl-space shortcut");
    EXPECT_TRUE(key == IC_KEY_CTRL_SPACE, "ctrl+@ should map to ctrl-space keycode");

    EXPECT_FALSE(ic_parse_key_spec("ctrl+alt", &key), "modifier-only key spec should be rejected");
    EXPECT_FALSE(ic_parse_key_spec("definitely-not-a-key", &key),
                 "unknown key token should be rejected");

    char formatted[64];
    EXPECT_TRUE(ic_format_key_spec(IC_KEY_CTRL_K, formatted, sizeof(formatted)),
                "formatting ctrl-k should succeed");
    EXPECT_STREQ(formatted, "ctrl+k", "formatted ctrl-k key spec should be canonicalized");

    EXPECT_TRUE(ic_format_key_spec(IC_KEY_WITH_ALT(IC_KEY_F12), formatted, sizeof(formatted)),
                "formatting alt+f12 should succeed");
    EXPECT_STREQ(formatted, "alt+f12", "formatted alt+f12 key spec should be canonicalized");

    EXPECT_TRUE(ic_format_key_spec(IC_KEY_NONE, formatted, sizeof(formatted)),
                "formatting IC_KEY_NONE should succeed");
    EXPECT_STREQ(formatted, "none", "none keycode should format to literal 'none'");

    char tiny[4];
    EXPECT_FALSE(ic_format_key_spec(IC_KEY_WITH_ALT(IC_KEY_F12), tiny, sizeof(tiny)),
                 "format should fail when output buffer is too small");

    return true;
}

static bool test_key_binding_crud_and_profiles(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    const char* original_profile = ic_get_key_binding_profile();
    EXPECT_TRUE(ic_set_key_binding_profile("emacs"), "switching to emacs profile should succeed");

    EXPECT_TRUE(ic_bind_key(IC_KEY_F2, IC_KEY_ACTION_CLEAR_SCREEN),
                "binding explicit key/action should succeed");
    ic_key_action_t action = IC_KEY_ACTION_NONE;
    EXPECT_TRUE(ic_get_key_binding(IC_KEY_F2, &action), "new key binding should be queryable");
    EXPECT_TRUE(action == IC_KEY_ACTION_CLEAR_SCREEN,
                "queried key binding action should match bound value");

    size_t listed_count = ic_list_key_bindings(NULL, 0);
    EXPECT_TRUE(listed_count >= 1, "binding list count should reflect at least one binding");

    ic_key_binding_entry_t entries[64];
    size_t written = ic_list_key_bindings(entries, 64);
    EXPECT_TRUE(written > 0 && written <= listed_count,
                "binding listing with buffer should write bounded number of entries");

    EXPECT_TRUE(ic_clear_key_binding(IC_KEY_F2), "clearing existing binding should succeed");
    EXPECT_FALSE(ic_clear_key_binding(IC_KEY_F2),
                 "clearing same binding twice should fail on second attempt");
    EXPECT_FALSE(ic_bind_key(IC_KEY_F3, IC_KEY_ACTION__MAX),
                 "binding invalid action enum should fail");

    EXPECT_TRUE(ic_bind_key_named("ctrl+x", "undo"),
                "binding named action should accept known aliases");
    EXPECT_TRUE(ic_get_key_binding(IC_KEY_CTRL_X, &action),
                "named binding should resolve to parsed keycode");
    EXPECT_TRUE(action == IC_KEY_ACTION_UNDO,
                "named binding should map alias to expected action enum");

    size_t profile_count = ic_list_key_binding_profiles(NULL, 0);
    EXPECT_TRUE(profile_count >= 2, "at least emacs and vim profiles should be registered");

    ic_key_binding_profile_info_t profiles[8];
    size_t profile_written = ic_list_key_binding_profiles(profiles, 8);
    bool saw_emacs = false;
    bool saw_vim = false;
    for (size_t i = 0; i < profile_written; ++i) {
        if (profiles[i].name == NULL)
            continue;
        if (strcmp(profiles[i].name, "emacs") == 0)
            saw_emacs = true;
        if (strcmp(profiles[i].name, "vim") == 0)
            saw_vim = true;
    }
    EXPECT_TRUE(saw_emacs && saw_vim, "profile listing should expose both emacs and vim profiles");

    EXPECT_TRUE(ic_set_key_binding_profile("vim"), "switching to vim profile should succeed");
    EXPECT_STREQ(ic_get_key_binding_profile(), "vim",
                 "getter should report newly selected profile");

    const char* default_specs =
        ic_key_binding_profile_default_specs(IC_KEY_ACTION_CURSOR_WORD_NEXT_OR_COMPLETE);
    EXPECT_TRUE(default_specs != NULL && strstr(default_specs, "alt+w") != NULL,
                "vim profile should override cursor-word-next default specs with alt+w");

    EXPECT_FALSE(ic_set_key_binding_profile("does-not-exist"),
                 "unknown key binding profile should be rejected");

    EXPECT_TRUE(ic_set_key_binding_profile(original_profile),
                "restoring original key binding profile should succeed");
    return true;
}

static bool test_key_action_name_mappings(void) {
    EXPECT_TRUE(ic_key_action_from_name("history-up") == IC_KEY_ACTION_HISTORY_PREV,
                "history-up alias should map to HISTORY_PREV action");
    EXPECT_TRUE(ic_key_action_from_name("completion") == IC_KEY_ACTION_COMPLETE,
                "completion alias should map to COMPLETE action");
    EXPECT_TRUE(ic_key_action_from_name("unhandled") == IC_KEY_ACTION_RUNOFF,
                "unhandled alias should map to RUNOFF action");
    EXPECT_TRUE(ic_key_action_from_name("unknown-action") == IC_KEY_ACTION__MAX,
                "unknown action name should map to sentinel");

    EXPECT_STREQ(ic_key_action_name(IC_KEY_ACTION_CLEAR_SCREEN), "clear-screen",
                 "action-to-name lookup should return canonical clear-screen label");
    EXPECT_TRUE(ic_key_action_name(IC_KEY_ACTION__MAX) == NULL,
                "invalid action enum should not produce a name");

    return true;
}

static bool test_string_matching_and_token_helpers(void) {
    EXPECT_TRUE(ic_starts_with("prefix-value", "pre"),
                "starts_with should match explicit ASCII prefixes");
    EXPECT_FALSE(ic_starts_with("prefix-value", "Prefix"), "starts_with should be case-sensitive");
    EXPECT_TRUE(ic_starts_with("anything", NULL), "NULL prefix should be treated as empty prefix");
    EXPECT_FALSE(ic_starts_with(NULL, "x"),
                 "NULL source string should never match non-NULL prefix");

    EXPECT_TRUE(ic_istarts_with("Prefix-Value", "pre"),
                "istarts_with should match ASCII prefixes case-insensitively");
    EXPECT_FALSE(ic_istarts_with(NULL, "pre"),
                 "istarts_with should fail when source string is NULL");

    EXPECT_TRUE(ic_char_is_white(" ", 1), "space should classify as whitespace");
    EXPECT_TRUE(ic_char_is_nonwhite("x", 1), "regular letters should classify as non-whitespace");
    EXPECT_TRUE(ic_char_is_separator(";", 1), "semicolon should classify as separator");
    EXPECT_TRUE(ic_char_is_nonseparator("a", 1), "letters should classify as non-separators");
    EXPECT_TRUE(ic_char_is_digit("9", 1), "digit classification should recognize decimal digits");
    EXPECT_TRUE(ic_char_is_hexdigit("F", 1),
                "hexdigit classification should recognize uppercase hexadecimal digits");
    EXPECT_TRUE(ic_char_is_letter("Z", 1), "letter classification should recognize ASCII letters");
    EXPECT_TRUE(ic_char_is_idletter("_", 1), "idletter classification should include underscore");
    EXPECT_TRUE(ic_char_is_filename_letter(".", 1),
                "filename letter classification should include period character");

    const char* sample = "func test";
    EXPECT_TRUE(ic_is_token(sample, 0, &ic_char_is_letter) == 4,
                "token detector should report token length at token boundaries");
    EXPECT_TRUE(ic_is_token(sample, 1, &ic_char_is_letter) == -1,
                "token detector should reject offsets inside an existing token");
    EXPECT_TRUE(ic_match_token(sample, 0, &ic_char_is_letter, "func") == 4,
                "match_token should return token length on exact match");
    EXPECT_TRUE(ic_match_token(sample, 0, &ic_char_is_letter, "fun") == 0,
                "match_token should reject strict prefix matches");

    const char* tokens[] = {"foo", "func", NULL};
    EXPECT_TRUE(ic_match_any_token(sample, 0, &ic_char_is_letter, tokens) == 4,
                "match_any_token should return matching token length when any candidate matches");
    EXPECT_TRUE(ic_match_any_token(sample, 5, &ic_char_is_letter, tokens) == 0,
                "match_any_token should return zero when no candidate matches");

    return true;
}

static bool test_prev_next_char_utf8_helpers(void) {
    const char sample[] = {'a', (char)0xE2, (char)0x82, (char)0xAC, 'b', 0};

    EXPECT_TRUE(ic_next_char(sample, 0) == 1, "next_char should advance one byte over ASCII");
    EXPECT_TRUE(ic_next_char(sample, 1) == 4,
                "next_char should advance over complete multi-byte UTF-8 sequence");
    EXPECT_TRUE(ic_next_char(sample, 4) == 5, "next_char should advance over trailing ASCII");

    EXPECT_TRUE(ic_prev_char(sample, 5) == 4, "prev_char should step back one byte over ASCII");
    EXPECT_TRUE(ic_prev_char(sample, 4) == 1,
                "prev_char should step back over complete multi-byte UTF-8 sequence");
    EXPECT_TRUE(ic_prev_char(sample, 1) == 0, "prev_char should step back to start of buffer");

    EXPECT_TRUE(ic_next_char(sample, 5) == -1, "next_char should fail at end-of-string cursor");
    EXPECT_TRUE(ic_prev_char(sample, 0) == -1, "prev_char should fail at start-of-string cursor");
    EXPECT_TRUE(ic_next_char(NULL, 0) == -1, "next_char should reject NULL strings");
    EXPECT_TRUE(ic_prev_char(NULL, 0) == -1, "prev_char should reject NULL strings");

    return true;
}

static bool test_term_visibility_tracking_with_escape_and_control_bytes(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL || env->term == NULL)
        return false;

    term_reset_line_state(env->term);
    term_set_track_output(env->term, true);

    const char* sgr_only = "\x1B[31m";
    term_write_n(env->term, sgr_only, (ssize_t)strlen(sgr_only));
    EXPECT_FALSE(term_line_has_visible_content(env->term),
                 "ANSI SGR escape bytes alone should not mark line as visibly non-empty");

    const char ignored_controls[] = {'\x01', '\x02', '\x06'};
    term_write_n(env->term, ignored_controls, (ssize_t)sizeof(ignored_controls));
    EXPECT_FALSE(term_line_has_visible_content(env->term),
                 "ignored control bytes should not mark line as visibly non-empty");

    term_write_n(env->term, "\t", 1);
    EXPECT_TRUE(term_line_has_visible_content(env->term),
                "tab should count as visible line content while tracking output");

    term_write_n(env->term, "\n", 1);
    EXPECT_FALSE(term_line_has_visible_content(env->term),
                 "newline should reset tracked visible-content state for the line");

    term_set_track_output(env->term, false);
    term_reset_line_state(env->term);
    return true;
}

static bool test_key_spec_ctrl_space_variants(void) {
    ic_keycode_t key = IC_KEY_NONE;
    EXPECT_TRUE(ic_parse_key_spec("ctrl+space", &key), "ctrl+space key spec should parse");
    EXPECT_TRUE(key == IC_KEY_CTRL_SPACE,
                "ctrl+space should map to dedicated ctrl-space keycode constant");

    EXPECT_TRUE(ic_parse_key_spec("ctrl+@", &key), "ctrl+@ key spec should parse");
    EXPECT_TRUE(key == IC_KEY_CTRL_SPACE, "ctrl+@ should map to the same keycode as ctrl+space");

    char formatted[64];
    EXPECT_TRUE(ic_format_key_spec(IC_KEY_CTRL_SPACE, formatted, sizeof(formatted)),
                "formatting ctrl-space keycode should succeed");
    EXPECT_STREQ(formatted, "ctrl+space",
                 "ctrl-space keycode should format to canonical ctrl+space name");

    EXPECT_TRUE(ic_parse_key_spec("ctrl+tab", &key), "ctrl+tab key spec should parse");
    EXPECT_TRUE(ic_format_key_spec(key, formatted, sizeof(formatted)),
                "formatting ctrl+tab keycode should succeed");
    EXPECT_STREQ(formatted, "ctrl+tab",
                 "ctrl+tab should preserve explicit named key in formatted output");

    return true;
}

static bool test_initial_input_env_lifecycle(void) {
    ic_env_t* env = ensure_env();
    if (env == NULL)
        return false;

    ic_env_clear_initial_input(env);
    EXPECT_TRUE(env->initial_input == NULL,
                "initial input should start cleared for lifecycle test");

    char mutable_seed[] = "echo seeded";
    ic_env_set_initial_input(env, mutable_seed);
    EXPECT_TRUE(env->initial_input != NULL, "setting initial input should allocate env storage");
    EXPECT_STREQ(env->initial_input, "echo seeded",
                 "initial input setter should preserve original text");

    mutable_seed[0] = 'X';
    EXPECT_STREQ(env->initial_input, "echo seeded",
                 "initial input should be copied, not aliased to caller memory");

    ic_env_set_initial_input(env, "printf seeded");
    EXPECT_STREQ(env->initial_input, "printf seeded",
                 "setting initial input again should replace previous stored value");

    ic_env_clear_initial_input(env);
    EXPECT_TRUE(env->initial_input == NULL, "clearing initial input should reset pointer to NULL");

    return true;
}

static bool test_unicode_display_width_control_codepoints(void) {
    static const uint8_t control_mix[] = {'A', 0x01, 0x7F, 0x09, 'B'};
    size_t ansi_chars = 0;
    size_t visible_chars = 0;
    size_t display_width = unicode_calculate_display_width(
        (const char*)control_mix, sizeof(control_mix), &ansi_chars, &visible_chars);

    EXPECT_TRUE(display_width == 2,
                "control bytes should have zero width while printable bytes keep normal width");
    EXPECT_TRUE(ansi_chars == 0, "plain control bytes should not be counted as ANSI escape bytes");
    EXPECT_TRUE(visible_chars == 2,
                "visible char count should exclude zero-width control codepoints");

    size_t utf8_width = unicode_calculate_utf8_width((const char*)control_mix, sizeof(control_mix));
    EXPECT_TRUE(utf8_width == 2,
                "utf8 width helper should also treat C0/C1 controls as zero-width characters");

    return true;
}

typedef bool (*test_fn_t)(void);

typedef struct test_case_s {
    const char* name;
    test_fn_t fn;
} test_case_t;

static const test_case_t kTests[] = {
    {"multiline_toggle", test_multiline_toggle},
    {"line_number_modes", test_line_number_modes},
    {"line_number_continuation_prompt_toggle", test_line_number_continuation_prompt_toggle},
    {"line_number_prompt_replacement_toggle", test_line_number_prompt_replacement_toggle},
    {"prompt_line_replacement_requires_content", test_prompt_line_replacement_requires_content},
    {"visible_whitespace_marker", test_visible_whitespace_marker},
    {"prompt_cleanup_modes", test_prompt_cleanup_modes},
    {"multiline_start_line_count_clamp", test_multiline_start_line_count_clamp},
    {"editline_buffer_api_without_editor", test_editline_buffer_api_without_editor},
    {"continuation_callback_registration", test_continuation_callback_registration},
    {"completion_generation_and_apply", test_completion_generation_and_apply},
    {"history_dedup_snapshot", test_history_dedup_snapshot},
    {"history_fuzzy_case_toggle", test_history_fuzzy_case_toggle},
    {"history_fuzzy_case_toggle_via_api", test_history_fuzzy_case_toggle_via_api},
    {"line_wrapping_calculations", test_line_wrapping_calculations},
    {"unicode_decode_utf8_valid_sequences", test_unicode_decode_utf8_valid_sequences},
    {"unicode_decode_utf8_invalid_sequences", test_unicode_decode_utf8_invalid_sequences},
    {"unicode_encode_utf8_roundtrip", test_unicode_encode_utf8_roundtrip},
    {"unicode_qutf8_raw_byte_roundtrip", test_unicode_qutf8_raw_byte_roundtrip},
    {"unicode_width_calculation_with_invalid_and_ansi",
     test_unicode_width_calculation_with_invalid_and_ansi},
    {"str_next_ofs_with_utf8_and_escape_sequences",
     test_str_next_ofs_with_utf8_and_escape_sequences},
    {"stringbuf_utf8_navigation_and_deletion", test_stringbuf_utf8_navigation_and_deletion},
    {"push_raw_input_preconditions", test_push_raw_input_preconditions},
    {"tty_character_pushback_capacity_guard", test_tty_character_pushback_capacity_guard},
    {"push_raw_input_null_pointer_rejected", test_push_raw_input_null_pointer_rejected},
    {"prompt_marker_roundtrip", test_prompt_marker_roundtrip},
    {"hint_delay_clamps", test_hint_delay_clamps},
    {"status_hint_mode_validation", test_status_hint_mode_validation},
    {"option_toggle_consistency", test_option_toggle_consistency},
    {"prompt_cleanup_newline_and_getters", test_prompt_cleanup_newline_and_getters},
    {"brace_pair_setters_validation", test_brace_pair_setters_validation},
    {"abbreviation_management", test_abbreviation_management},
    {"key_spec_parse_and_format_roundtrip", test_key_spec_parse_and_format_roundtrip},
    {"key_binding_crud_and_profiles", test_key_binding_crud_and_profiles},
    {"key_action_name_mappings", test_key_action_name_mappings},
    {"string_matching_and_token_helpers", test_string_matching_and_token_helpers},
    {"prev_next_char_utf8_helpers", test_prev_next_char_utf8_helpers},
    {"term_visibility_tracking_with_escape_and_control_bytes",
     test_term_visibility_tracking_with_escape_and_control_bytes},
    {"key_spec_ctrl_space_variants", test_key_spec_ctrl_space_variants},
    {"initial_input_env_lifecycle", test_initial_input_env_lifecycle},
    {"unicode_display_width_control_codepoints", test_unicode_display_width_control_codepoints},
};

int main(void) {
    size_t failures = 0;
    const size_t test_count = sizeof(kTests) / sizeof(kTests[0]);

    for (size_t i = 0; i < test_count; ++i) {
        if (!kTests[i].fn()) {
            expect_safe_log("Test '%s' failed\n", kTests[i].name);
            failures += 1;
        }
    }

    if (failures > 0) {
        expect_safe_log("%zu/%zu isocline behavior tests failed\n", failures, test_count);
        return 1;
    }

    (void)printf("All %zu isocline behavior tests passed\n", test_count);
    return 0;
}
