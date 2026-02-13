#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "builtins_completions_handler.h"
#include "completion_spell.h"
#include "completion_tracker.h"
#include "completion_utils.h"
extern "C" {
#include "completions.h"
#include "env.h"
}

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

static bool test_unquote_path_with_escaped_quote(void) {
    const char* test_name = "unquote_path_with_escaped_quote";
    if (!expect_streq(completion_utils::unquote_path("\"a\\\"b\""), "a\"b", test_name,
                      "escaped quotes should be unescaped")) {
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
    EXPECT_TRUE(completion_utils::find_last_unquoted_space("\"a b\"") == std::string::npos,
                test_name, "quoted spaces should be ignored");
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

static bool test_normalize_for_comparison(void) {
    const char* test_name = "normalize_for_comparison";
    g_completion_case_sensitive = false;
    if (!expect_streq(completion_utils::normalize_for_comparison("MiXeD"), "mixed", test_name,
                      "normalize should lower-case when case-insensitive")) {
        return false;
    }

    g_completion_case_sensitive = true;
    if (!expect_streq(completion_utils::normalize_for_comparison("MiXeD"), "MiXeD", test_name,
                      "normalize should preserve case when case-sensitive")) {
        return false;
    }

    g_completion_case_sensitive = false;
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
    {"unquote_path_with_escaped_quote", test_unquote_path_with_escaped_quote},
    {"tokenize_command_line", test_tokenize_command_line},
    {"find_last_unquoted_space", test_find_last_unquoted_space},
    {"case_sensitivity_helpers", test_case_sensitivity_helpers},
    {"normalize_for_comparison", test_normalize_for_comparison},
    {"starts_with_helpers", test_starts_with_helpers},
    {"sanitize_job_summary", test_sanitize_job_summary},
    {"spell_transposition_and_distance", test_spell_transposition_and_distance},
    {"spell_match_ordering", test_spell_match_ordering},
    {"spell_match_add_limit", test_spell_match_add_limit},
    {"completion_tracker_deduplication", test_completion_tracker_deduplication},
    {"completion_tracker_trims_trailing_spaces", test_completion_tracker_trims_trailing_spaces},
    {"completion_tracker_max_results", test_completion_tracker_max_results},
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
