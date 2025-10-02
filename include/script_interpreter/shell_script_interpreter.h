#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cjsh.h"

class Shell;
struct Command;

enum class DebugLevel {
    NONE = 0,
    BASIC = 1,
    VERBOSE = 2,
    TRACE = 3
};

class ShellScriptInterpreter {
   public:
    ShellScriptInterpreter();
    ~ShellScriptInterpreter();

    void set_debug_level(DebugLevel level);
    DebugLevel get_debug_level() const;
    void set_parser(Parser* parser) {
        this->shell_parser = parser;
    }

    int execute_block(const std::vector<std::string>& lines);
    std::vector<std::string> parse_into_lines(const std::string& script) {
        return shell_parser->parse_into_lines(script);
    }

    enum class ErrorSeverity {
        INFO = 0,     
        WARNING = 1,  
        ERROR = 2,    
        CRITICAL = 3  
    };

    enum class ErrorCategory {
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
        std::string
            error_code;  
        std::string message;       
        std::string line_content;  
        std::string suggestion;    
        std::vector<std::string>
            related_info;               
        std::string documentation_url;  

        SyntaxError(size_t line_num, const std::string& msg,
                    const std::string& line_content)
            : position({line_num, 0, 0, 0}),
              severity(ErrorSeverity::ERROR),
              category(ErrorCategory::SYNTAX),
              error_code("SYN001"),
              message(msg),
              line_content(line_content) {
        }
        SyntaxError(ErrorPosition pos, ErrorSeverity sev, ErrorCategory cat,
                    const std::string& code, const std::string& msg,
                    const std::string& line_content = "",
                    const std::string& suggestion = "")
            : position(pos),
              severity(sev),
              category(cat),
              error_code(code),
              message(msg),
              line_content(line_content),
              suggestion(suggestion) {
        }
    };

    std::vector<SyntaxError> validate_script_syntax(
        const std::vector<std::string>& lines);
    bool has_syntax_errors(const std::vector<std::string>& lines,
                           bool print_errors = true);

    std::vector<SyntaxError> validate_comprehensive_syntax(
        const std::vector<std::string>& lines, bool check_semantics = true,
        bool check_style = false, bool check_performance = false);

    std::vector<SyntaxError> validate_variable_usage(
        const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_command_existence(
        const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_redirection_syntax(
        const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_arithmetic_expressions(
        const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_parameter_expansions(
        const std::vector<std::string>& lines);

    std::vector<SyntaxError> analyze_control_flow(
        const std::vector<std::string>& lines);

    std::vector<SyntaxError> check_style_guidelines(
        const std::vector<std::string>& lines);

    
    std::vector<SyntaxError> validate_pipeline_syntax(
        const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_function_syntax(
        const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_loop_syntax(
        const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_conditional_syntax(
        const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_array_syntax(
        const std::vector<std::string>& lines);

    std::vector<SyntaxError> validate_heredoc_syntax(
        const std::vector<std::string>& lines);

    bool has_function(const std::string& name) const;
    std::vector<std::string> get_function_names() const;

    std::string expand_parameter_expression(const std::string& param_expr);

    std::string get_variable_value(const std::string& var_name);

    void push_function_scope();
    void pop_function_scope();
    void set_local_variable(const std::string& name, const std::string& value);
    bool is_local_variable(const std::string& name) const;

   private:
    DebugLevel debug_level;
    Parser* shell_parser = nullptr;
    std::unordered_map<std::string, std::vector<std::string>> functions;

    std::vector<std::unordered_map<std::string, std::string>>
        local_variable_stack;
    bool variable_is_set(const std::string& var_name);
    std::string pattern_match_prefix(const std::string& value,
                                     const std::string& pattern,
                                     bool longest = false);
    std::string pattern_match_suffix(const std::string& value,
                                     const std::string& pattern,
                                     bool longest = false);
    std::string pattern_substitute(const std::string& value,
                                   const std::string& replacement_expr,
                                   bool global = false);
    std::string case_convert(const std::string& value,
                             const std::string& pattern, bool uppercase,
                             bool all_chars);
    bool matches_pattern(const std::string& text, const std::string& pattern);
    bool matches_char_class(char c, const std::string& char_class);
    int set_last_status(int code);
    int run_pipeline(const std::vector<Command>& cmds);
};
