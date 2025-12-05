#pragma once

#include <limits.h>
#include <unistd.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class Shell;

class Built_ins {
   public:
    Built_ins();
    ~Built_ins();

    void set_shell(Shell* shell_ptr);
    std::string get_current_directory() const;
    std::string get_previous_directory() const;
    void set_current_directory();

    Shell* get_shell();

    int builtin_command(const std::vector<std::string>& args);
    int is_builtin_command(const std::string& cmd) const;

    std::vector<std::string> get_builtin_commands() const;

    std::string get_last_error() const;
    int do_ai_request(const std::string& prompt);

   private:
    std::string current_directory;
    std::string previous_directory;
    std::unordered_map<std::string, std::function<int(const std::vector<std::string>&)>> builtins;
    Shell* shell;
    std::unordered_map<std::string, std::string> aliases;
    std::unordered_map<std::string, std::string> env_vars;
    std::string last_terminal_output_error;
};