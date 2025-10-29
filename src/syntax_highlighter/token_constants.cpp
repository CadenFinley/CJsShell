#include "token_constants.h"

namespace token_constants {

const std::unordered_set<std::string> comparison_operators = {
    "=",   "==",  "!=",  "<",   "<=",  ">",   ">=",  "-eq",
    "-ne", "-gt", "-ge", "-lt", "-le", "-ef", "-nt", "-ot"};

const std::unordered_set<std::string> command_operators = {"&&", "||", "|", ";"};

const std::unordered_set<std::string> shell_keywords = {
    "if",    "then", "else", "elif", "fi",       "case",   "in",   "esac",   "while",
    "until", "for",  "do",   "done", "function", "select", "time", "coproc", "{",
    "}",     "[[",   "]]",   "(",    ")",        ":",      "[",    "]"};

const std::unordered_set<std::string> redirection_operators = {
    ">",  ">>",  "<",  "<<",  "<<<",  "&>",   "&>>", "<&", ">&", "|&",
    "2>", "2>>", "1>", "1>>", "2>&1", "1>&2", ">&2", "<>", "1<", "2<",
    "0<", "0>",  "3>", "4>",  "5>",   "6>",   "7>",  "8>", "9>"};

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
