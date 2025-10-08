#pragma once

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

class Shell;

std::vector<std::string> tokenize_command(const std::string& cmdline);

struct Command {
    std::vector<std::string> args;
    std::string input_file;
    std::string output_file;
    std::string append_file;
    bool background = false;
    bool stderr_to_stdout = false;
    bool stdout_to_stderr = false;
    std::string stderr_file;
    bool stderr_append = false;
    std::string here_doc;
    std::string here_string;
    bool both_output = false;
    std::string both_output_file;
    bool force_overwrite = false;

    std::map<int, std::string> fd_redirections;
    std::map<int, int> fd_duplications;
    std::vector<std::string> process_substitutions;

    Command() {
        args.reserve(8);
        process_substitutions.reserve(2);
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
    std::vector<std::string> parse_semicolon_commands(const std::string& command);
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

    void set_shell(Shell* shell) {
        this->shell = shell;
    }

   private:
    std::string get_variable_value(const std::string& var_name);
    std::string get_exported_variable_value(const std::string& var_name);
    std::string resolve_parameter_value(const std::string& var_name);
    std::vector<std::string> expand_braces(const std::string& pattern);
    std::unordered_map<std::string, std::string> aliases;
    std::unordered_map<std::string, std::string> env_vars;
    Shell* shell = nullptr;
    bool command_validation_enabled = true;
    bool use_exported_vars_only = false;

    std::map<std::string, std::string> current_here_docs;
};