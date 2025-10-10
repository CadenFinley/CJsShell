#include "token_constants.h"

namespace token_constants {

const std::unordered_set<std::string> comparison_operators = {
    "=",   "==",  "!=",  "<",   "<=",  ">",   ">=",  "-eq",
    "-ne", "-gt", "-ge", "-lt", "-le", "-ef", "-nt", "-ot"};

const std::unordered_set<std::string> basic_unix_commands = {
    "cat",  "mv",    "cp",    "rm", "mkdir", "rmdir", "touch",  "grep",
    "find", "chmod", "chown", "ps", "man",   "which", "whereis"};

const std::unordered_set<std::string> command_operators = {"&&", "||", "|", ";"};

const std::unordered_set<std::string> shell_keywords = {
    "if",     "then",  "else", "elif", "fi",   "case",     "in",     "esac",
    "while",  "until", "for",  "do",   "done", "function", "select", "time",
    "coproc", "{",     "}",    "[[",   "]]",   "(",        ")",      ":", "[","]"};

const std::unordered_set<std::string> shell_built_ins = {
    "echo",    "printf", "pwd",      "cd",          "ls",        "alias",    "export", "unalias",
    "unset",   "set",    "shift",    "break",       "continue",  "return",   "source", ".",
      "help",   "version",      "eval",     "syntax", "history",
    "exit",    "quit",    "test",         "exec",   "trap",
    "jobs",    "fg",     "bg",       "wait",        "kill",      "readonly", "read",   "umask",
    "getopts", "times",  "type",     "hash"};

const std::unordered_map<std::string, std::string> default_styles = {
    {"unknown-command", "bold color=#FF5555"},
    {"colon", "bold color=#8BE9FD"},
    {"path-exists", "color=#50FA7B"},
    {"path-not-exists", "color=#FF5555"},
    {"glob-pattern", "color=#F1FA8C"},
    {"operator", "bold color=#FF79C6"},
    {"keyword", "bold color=#BD93F9"},
    {"builtin", "color=#FFB86C"},
    {"system", "color=#50FA7B"},
    {"installed", "color=#8BE9FD"},
    {"variable", "color=#8BE9FD"},
    {"assignment-value", "color=#F8F8F2"},
    {"string", "color=#F1FA8C"},
    {"comment", "color=#6272A4"},
    {"command-substitution", "color=#8BE9FD"},
    {"arithmetic", "color=#FF79C6"},
    {"option", "color=#BD93F9"},
    {"number", "color=#FFB86C"},
    {"function-definition", "bold color=#F1FA8C"},
    {"history-expansion", "bold color=#FF79C6"},
    {"ic-linenumbers", "ansi-lightgray"},
    {"ic-linenumber-current", "ansi-yellow"}};

}  // namespace token_constants
