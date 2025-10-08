#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "function_evaluator.h"
#include "parser.h"
#include "variable_manager.h"

class ShellScriptInterpreter {
   public:
    // Special exit codes for control flow
    static constexpr int exit_break = 253;
    static constexpr int exit_continue = 254;
    static constexpr int exit_return = 255;
    static constexpr int exit_command_not_found = 127;

    ShellScriptInterpreter();
    ~ShellScriptInterpreter();

    ShellScriptInterpreter(const ShellScriptInterpreter&) = delete;
    ShellScriptInterpreter& operator=(const ShellScriptInterpreter&) = delete;
    ShellScriptInterpreter(ShellScriptInterpreter&&) = delete;
    ShellScriptInterpreter& operator=(ShellScriptInterpreter&&) = delete;

    void set_parser(Parser* parser) {
        this->shell_parser = parser;
    }

    int execute_block(const std::vector<std::string>& lines);
    std::vector<std::string> parse_into_lines(const std::string& script) {
        return shell_parser->parse_into_lines(script);
    }

    enum class ErrorSeverity : std::uint8_t {
        INFO = 0,
        WARNING = 1,
        ERROR = 2,
        CRITICAL = 3
    };

    enum class ErrorCategory : std::uint8_t {
        SYNTAX,
        CONTROL_FLOW,
        REDIRECTION,
        VARIABLES,
        COMMANDS,
        SEMANTICS,
        STYLE,
        PERFORMANCE
    };

    struct ErrorPosition {
        size_t line_number;
        size_t column_start;
        size_t column_end;
        size_t char_offset;
    };

    struct SyntaxError {
        ErrorPosition position;
        ErrorSeverity severity;
        ErrorCategory category;
        std::string error_code;
        std::string message;
        std::string line_content;
        std::string suggestion;
        std::vector<std::string> related_info;
        std::string documentation_url;

        SyntaxError(size_t line_num, const std::string& msg, const std::string& line_content)
            : position({line_num, 0, 0, 0}),
              severity(ErrorSeverity::ERROR),
              category(ErrorCategory::SYNTAX),
              error_code("SYN001"),
              message(msg),
              line_content(line_content) {
        }
        SyntaxError(ErrorPosition pos, ErrorSeverity sev, ErrorCategory cat,
                    const std::string& code, const std::string& msg,
                    const std::string& line_content = "", const std::string& suggestion = "")
            : position(pos),
              severity(sev),
              category(cat),
              error_code(code),
              message(msg),
              line_content(line_content),
              suggestion(suggestion) {
        }
    };

    std::vector<SyntaxError> validate_script_syntax(const std::vector<std::string>& lines);
    bool has_syntax_errors(const std::vector<std::string>& lines, bool print_errors = true);

    std::vector<SyntaxError> validate_comprehensive_syntax(const std::vector<std::string>& lines,
                                                           bool check_semantics = true,
                                                           bool check_style = false,
                                                           bool check_performance = false);

    std::vector<SyntaxError> validate_variable_usage(const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_command_existence(const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_redirection_syntax(const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_arithmetic_expressions(const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_parameter_expansions(const std::vector<std::string>& lines);

    std::vector<SyntaxError> analyze_control_flow(const std::vector<std::string>& lines);

    std::vector<SyntaxError> check_style_guidelines(const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_pipeline_syntax(const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_function_syntax(const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_loop_syntax(const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_conditional_syntax(const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_array_syntax(const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_heredoc_syntax(const std::vector<std::string>& lines);

    bool has_function(const std::string& name) const;
    std::vector<std::string> get_function_names() const;

    std::string expand_parameter_expression(const std::string& param_expr);

    std::string get_variable_value(const std::string& var_name);

    void push_function_scope();
    void pop_function_scope();
    void set_local_variable(const std::string& name, const std::string& value);
    bool is_local_variable(const std::string& name) const;

   private:
    Parser* shell_parser = nullptr;
    function_evaluator::FunctionMap functions;

    VariableManager variable_manager;

    bool variable_is_set(const std::string& var_name);
    bool matches_pattern(const std::string& text, const std::string& pattern);
    bool matches_char_class(char c, const std::string& char_class);
    int set_last_status(int code);
    int run_pipeline(const std::vector<Command>& cmds);

    struct CommandSubstitutionExpansion {
        std::string text;
        std::vector<std::string> outputs;
    };

    bool should_interpret_as_cjsh_script(const std::string& path) const;
    static std::optional<size_t> find_matching_paren(const std::string& text, size_t start_index);
    CommandSubstitutionExpansion expand_command_substitutions(const std::string& input) const;
    std::string simplify_parentheses_in_condition(
        const std::string& condition,
        const std::function<int(const std::string&)>& evaluator) const;
    int evaluate_logical_condition_internal(const std::string& condition,
                                            const std::function<int(const std::string&)>& executor);
    long long evaluate_arithmetic_expression(const std::string& expr);

    // Extracted methods from execute_block refactoring
    std::string expand_all_substitutions(const std::string& input,
                                         const std::function<int(const std::string&)>& executor);

    int execute_command_internal(const std::string& cmd_text, bool allow_semicolon_split,
                                 const std::function<int(const std::string&)>& executor);

    int process_theme_definition_block(const std::vector<std::string>& lines, size_t& line_index);

    int process_function_definition_line(const std::string& line,
                                         const std::vector<std::string>& lines, size_t& line_index,
                                         std::string& remaining_line);

    static bool is_control_flow_exit_code(int code);
    static bool should_skip_line(const std::string& line);

    struct BlockHandlerResult {
        bool handled;
        int exit_code;
        size_t next_line_index;
    };

    BlockHandlerResult try_dispatch_block_statement(
        const std::vector<std::string>& lines, size_t line_index, const std::string& line,
        const std::function<int(const std::vector<std::string>&, size_t&)>& handle_if_block,
        const std::function<int(const std::vector<std::string>&, size_t&)>& handle_for_block,
        const std::function<int(const std::vector<std::string>&, size_t&)>& handle_while_block,
        const std::function<int(const std::vector<std::string>&, size_t&)>& handle_until_block,
        const std::function<int(const std::vector<std::string>&, size_t&)>& handle_case_block);
};
