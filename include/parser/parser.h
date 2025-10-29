#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Shell;
#include "parser/expansion_engine.h"
#include "parser/tokenizer.h"
#include "parser/variable_expander.h"

struct Command {
    std::vector<std::string> args;
    std::string input_file;
    std::string output_file;
    std::string append_file;
    std::string original_text;
    bool background = false;
    bool negate_pipeline = false;
    bool stderr_to_stdout = false;
    bool stdout_to_stderr = false;
    std::string stderr_file;
    bool stderr_append = false;
    std::string here_doc;
    std::string here_string;
    bool both_output = false;
    std::string both_output_file;
    bool force_overwrite = false;

    std::vector<std::pair<int, std::string>> fd_redirections;
    std::vector<std::pair<int, int>> fd_duplications;
    std::vector<std::string> process_substitutions;

    Command() {
        args.reserve(8);
        process_substitutions.reserve(2);
        fd_redirections.reserve(2);
        fd_duplications.reserve(2);
    }

    void set_fd_redirection(int fd, std::string value) {
        auto it = std::find_if(fd_redirections.begin(), fd_redirections.end(),
                               [fd](const auto& entry) { return entry.first == fd; });
        if (it != fd_redirections.end()) {
            it->second = std::move(value);
        } else {
            fd_redirections.emplace_back(fd, std::move(value));
        }
    }

    void set_fd_duplication(int fd, int target) {
        auto it = std::find_if(fd_duplications.begin(), fd_duplications.end(),
                               [fd](const auto& entry) { return entry.first == fd; });
        if (it != fd_duplications.end()) {
            it->second = target;
        } else {
            fd_duplications.emplace_back(fd, target);
        }
    }

    bool has_fd_redirection(int fd) const {
        return std::any_of(fd_redirections.begin(), fd_redirections.end(),
                           [fd](const auto& entry) { return entry.first == fd; });
    }

    bool has_fd_duplication(int fd) const {
        return std::any_of(fd_duplications.begin(), fd_duplications.end(),
                           [fd](const auto& entry) { return entry.first == fd; });
    }
};

struct LogicalCommand {
    std::string command;
    std::string op;
};

class Parser {
   public:
    std::vector<std::string> parse_into_lines(const std::string& scripts);

    bool should_validate_command(const std::string& command) const;
    bool is_valid_command(const std::string& command_name) const;
    std::string get_command_validation_error(const std::string& command_name) const;
    void set_command_validation_enabled(bool enabled) {
        command_validation_enabled = enabled;
    }
    bool get_command_validation_enabled() const {
        return command_validation_enabled;
    }

    std::vector<std::string> parse_command(const std::string& cmdline);
    std::vector<Command> parse_pipeline(const std::string& command);
    std::vector<std::string> expand_wildcards(const std::string& pattern);
    std::vector<LogicalCommand> parse_logical_commands(const std::string& command);
    std::vector<std::string> parse_semicolon_commands(const std::string& command,
                                                      bool split_on_newlines = false);
    bool is_env_assignment(const std::string& command, std::string& var_name,
                           std::string& var_value);
    void expand_env_vars(std::string& arg);
    void expand_env_vars_selective(std::string& arg);
    void expand_exported_env_vars_only(std::string& arg);
    std::vector<std::string> parse_command_exported_vars_only(const std::string& cmdline);
    std::vector<std::string> split_by_ifs(const std::string& input);
    long long evaluate_arithmetic(const std::string& expr);

    std::vector<Command> parse_pipeline_with_preprocessing(const std::string& command);

    void set_aliases(const std::unordered_map<std::string, std::string>& new_aliases) {
        this->aliases = new_aliases;
    }

    void set_env_vars(const std::unordered_map<std::string, std::string>& new_env_vars) {
        this->env_vars = new_env_vars;
    }

    void set_shell(Shell* shell);

   private:
    void ensure_parsers_initialized();
    bool is_control_word_at_position(const std::string& command, size_t i, 
                                     int paren_depth, int brace_depth, bool in_quotes,
                                     int& control_depth);
    void process_heredoc_content(std::string& content);
    bool handle_fd_redirection(const std::string& value, size_t& i, 
                              const std::vector<std::string>& tokens, Command& cmd,
                              std::vector<std::string>& filtered_args);

    std::unordered_map<std::string, std::string> aliases;
    std::unordered_map<std::string, std::string> env_vars;
    Shell* shell = nullptr;
    bool command_validation_enabled = true;

    std::map<std::string, std::string> current_here_docs;

    std::unique_ptr<Tokenizer> tokenizer;
    std::unique_ptr<VariableExpander> variableExpander;
    std::unique_ptr<ExpansionEngine> expansionEngine;
};
