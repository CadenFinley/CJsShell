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

}  // namespace token_constants
