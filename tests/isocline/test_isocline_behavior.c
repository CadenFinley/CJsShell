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
    (void)prefix;
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
    {"completion_generation_and_apply", test_completion_generation_and_apply},
    {"history_dedup_snapshot", test_history_dedup_snapshot},
    {"line_wrapping_calculations", test_line_wrapping_calculations},
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
