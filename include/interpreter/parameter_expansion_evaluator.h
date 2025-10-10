#pragma once

#include <functional>
#include <string>

class ParameterExpansionEvaluator {
   public:
    using VariableReader = std::function<std::string(const std::string&)>;
    using VariableWriter = std::function<void(const std::string&, const std::string&)>;
    using VariableChecker = std::function<bool(const std::string&)>;
    using PatternMatcher = std::function<bool(const std::string&, const std::string&)>;

    ParameterExpansionEvaluator(VariableReader var_reader, VariableWriter var_writer,
                                VariableChecker var_checker, PatternMatcher pattern_matcher);
    std::string expand(const std::string& param_expr);

   private:
    VariableReader read_variable;
    VariableWriter write_variable;
    VariableChecker is_variable_set;
    PatternMatcher matches_pattern;

    std::string pattern_match_prefix(const std::string& value, const std::string& pattern,
                                     bool longest);
    std::string pattern_match_suffix(const std::string& value, const std::string& pattern,
                                     bool longest);
    std::string pattern_substitute(const std::string& value, const std::string& replacement_expr,
                                   bool global);
    std::string case_convert(const std::string& value, const std::string& pattern, bool uppercase,
                             bool all_chars);
    bool try_evaluate_substring(const std::string& param_expr, std::string& result);
};
