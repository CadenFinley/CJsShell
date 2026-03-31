/*
  test_completions.cpp

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

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "builtins_completions_handler.h"
#include "cjsh_completions.h"
#include "command_line_utils.h"
#include "completion_spell.h"
#include "completion_tracker.h"
#include "completion_utils.h"
extern "C" {
#include "completions.h"
#include "env.h"
}

static void log_failure(const char* test_name, const char* message) {
    std::fprintf(stderr, "[FAIL] %s: %s\n", test_name, message);
}

#define EXPECT_TRUE(condition, test_name, message) \
    do {                                           \
        if (!(condition)) {                        \
            log_failure(test_name, message);       \
            return false;                          \
        }                                          \
    } while (0)

#define EXPECT_FALSE(condition, test_name, message) EXPECT_TRUE(!(condition), test_name, message)

static bool expect_streq(const std::string& actual, const std::string& expected,
                         const char* test_name, const char* message) {
    if (actual == expected) {
        return true;
    }
    std::fprintf(stderr, "[FAIL] %s: %s\n", test_name, message);
    std::fprintf(stderr, "  actual:   %s\n", actual.c_str());
    std::fprintf(stderr, "  expected: %s\n", expected.c_str());
    return false;
}

struct CompletionAction {
    std::string text;
    long delete_before;
    long delete_after;
    const char* source;
};

static std::vector<CompletionAction> g_completion_actions;
static const std::unordered_map<std::string, completion_spell::SpellCorrectionMatch>*
    g_spell_matches = nullptr;
static size_t g_spell_prefix_len = 0;

static void completion_action_completer(ic_completion_env_t* cenv, const char* prefix) {
    completion_tracker::completion_session_begin(cenv, prefix);
    for (const auto& action : g_completion_actions) {
        const char* source = action.source == nullptr ? "test" : action.source;
        completion_tracker::safe_add_completion_prim_with_source(
            cenv, action.text.c_str(), nullptr, nullptr, source, action.delete_before,
            action.delete_after);
    }
    completion_tracker::completion_session_end();
}

static void spell_match_completer(ic_completion_env_t* cenv, const char* prefix) {
    if (g_spell_matches == nullptr) {
        return;
    }
    completion_tracker::completion_session_begin(cenv, prefix);
    completion_spell::add_spell_correction_matches(cenv, *g_spell_matches, g_spell_prefix_len);
    completion_tracker::completion_session_end();
}

static ssize_t run_completion_generation(const char* input, ic_completer_fun_t* completer,
                                         ssize_t max_results) {
    ic_env_t* env = ic_get_env();
    if (env == nullptr || env->completions == nullptr) {
        return -1;
    }
    completions_set_completer(env->completions, completer, nullptr);
    return completions_generate(env, env->completions, input,
                                static_cast<ssize_t>(std::strlen(input)), max_results);
}

static bool test_quote_and_unquote_paths(void) {
    const char* test_name = "quote_and_unquote_paths";

    std::string plain = "simple";
    if (!expect_streq(completion_utils::quote_path_if_needed(plain), "simple", test_name,
                      "plain path should be unchanged")) {
        return false;
    }

    std::string spaced = "two words";
    if (!expect_streq(completion_utils::quote_path_if_needed(spaced), "\"two words\"", test_name,
                      "paths with spaces should be quoted")) {
        return false;
    }

    std::string with_quotes = "a\"b\\c";
    if (!expect_streq(completion_utils::quote_path_if_needed(with_quotes), "\"a\\\"b\\\\c\"",
                      test_name, "quotes and backslashes should be escaped")) {
        return false;
    }

    if (!expect_streq(completion_utils::unquote_path("\"two words\""), "two words", test_name,
                      "double-quoted path should be unquoted")) {
        return false;
    }

    if (!expect_streq(completion_utils::unquote_path("'a b'"), "a b", test_name,
                      "single-quoted path should be unquoted")) {
        return false;
    }

    if (!expect_streq(completion_utils::unquote_path("a\\ b"), "a b", test_name,
                      "escaped whitespace should be unescaped")) {
        return false;
    }

    return true;
}

static bool test_quote_path_special_characters(void) {
    const char* test_name = "quote_path_special_characters";
    std::string path = "one&two";
    if (!expect_streq(completion_utils::quote_path_if_needed(path), "\"one&two\"", test_name,
                      "paths with special characters should be quoted")) {
        return false;
    }
    return true;
}

static bool test_quote_path_empty_and_dollar(void) {
    const char* test_name = "quote_path_empty_and_dollar";
    if (!expect_streq(completion_utils::quote_path_if_needed(""), "", test_name,
                      "empty path should remain empty")) {
        return false;
    }

    std::string path = "cost$1";
    if (!expect_streq(completion_utils::quote_path_if_needed(path), "\"cost$1\"", test_name,
                      "paths containing shell metacharacters should be quoted")) {
        return false;
    }

    return true;
}

static bool test_unquote_path_with_escaped_quote(void) {
    const char* test_name = "unquote_path_with_escaped_quote";
    if (!expect_streq(completion_utils::unquote_path("\"a\\\"b\""), "a\"b", test_name,
                      "escaped quotes should be unescaped")) {
        return false;
    }
    return true;
}

static bool test_unquote_path_with_mixed_quote_segments(void) {
    const char* test_name = "unquote_path_with_mixed_quote_segments";
    if (!expect_streq(completion_utils::unquote_path("'left'\"right\"\\ middle"),
                      "leftright middle", test_name,
                      "single quotes, double quotes, and escaped spaces should combine")) {
        return false;
    }
    return true;
}

static bool test_tokenize_command_line(void) {
    const char* test_name = "tokenize_command_line";
    std::string line = "cmd \"arg with space\" 'single quoted' plain\\ space";
    auto tokens = completion_utils::tokenize_command_line(line);

    EXPECT_TRUE(tokens.size() == 4, test_name, "expected four tokens");
    EXPECT_TRUE(tokens[0] == "cmd", test_name, "first token should be command");
    EXPECT_TRUE(tokens[1] == "arg with space", test_name,
                "double-quoted token should preserve spaces");
    EXPECT_TRUE(tokens[2] == "single quoted", test_name,
                "single-quoted token should preserve spaces");
    EXPECT_TRUE(tokens[3] == "plain space", test_name, "escaped space should join token");
    return true;
}

static bool test_tokenize_command_line_escaped_quotes(void) {
    const char* test_name = "tokenize_command_line_escaped_quotes";
    std::string line = "cmd \"a\\\"b\" tail";
    auto tokens = completion_utils::tokenize_command_line(line);

    EXPECT_TRUE(tokens.size() == 3, test_name, "expected three tokens");
    EXPECT_TRUE(tokens[0] == "cmd", test_name, "first token should be command");
    EXPECT_TRUE(tokens[1] == "a\"b", test_name, "escaped quote should be preserved in token");
    EXPECT_TRUE(tokens[2] == "tail", test_name, "last token should be preserved");
    return true;
}

static bool test_tokenize_command_line_unterminated_quote(void) {
    const char* test_name = "tokenize_command_line_unterminated_quote";
    std::string line = "cmd \"unterminated quote tail";
    auto tokens = completion_utils::tokenize_command_line(line);

    EXPECT_TRUE(tokens.size() == 2, test_name,
                "unterminated quoted text should remain a single token");
    EXPECT_TRUE(tokens[0] == "cmd", test_name, "first token should be command");
    EXPECT_TRUE(tokens[1] == "unterminated quote tail", test_name,
                "quoted payload should preserve spaces even when quote is unmatched");
    return true;
}

static bool test_tokenize_shell_words_preserve_literals(void) {
    const char* test_name = "tokenize_shell_words_preserve_literals";
    std::string line = "cmd \"arg with space\" plain\\ space 'single quoted'";
    auto tokens = command_line_utils::tokenize_shell_words(line, true);

    EXPECT_TRUE(tokens.size() == 4, test_name, "expected four tokens");
    EXPECT_TRUE(tokens[0] == "cmd", test_name, "first token should be command");
    EXPECT_TRUE(tokens[1] == "\"arg with space\"", test_name,
                "double quotes should be preserved when requested");
    EXPECT_TRUE(tokens[2] == "plain\\ space", test_name,
                "escaped space should retain the escape sequence");
    EXPECT_TRUE(tokens[3] == "'single quoted'", test_name,
                "single quotes should be preserved when requested");
    return true;
}

static bool test_find_last_unquoted_space(void) {
    const char* test_name = "find_last_unquoted_space";
    std::string line = "echo \"a b\" c";
    size_t pos = completion_utils::find_last_unquoted_space(line);
    EXPECT_TRUE(pos == 10, test_name, "last unquoted space should be before final token");
    EXPECT_TRUE(completion_utils::find_last_unquoted_space("\"a b\"") == std::string::npos,
                test_name, "quoted spaces should be ignored");
    return true;
}

static bool test_find_last_unquoted_space_with_tabs(void) {
    const char* test_name = "find_last_unquoted_space_with_tabs";
    std::string line = "cmd\targ";
    size_t pos = completion_utils::find_last_unquoted_space(line);
    EXPECT_TRUE(pos == 3, test_name, "tab should count as a delimiter");
    return true;
}

static bool test_find_last_unquoted_space_with_escaped_space(void) {
    const char* test_name = "find_last_unquoted_space_with_escaped_space";
    std::string escaped_only = "escaped\\ space";
    EXPECT_TRUE(completion_utils::find_last_unquoted_space(escaped_only) == std::string::npos,
                test_name, "escaped whitespace should not be treated as a delimiter");

    std::string with_tail = "escaped\\ space tail";
    size_t expected = with_tail.find(" tail");
    EXPECT_TRUE(expected != std::string::npos, test_name, "expected reference delimiter");
    size_t pos = completion_utils::find_last_unquoted_space(with_tail);
    EXPECT_TRUE(pos == expected, test_name,
                "last unquoted delimiter should ignore escaped whitespace");
    return true;
}

static bool test_find_last_unquoted_space_with_unterminated_quote(void) {
    const char* test_name = "find_last_unquoted_space_with_unterminated_quote";
    std::string line = "cmd \"quoted words remain quoted";
    size_t pos = completion_utils::find_last_unquoted_space(line);
    EXPECT_TRUE(pos == 3, test_name,
                "spaces inside an unterminated quote should not count as delimiters");
    return true;
}

static bool test_case_sensitivity_helpers(void) {
    const char* test_name = "case_sensitivity_helpers";
    const bool original_setting = is_completion_case_sensitive();
    set_completion_case_sensitive(false);
    EXPECT_TRUE(completion_utils::matches_completion_prefix("Hello", "he"), test_name,
                "case-insensitive prefix should match");
    EXPECT_TRUE(completion_utils::equals_completion_token("FOO", "foo"), test_name,
                "case-insensitive token should match");

    set_completion_case_sensitive(true);
    EXPECT_FALSE(completion_utils::matches_completion_prefix("Hello", "he"), test_name,
                 "case-sensitive prefix should reject mismatch");
    EXPECT_FALSE(completion_utils::equals_completion_token("FOO", "foo"), test_name,
                 "case-sensitive token should reject mismatch");

    set_completion_case_sensitive(original_setting);
    return true;
}

static bool test_normalize_for_comparison(void) {
    const char* test_name = "normalize_for_comparison";
    const bool original_setting = is_completion_case_sensitive();
    set_completion_case_sensitive(false);
    if (!expect_streq(completion_utils::normalize_for_comparison("MiXeD"), "mixed", test_name,
                      "normalize should lower-case when case-insensitive")) {
        set_completion_case_sensitive(original_setting);
        return false;
    }

    set_completion_case_sensitive(true);
    if (!expect_streq(completion_utils::normalize_for_comparison("MiXeD"), "MiXeD", test_name,
                      "normalize should preserve case when case-sensitive")) {
        set_completion_case_sensitive(original_setting);
        return false;
    }

    set_completion_case_sensitive(original_setting);
    return true;
}

static bool test_starts_with_helpers(void) {
    const char* test_name = "starts_with_helpers";
    EXPECT_TRUE(completion_utils::starts_with_case_insensitive("Hello", "he"), test_name,
                "case-insensitive helper should match");
    EXPECT_TRUE(completion_utils::starts_with_case_insensitive("Hello", "HE"), test_name,
                "case-insensitive helper should match uppercase prefix");
    EXPECT_FALSE(completion_utils::starts_with_case_insensitive("Hello", "hi"), test_name,
                 "case-insensitive helper should reject mismatched prefix");

    EXPECT_TRUE(completion_utils::starts_with_case_sensitive("Hello", "He"), test_name,
                "case-sensitive helper should match exact prefix");
    EXPECT_FALSE(completion_utils::starts_with_case_sensitive("Hello", "he"), test_name,
                 "case-sensitive helper should reject mismatched case");
    EXPECT_FALSE(completion_utils::starts_with_case_sensitive("Hello", "HelloWorld"), test_name,
                 "case-sensitive helper should reject longer prefix");
    return true;
}

static bool test_sanitize_job_summary(void) {
    const char* test_name = "sanitize_job_summary";
    std::string raw = "  ls\t -la \n \x01";
    if (!expect_streq(completion_utils::sanitize_job_command_summary(raw), "ls -la", test_name,
                      "summary should normalize whitespace and strip control chars")) {
        return false;
    }
    return true;
}

static bool test_sanitize_job_summary_truncates(void) {
    const char* test_name = "sanitize_job_summary_truncates";
    std::string long_cmd(120, 'a');
    std::string summary = completion_utils::sanitize_job_command_summary(long_cmd);
    EXPECT_TRUE(summary.size() == 80, test_name, "summary should truncate to 80 characters");
    return true;
}

static bool test_sanitize_job_summary_whitespace_only(void) {
    const char* test_name = "sanitize_job_summary_whitespace_only";
    std::string raw = " \t \n \r\x01\x02";
    if (!expect_streq(completion_utils::sanitize_job_command_summary(raw), "", test_name,
                      "all whitespace/control input should sanitize to empty string")) {
        return false;
    }
    return true;
}

static bool test_spell_transposition_and_distance(void) {
    const char* test_name = "spell_transposition_and_distance";
    EXPECT_TRUE(completion_spell::is_adjacent_transposition("abcd", "abdc"), test_name,
                "adjacent transposition should be detected");
    EXPECT_FALSE(completion_spell::is_adjacent_transposition("abcd", "adbc"), test_name,
                 "non-adjacent swap should not be treated as transposition");

    EXPECT_TRUE(completion_spell::compute_edit_distance_with_limit("kitten", "sitting", 3) == 3,
                test_name, "edit distance should match expected value");
    EXPECT_TRUE(completion_spell::compute_edit_distance_with_limit("kitten", "sitting", 2) == 3,
                test_name, "edit distance should exceed limit and return max+1");

    EXPECT_FALSE(completion_spell::should_consider_spell_correction("a"), test_name,
                 "single-character prefix should not trigger spell correction");
    EXPECT_TRUE(completion_spell::should_consider_spell_correction("ab"), test_name,
                "two-character prefix should trigger spell correction");
    return true;
}

static bool test_spell_distance_negative_limit(void) {
    const char* test_name = "spell_distance_negative_limit";
    int distance = completion_spell::compute_edit_distance_with_limit("abc", "xyz", -1);
    EXPECT_TRUE(distance == std::numeric_limits<int>::max(), test_name,
                "negative max distance should return sentinel max value");
    return true;
}

static bool test_spell_match_ordering(void) {
    const char* test_name = "spell_match_ordering";
    std::unordered_map<std::string, completion_spell::SpellCorrectionMatch> matches;
    matches["alpha"] = {"alpha", 2, false, 2};
    matches["alhpa"] = {"alhpa", 1, true, 1};
    matches["alpah"] = {"alpah", 1, false, 3};
    matches["alps"] = {"alps", 1, false, 2};

    auto ordered = completion_spell::order_spell_correction_matches(matches);
    EXPECT_TRUE(ordered.size() == 4, test_name, "expected four spell matches");
    EXPECT_TRUE(ordered[0].candidate == "alhpa", test_name,
                "transposition match should rank ahead of other distance-1 matches");
    EXPECT_TRUE(ordered[1].candidate == "alpah", test_name,
                "shared prefix length should break distance ties");
    EXPECT_TRUE(ordered[2].candidate == "alps", test_name,
                "remaining distance-1 match should follow by shared prefix length");
    EXPECT_TRUE(ordered[3].candidate == "alpha", test_name,
                "higher distance match should rank last");
    return true;
}

static bool test_spell_match_add_limit(void) {
    const char* test_name = "spell_match_add_limit";
    std::unordered_map<std::string, completion_spell::SpellCorrectionMatch> matches;
    for (int i = 0; i < 20; ++i) {
        std::string name = "spell" + std::to_string(i);
        matches[name] = {name, 1, false, 1};
    }

    g_spell_matches = &matches;
    g_spell_prefix_len = 4;
    ssize_t count = run_completion_generation("spel", &spell_match_completer, 64);
    g_spell_matches = nullptr;
    g_spell_prefix_len = 0;

    EXPECT_TRUE(count == 10, test_name, "spell match insertion should cap at 10 entries");
    return true;
}

static bool test_collect_spell_candidates_filter_and_case_normalization(void) {
    const char* test_name = "collect_spell_candidates_filter_and_case_normalization";
    const bool original_setting = is_completion_case_sensitive();
    set_completion_case_sensitive(false);

    std::vector<std::string> candidates = {"Git", "gti", "grep"};
    std::unordered_map<std::string, completion_spell::SpellCorrectionMatch> matches;
    completion_spell::collect_spell_correction_candidates(
        candidates, [](const std::string& value) { return value; },
        [](const std::string& value) { return value != "grep"; }, "gti", matches);

    bool ok = true;
    if (matches.size() != 1) {
        log_failure(test_name, "filtering and exact-match skipping should leave one candidate");
        ok = false;
    }

    auto it = matches.find("Git");
    if (it == matches.end()) {
        log_failure(test_name, "case-insensitive normalized transposition should be collected");
        ok = false;
    } else {
        if (it->second.distance != 1) {
            log_failure(test_name, "transposition should use effective distance of 1");
            ok = false;
        }
        if (!it->second.is_transposition) {
            log_failure(test_name, "candidate should be marked as transposition");
            ok = false;
        }
        if (it->second.shared_prefix_len != 1) {
            log_failure(test_name, "shared prefix length should be computed on normalized text");
            ok = false;
        }
    }

    set_completion_case_sensitive(original_setting);
    return ok;
}

static bool test_collect_spell_candidates_distance_thresholds(void) {
    const char* test_name = "collect_spell_candidates_distance_thresholds";
    std::vector<std::string> candidates = {"abcxxx", "abxxxx", "abxyz"};

    std::unordered_map<std::string, completion_spell::SpellCorrectionMatch> long_prefix_matches;
    completion_spell::collect_spell_correction_candidates(
        candidates, [](const std::string& value) { return value; },
        [](const std::string&) { return true; }, "abcdef", long_prefix_matches);

    EXPECT_TRUE(long_prefix_matches.find("abcxxx") != long_prefix_matches.end(), test_name,
                "distance-3 candidate should be kept for longer prefixes");
    EXPECT_TRUE(long_prefix_matches.find("abxxxx") == long_prefix_matches.end(), test_name,
                "distance-4 candidate should be discarded for longer prefixes");

    std::unordered_map<std::string, completion_spell::SpellCorrectionMatch> short_prefix_matches;
    completion_spell::collect_spell_correction_candidates(
        candidates, [](const std::string& value) { return value; },
        [](const std::string&) { return true; }, "abcde", short_prefix_matches);

    EXPECT_TRUE(short_prefix_matches.find("abxyz") == short_prefix_matches.end(), test_name,
                "distance-3 candidate should be discarded for short prefixes");
    return true;
}

static bool test_collect_spell_candidates_without_filter(void) {
    const char* test_name = "collect_spell_candidates_without_filter";
    std::vector<std::string> candidates = {"gti"};
    std::unordered_map<std::string, completion_spell::SpellCorrectionMatch> matches;
    const std::function<bool(const std::string&)> no_filter;

    completion_spell::collect_spell_correction_candidates(
        candidates, [](const std::string& value) { return value; }, no_filter, "git", matches);

    EXPECT_TRUE(matches.find("gti") != matches.end(), test_name,
                "empty filter should behave as allow-all");
    return true;
}

static bool test_completion_tracker_deduplication(void) {
    const char* test_name = "completion_tracker_deduplication";
    g_completion_actions = {
        {"d", 1, 0, "test"},
        {"bd", 2, 0, "test"},
    };
    ssize_t count = run_completion_generation("abc", &completion_action_completer, 64);
    g_completion_actions.clear();

    EXPECT_TRUE(count == 1, test_name, "duplicate final result should only be added once");
    return true;
}

static bool test_completion_tracker_trims_trailing_spaces(void) {
    const char* test_name = "completion_tracker_trims_trailing_spaces";
    g_completion_actions = {
        {"arg ", 0, 0, "test"},
        {"arg", 0, 0, "test"},
    };
    ssize_t count = run_completion_generation("cmd ", &completion_action_completer, 64);
    g_completion_actions.clear();

    EXPECT_TRUE(count == 1, test_name, "canonicalized results should ignore trailing spaces");
    return true;
}

static bool test_completion_tracker_max_results(void) {
    const char* test_name = "completion_tracker_max_results";
    std::string error;
    EXPECT_FALSE(completion_tracker::set_completion_max_results(0, &error), test_name,
                 "setting max results below minimum should fail");
    EXPECT_TRUE(!error.empty(), test_name, "error message should be populated");

    long default_max = completion_tracker::get_completion_default_max_results();
    long min_allowed = completion_tracker::get_completion_min_allowed_results();
    EXPECT_TRUE(completion_tracker::set_completion_max_results(min_allowed, nullptr), test_name,
                "setting minimum max results should succeed");
    EXPECT_TRUE(completion_tracker::get_completion_max_results() == min_allowed, test_name,
                "configured max results should match requested value");

    g_completion_actions = {
        {"one", 0, 0, "test"},
        {"two", 0, 0, "test"},
        {"three", 0, 0, "test"},
    };
    ssize_t count = run_completion_generation("", &completion_action_completer, 64);
    g_completion_actions.clear();

    EXPECT_TRUE(count == min_allowed, test_name, "completion count should honor max results cap");

    completion_tracker::set_completion_max_results(default_max, nullptr);
    return true;
}

static bool test_completion_tracker_delete_before_bounds(void) {
    const char* test_name = "completion_tracker_delete_before_bounds";
    completion_tracker::CompletionTracker tracker(nullptr, "abc");

    if (!expect_streq(tracker.calculate_final_result("z", 2), "az", test_name,
                      "delete_before within bounds should trim from prefix")) {
        return false;
    }
    if (!expect_streq(tracker.calculate_final_result("z", 5), "abcz", test_name,
                      "delete_before beyond prefix length should leave prefix unchanged")) {
        return false;
    }
    if (!expect_streq(tracker.calculate_final_result("z", -1), "abcz", test_name,
                      "negative delete_before should leave prefix unchanged")) {
        return false;
    }

    tracker.added_completions.insert("abcz");
    EXPECT_TRUE(tracker.would_create_duplicate("z", 5), test_name,
                "out-of-range delete_before should still deduplicate canonical result");
    return true;
}

static bool has_entry(const builtin_completions::CommandDoc* doc, const std::string& text,
                      builtin_completions::EntryKind kind) {
    if (doc == nullptr) {
        return false;
    }
    for (const auto& entry : doc->entries) {
        if (entry.text == text && entry.kind == kind) {
            return true;
        }
    }
    return false;
}

static bool test_builtin_docs(void) {
    const char* test_name = "builtin_docs";

    const auto* cjsh_doc = builtin_completions::lookup_builtin_command_doc("cjsh");
    EXPECT_TRUE(cjsh_doc != nullptr, test_name, "cjsh doc should exist");
    EXPECT_TRUE(cjsh_doc->summary_present, test_name, "cjsh summary should be present");
    EXPECT_TRUE(has_entry(cjsh_doc, "--help", builtin_completions::EntryKind::Option), test_name,
                "cjsh doc should include --help option");

    const auto* hook_doc = builtin_completions::lookup_builtin_command_doc("hook");
    EXPECT_TRUE(hook_doc != nullptr, test_name, "hook doc should exist");
    EXPECT_TRUE(has_entry(hook_doc, "add", builtin_completions::EntryKind::Subcommand), test_name,
                "hook doc should include add subcommand");
    EXPECT_TRUE(has_entry(hook_doc, "remove", builtin_completions::EntryKind::Subcommand),
                test_name, "hook doc should include remove subcommand");
    EXPECT_TRUE(has_entry(hook_doc, "list", builtin_completions::EntryKind::Subcommand), test_name,
                "hook doc should include list subcommand");
    EXPECT_TRUE(has_entry(hook_doc, "clear", builtin_completions::EntryKind::Subcommand), test_name,
                "hook doc should include clear subcommand");

    const auto* abbreviate_doc = builtin_completions::lookup_builtin_command_doc("abbreviate");
    EXPECT_TRUE(abbreviate_doc != nullptr, test_name, "alias doc should be available");
    if (!expect_streq(abbreviate_doc->summary, "Manage interactive abbreviations", test_name,
                      "alias summary should match base command")) {
        return false;
    }

    const auto* generate_doc =
        builtin_completions::lookup_builtin_command_doc("generate-completions");
    EXPECT_TRUE(generate_doc != nullptr, test_name, "generate-completions doc should exist");
    EXPECT_TRUE(has_entry(generate_doc, "--no-force", builtin_completions::EntryKind::Option),
                test_name, "generate-completions should include --no-force");
    EXPECT_TRUE(has_entry(generate_doc, "--jobs", builtin_completions::EntryKind::Option),
                test_name, "generate-completions should include --jobs");
    EXPECT_TRUE(has_entry(generate_doc, "--subcommands", builtin_completions::EntryKind::Option),
                test_name, "generate-completions should include --subcommands");

    const auto* source_doc = builtin_completions::lookup_builtin_command_doc(".");
    EXPECT_TRUE(source_doc != nullptr, test_name, "dot alias doc should exist");
    EXPECT_TRUE(source_doc->summary == "Execute commands from a file in the current shell",
                test_name, "dot alias should share source summary");

    const auto* cjshopt_doc = builtin_completions::lookup_builtin_command_doc("cjshopt");
    EXPECT_TRUE(cjshopt_doc != nullptr, test_name, "cjshopt doc should exist");
    EXPECT_TRUE(
        has_entry(cjshopt_doc, "completion-case", builtin_completions::EntryKind::Subcommand),
        test_name, "cjshopt should include completion-case subcommand");

    const auto* completion_max_doc =
        builtin_completions::lookup_builtin_command_doc("cjshopt-set-completion-max");
    EXPECT_TRUE(completion_max_doc != nullptr, test_name,
                "cjshopt-set-completion-max doc should exist");
    EXPECT_TRUE(has_entry(completion_max_doc, "--status", builtin_completions::EntryKind::Option),
                test_name, "set-completion-max should include --status option");

    const auto* type_doc = builtin_completions::lookup_builtin_command_doc("type");
    EXPECT_TRUE(type_doc != nullptr, test_name, "type doc should exist");
    EXPECT_TRUE(has_entry(type_doc, "-a", builtin_completions::EntryKind::Option), test_name,
                "type should include -a option");

    const auto* jobname_doc = builtin_completions::lookup_builtin_command_doc("jobname");
    EXPECT_TRUE(jobname_doc != nullptr, test_name, "jobname doc should exist");
    EXPECT_TRUE(has_entry(jobname_doc, "--clear", builtin_completions::EntryKind::Option),
                test_name, "jobname should include --clear option");

    const auto* missing_doc =
        builtin_completions::lookup_builtin_command_doc("definitely-not-a-real-command");
    EXPECT_TRUE(missing_doc == nullptr, test_name,
                "lookup should return nullptr for unknown commands");

    return true;
}

typedef bool (*test_fn_t)(void);

typedef struct test_case_s {
    const char* name;
    test_fn_t fn;
} test_case_t;

static const test_case_t kTests[] = {
    {"quote_and_unquote_paths", test_quote_and_unquote_paths},
    {"quote_path_special_characters", test_quote_path_special_characters},
    {"quote_path_empty_and_dollar", test_quote_path_empty_and_dollar},
    {"unquote_path_with_escaped_quote", test_unquote_path_with_escaped_quote},
    {"unquote_path_with_mixed_quote_segments", test_unquote_path_with_mixed_quote_segments},
    {"tokenize_command_line", test_tokenize_command_line},
    {"tokenize_command_line_escaped_quotes", test_tokenize_command_line_escaped_quotes},
    {"tokenize_command_line_unterminated_quote", test_tokenize_command_line_unterminated_quote},
    {"tokenize_shell_words_preserve_literals", test_tokenize_shell_words_preserve_literals},
    {"find_last_unquoted_space", test_find_last_unquoted_space},
    {"find_last_unquoted_space_with_tabs", test_find_last_unquoted_space_with_tabs},
    {"find_last_unquoted_space_with_escaped_space",
     test_find_last_unquoted_space_with_escaped_space},
    {"find_last_unquoted_space_with_unterminated_quote",
     test_find_last_unquoted_space_with_unterminated_quote},
    {"case_sensitivity_helpers", test_case_sensitivity_helpers},
    {"normalize_for_comparison", test_normalize_for_comparison},
    {"starts_with_helpers", test_starts_with_helpers},
    {"sanitize_job_summary", test_sanitize_job_summary},
    {"sanitize_job_summary_truncates", test_sanitize_job_summary_truncates},
    {"sanitize_job_summary_whitespace_only", test_sanitize_job_summary_whitespace_only},
    {"spell_transposition_and_distance", test_spell_transposition_and_distance},
    {"spell_distance_negative_limit", test_spell_distance_negative_limit},
    {"spell_match_ordering", test_spell_match_ordering},
    {"spell_match_add_limit", test_spell_match_add_limit},
    {"collect_spell_candidates_filter_and_case_normalization",
     test_collect_spell_candidates_filter_and_case_normalization},
    {"collect_spell_candidates_distance_thresholds",
     test_collect_spell_candidates_distance_thresholds},
    {"collect_spell_candidates_without_filter", test_collect_spell_candidates_without_filter},
    {"completion_tracker_deduplication", test_completion_tracker_deduplication},
    {"completion_tracker_trims_trailing_spaces", test_completion_tracker_trims_trailing_spaces},
    {"completion_tracker_max_results", test_completion_tracker_max_results},
    {"completion_tracker_delete_before_bounds", test_completion_tracker_delete_before_bounds},
    {"builtin_docs", test_builtin_docs},
};

int main(void) {
    size_t failures = 0;
    const size_t test_count = sizeof(kTests) / sizeof(kTests[0]);

    for (size_t i = 0; i < test_count; ++i) {
        if (!kTests[i].fn()) {
            std::fprintf(stderr, "Test '%s' failed\n", kTests[i].name);
            failures += 1;
        }
    }

    if (failures > 0) {
        std::fprintf(stderr, "%zu/%zu completion tests failed\n", failures, test_count);
        return 1;
    }

    std::printf("All %zu completion tests passed\n", test_count);
    return 0;
}
