#include <cstddef>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "builtins_completions_handler.h"
#include "completion_spell.h"
#include "completion_utils.h"

extern bool g_completion_case_sensitive;

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

static bool test_find_last_unquoted_space(void) {
    const char* test_name = "find_last_unquoted_space";
    std::string line = "echo \"a b\" c";
    size_t pos = completion_utils::find_last_unquoted_space(line);
    EXPECT_TRUE(pos == 10, test_name, "last unquoted space should be before final token");
    return true;
}

static bool test_case_sensitivity_helpers(void) {
    const char* test_name = "case_sensitivity_helpers";
    g_completion_case_sensitive = false;
    EXPECT_TRUE(completion_utils::matches_completion_prefix("Hello", "he"), test_name,
                "case-insensitive prefix should match");
    EXPECT_TRUE(completion_utils::equals_completion_token("FOO", "foo"), test_name,
                "case-insensitive token should match");

    g_completion_case_sensitive = true;
    EXPECT_FALSE(completion_utils::matches_completion_prefix("Hello", "he"), test_name,
                 "case-sensitive prefix should reject mismatch");
    EXPECT_FALSE(completion_utils::equals_completion_token("FOO", "foo"), test_name,
                 "case-sensitive token should reject mismatch");

    g_completion_case_sensitive = false;
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

    return true;
}

typedef bool (*test_fn_t)(void);

typedef struct test_case_s {
    const char* name;
    test_fn_t fn;
} test_case_t;

static const test_case_t kTests[] = {
    {"quote_and_unquote_paths", test_quote_and_unquote_paths},
    {"tokenize_command_line", test_tokenize_command_line},
    {"find_last_unquoted_space", test_find_last_unquoted_space},
    {"case_sensitivity_helpers", test_case_sensitivity_helpers},
    {"sanitize_job_summary", test_sanitize_job_summary},
    {"spell_transposition_and_distance", test_spell_transposition_and_distance},
    {"spell_match_ordering", test_spell_match_ordering},
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
