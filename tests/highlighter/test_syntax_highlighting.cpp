#include <cstdio>
#include <memory>
#include <string>

extern "C" {
#include "attr.h"
#include "bbcode.h"
#include "env.h"
#include "highlight.h"
#include "isocline.h"
}

#include "cjsh.h"
#include "cjsh_syntax_highlighter.h"
#include "shell.h"
#include "shell_env.h"
#include "token_constants.h"

std::unique_ptr<Shell> g_shell;
bool g_exit_flag = false;
bool g_startup_active = false;
bool g_force_exit_requested = false;
std::uint64_t g_command_sequence = 0;

extern "C" void syntax_highlight_bridge(ic_highlight_env_t* henv, const char* input, void* arg) {
    SyntaxHighlighter::highlight(henv, input, arg);
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

static ic_env_t* ensure_env(const char* test_name) {
    ic_env_t* env = ic_get_env();
    if (env == nullptr) {
        log_failure(test_name, "ic_get_env() returned nullptr");
    }
    return env;
}

static void ensure_style_definitions(void) {
    static bool initialized = false;
    if (initialized) {
        return;
    }

    for (const auto& pair : token_constants::default_styles()) {
        std::string style_name = pair.first;
        if (style_name.rfind("ic-", 0) != 0) {
            style_name = "cjsh-" + style_name;
        }
        ic_style_def(style_name.c_str(), pair.second.c_str());
    }

    initialized = true;
}

static attrbuf_t* highlight_input(const std::string& input, const char* test_name) {
    ic_env_t* env = ensure_env(test_name);
    if (env == nullptr) {
        return nullptr;
    }

    ensure_style_definitions();
    attrbuf_t* attrs = attrbuf_new(env->mem);
    if (attrs == nullptr) {
        log_failure(test_name, "attrbuf_new() returned nullptr");
        return nullptr;
    }

    highlight(env->mem, env->bbcode, input.c_str(), attrs, syntax_highlight_bridge, nullptr);
    return attrs;
}

static bool expect_style_range(attrbuf_t* attrs, bbcode_t* bbcode, size_t start, size_t length,
                               const char* style, const char* test_name, const char* message) {
    if (length == 0) {
        log_failure(test_name, "expected non-empty highlight range");
        return false;
    }
    attr_t expected = bbcode_style(bbcode, style);
    if (attr_is_none(expected)) {
        log_failure(test_name, "expected style not registered");
        return false;
    }

    for (size_t i = start; i < start + length; ++i) {
        attr_t actual = attrbuf_attr_at(attrs, static_cast<ssize_t>(i));
        if (!attr_is_eq(actual, expected)) {
            log_failure(test_name, message);
            return false;
        }
    }
    return true;
}

static bool test_variable_assignment_highlighting(void) {
    const char* test_name = "variable_assignment_highlighting";
    const std::string input = "FOO=42";
    attrbuf_t* attrs = highlight_input(input, test_name);
    if (attrs == nullptr) {
        return false;
    }

    ic_env_t* env = ensure_env(test_name);
    if (env == nullptr) {
        attrbuf_free(attrs);
        return false;
    }

    bool ok = expect_style_range(attrs, env->bbcode, 0, 3, "cjsh-variable", test_name,
                                 "FOO should be highlighted as variable") &&
              expect_style_range(attrs, env->bbcode, 3, 1, "cjsh-operator", test_name,
                                 "= should be highlighted as operator") &&
              expect_style_range(attrs, env->bbcode, 4, 2, "cjsh-number", test_name,
                                 "42 should be highlighted as number");

    attrbuf_free(attrs);
    return ok;
}

static bool test_comment_highlighting(void) {
    const char* test_name = "comment_highlighting";
    const std::string input = "echo hi # comment";
    attrbuf_t* attrs = highlight_input(input, test_name);
    if (attrs == nullptr) {
        return false;
    }

    ic_env_t* env = ensure_env(test_name);
    if (env == nullptr) {
        attrbuf_free(attrs);
        return false;
    }

    size_t start = input.find('#');
    if (start == std::string::npos) {
        log_failure(test_name, "failed to locate comment marker");
        attrbuf_free(attrs);
        return false;
    }
    size_t length = input.size() - start;
    bool ok = expect_style_range(attrs, env->bbcode, start, length, "cjsh-comment", test_name,
                                 "comment range should be highlighted");

    attrbuf_free(attrs);
    return ok;
}

static bool test_command_substitution_and_variable(void) {
    const char* test_name = "command_substitution_and_variable";
    const std::string input = "echo $(date) $USER";
    attrbuf_t* attrs = highlight_input(input, test_name);
    if (attrs == nullptr) {
        return false;
    }

    ic_env_t* env = ensure_env(test_name);
    if (env == nullptr) {
        attrbuf_free(attrs);
        return false;
    }

    size_t cmd_start = input.find("$(");
    size_t cmd_end = input.find(')', cmd_start);
    if (cmd_start == std::string::npos || cmd_end == std::string::npos || cmd_end < cmd_start) {
        log_failure(test_name, "failed to locate command substitution range");
        attrbuf_free(attrs);
        return false;
    }
    size_t cmd_length = (cmd_end == std::string::npos) ? 0 : (cmd_end - cmd_start + 1);

    size_t var_start = input.find("$USER");
    if (var_start == std::string::npos) {
        log_failure(test_name, "failed to locate $USER token");
        attrbuf_free(attrs);
        return false;
    }
    size_t var_length = std::string("$USER").size();

    bool ok =
        expect_style_range(attrs, env->bbcode, cmd_start, cmd_length, "cjsh-command-substitution",
                           test_name, "$(...) should be highlighted as command substitution") &&
        expect_style_range(attrs, env->bbcode, var_start, var_length, "cjsh-variable", test_name,
                           "$USER should be highlighted as variable");

    attrbuf_free(attrs);
    return ok;
}

static bool test_function_definition_highlighting(void) {
    const char* test_name = "function_definition_highlighting";
    const std::string input = "myfunc() { echo hi; }";
    attrbuf_t* attrs = highlight_input(input, test_name);
    if (attrs == nullptr) {
        return false;
    }

    ic_env_t* env = ensure_env(test_name);
    if (env == nullptr) {
        attrbuf_free(attrs);
        return false;
    }

    size_t brace_pos = input.find('{');
    if (brace_pos == std::string::npos) {
        log_failure(test_name, "failed to locate opening brace");
        attrbuf_free(attrs);
        return false;
    }
    bool ok = expect_style_range(attrs, env->bbcode, 0, 6, "cjsh-function-definition", test_name,
                                 "function name should be highlighted") &&
              expect_style_range(attrs, env->bbcode, 6, 2, "cjsh-function-definition", test_name,
                                 "function parentheses should be highlighted") &&
              expect_style_range(attrs, env->bbcode, brace_pos, 1, "cjsh-operator", test_name,
                                 "opening brace should be highlighted as operator");

    attrbuf_free(attrs);
    return ok;
}

static bool test_assignment_value_highlighting(void) {
    const char* test_name = "assignment_value_highlighting";
    const std::string input = "FOO=bar";
    attrbuf_t* attrs = highlight_input(input, test_name);
    if (attrs == nullptr) {
        return false;
    }

    ic_env_t* env = ensure_env(test_name);
    if (env == nullptr) {
        attrbuf_free(attrs);
        return false;
    }

    bool ok = expect_style_range(attrs, env->bbcode, 0, 3, "cjsh-variable", test_name,
                                 "FOO should be highlighted as variable") &&
              expect_style_range(attrs, env->bbcode, 3, 1, "cjsh-operator", test_name,
                                 "= should be highlighted as operator") &&
              expect_style_range(attrs, env->bbcode, 4, 3, "cjsh-assignment-value", test_name,
                                 "bar should be highlighted as assignment value");

    attrbuf_free(attrs);
    return ok;
}

static bool test_arithmetic_substitution_highlighting(void) {
    const char* test_name = "arithmetic_substitution_highlighting";
    const std::string input = "echo $((1 + 2))";
    attrbuf_t* attrs = highlight_input(input, test_name);
    if (attrs == nullptr) {
        return false;
    }

    ic_env_t* env = ensure_env(test_name);
    if (env == nullptr) {
        attrbuf_free(attrs);
        return false;
    }

    size_t start = input.find("$((");
    size_t end = input.rfind("))");
    if (start == std::string::npos || end == std::string::npos || end < start) {
        log_failure(test_name, "failed to locate arithmetic substitution range");
        attrbuf_free(attrs);
        return false;
    }
    size_t length = end - start + 2;

    bool ok = expect_style_range(attrs, env->bbcode, start, length, "cjsh-arithmetic", test_name,
                                 "arithmetic substitution should be highlighted");

    attrbuf_free(attrs);
    return ok;
}

static bool test_backtick_command_substitution_highlighting(void) {
    const char* test_name = "backtick_command_substitution_highlighting";
    const std::string input = "echo `date +%s`";
    attrbuf_t* attrs = highlight_input(input, test_name);
    if (attrs == nullptr) {
        return false;
    }

    ic_env_t* env = ensure_env(test_name);
    if (env == nullptr) {
        attrbuf_free(attrs);
        return false;
    }

    size_t start = input.find('`');
    size_t end = input.rfind('`');
    if (start == std::string::npos || end == std::string::npos || end <= start) {
        log_failure(test_name, "failed to locate backtick substitution range");
        attrbuf_free(attrs);
        return false;
    }
    size_t length = end - start + 1;

    bool ok = expect_style_range(attrs, env->bbcode, start, length, "cjsh-command-substitution",
                                 test_name, "backtick command substitution should be highlighted");

    attrbuf_free(attrs);
    return ok;
}

static bool test_history_expansion_highlighting(void) {
    const char* test_name = "history_expansion_highlighting";
    const std::string input = "echo !! && echo !$";
    attrbuf_t* attrs = highlight_input(input, test_name);
    if (attrs == nullptr) {
        return false;
    }

    ic_env_t* env = ensure_env(test_name);
    if (env == nullptr) {
        attrbuf_free(attrs);
        return false;
    }

    size_t bang_bang = input.find("!!");
    size_t bang_dollar = input.find("!$");
    if (bang_bang == std::string::npos || bang_dollar == std::string::npos) {
        log_failure(test_name, "failed to locate history expansion tokens");
        attrbuf_free(attrs);
        return false;
    }

    bool ok = expect_style_range(attrs, env->bbcode, bang_bang, 2, "cjsh-history-expansion",
                                 test_name, "!! should be highlighted as history expansion") &&
              expect_style_range(attrs, env->bbcode, bang_dollar, 2, "cjsh-history-expansion",
                                 test_name, "!$ should be highlighted as history expansion");

    attrbuf_free(attrs);
    return ok;
}

static bool test_operator_separator_highlighting(void) {
    const char* test_name = "operator_separator_highlighting";
    const std::string input = "echo ok && echo more || echo last";
    attrbuf_t* attrs = highlight_input(input, test_name);
    if (attrs == nullptr) {
        return false;
    }

    ic_env_t* env = ensure_env(test_name);
    if (env == nullptr) {
        attrbuf_free(attrs);
        return false;
    }

    size_t and_pos = input.find("&&");
    size_t or_pos = input.find("||");
    if (and_pos == std::string::npos || or_pos == std::string::npos) {
        log_failure(test_name, "failed to locate command separators");
        attrbuf_free(attrs);
        return false;
    }

    bool ok = expect_style_range(attrs, env->bbcode, and_pos, 2, "cjsh-operator", test_name,
                                 "&& should be highlighted as operator") &&
              expect_style_range(attrs, env->bbcode, or_pos, 2, "cjsh-operator", test_name,
                                 "|| should be highlighted as operator");

    attrbuf_free(attrs);
    return ok;
}

static bool test_option_glob_redirection_highlighting(void) {
    const char* test_name = "option_glob_redirection_highlighting";
    const std::string input = "ls -la *.cpp > out.txt";
    attrbuf_t* attrs = highlight_input(input, test_name);
    if (attrs == nullptr) {
        return false;
    }

    ic_env_t* env = ensure_env(test_name);
    if (env == nullptr) {
        attrbuf_free(attrs);
        return false;
    }

    size_t option_pos = input.find("-la");
    size_t glob_pos = input.find("*.cpp");
    size_t redir_pos = input.find("> ");
    if (option_pos == std::string::npos || glob_pos == std::string::npos ||
        redir_pos == std::string::npos) {
        log_failure(test_name, "failed to locate option/glob/redirection tokens");
        attrbuf_free(attrs);
        return false;
    }

    bool ok = expect_style_range(attrs, env->bbcode, option_pos, 3, "cjsh-option", test_name,
                                 "-la should be highlighted as option") &&
              expect_style_range(attrs, env->bbcode, glob_pos, 5, "cjsh-glob-pattern", test_name,
                                 "*.cpp should be highlighted as glob pattern") &&
              expect_style_range(attrs, env->bbcode, redir_pos, 1, "cjsh-operator", test_name,
                                 "> should be highlighted as operator");

    attrbuf_free(attrs);
    return ok;
}

static bool test_keyword_argument_highlighting(void) {
    const char* test_name = "keyword_argument_highlighting";
    const std::string input = "echo if then fi";
    attrbuf_t* attrs = highlight_input(input, test_name);
    if (attrs == nullptr) {
        return false;
    }

    ic_env_t* env = ensure_env(test_name);
    if (env == nullptr) {
        attrbuf_free(attrs);
        return false;
    }

    size_t if_pos = input.find("if");
    size_t then_pos = input.find("then");
    size_t fi_pos = input.rfind("fi");
    if (if_pos == std::string::npos || then_pos == std::string::npos ||
        fi_pos == std::string::npos) {
        log_failure(test_name, "failed to locate keyword tokens");
        attrbuf_free(attrs);
        return false;
    }

    bool ok = expect_style_range(attrs, env->bbcode, if_pos, 2, "cjsh-keyword", test_name,
                                 "if should be highlighted as keyword") &&
              expect_style_range(attrs, env->bbcode, then_pos, 4, "cjsh-keyword", test_name,
                                 "then should be highlighted as keyword") &&
              expect_style_range(attrs, env->bbcode, fi_pos, 2, "cjsh-keyword", test_name,
                                 "fi should be highlighted as keyword");

    attrbuf_free(attrs);
    return ok;
}

typedef bool (*test_fn_t)(void);

typedef struct test_case_s {
    const char* name;
    test_fn_t fn;
} test_case_t;

static const test_case_t kTests[] = {
    {"variable_assignment_highlighting", test_variable_assignment_highlighting},
    {"comment_highlighting", test_comment_highlighting},
    {"command_substitution_and_variable", test_command_substitution_and_variable},
    {"function_definition_highlighting", test_function_definition_highlighting},
    {"assignment_value_highlighting", test_assignment_value_highlighting},
    {"arithmetic_substitution_highlighting", test_arithmetic_substitution_highlighting},
    {"backtick_command_substitution_highlighting", test_backtick_command_substitution_highlighting},
    {"history_expansion_highlighting", test_history_expansion_highlighting},
    {"operator_separator_highlighting", test_operator_separator_highlighting},
    {"option_glob_redirection_highlighting", test_option_glob_redirection_highlighting},
    {"keyword_argument_highlighting", test_keyword_argument_highlighting},
};

int main(void) {
    g_shell = std::make_unique<Shell>();
    g_shell->set_interactive_mode(false);
    config::history_expansion_enabled = true;

    size_t failures = 0;
    const size_t test_count = sizeof(kTests) / sizeof(kTests[0]);

    for (size_t i = 0; i < test_count; ++i) {
        if (!kTests[i].fn()) {
            std::fprintf(stderr, "Test '%s' failed\n", kTests[i].name);
            failures += 1;
        }
    }

    if (failures > 0) {
        std::fprintf(stderr, "%zu/%zu syntax highlighting tests failed\n", failures, test_count);
        return 1;
    }

    std::printf("All %zu syntax highlighting tests passed\n", test_count);
    return 0;
}
