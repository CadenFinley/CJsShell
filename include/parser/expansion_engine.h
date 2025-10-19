#pragma once

#include <string>
#include <vector>

class Shell;

class ExpansionEngine {
   public:
    explicit ExpansionEngine(Shell* shell = nullptr);

    std::vector<std::string> expand_braces(const std::string& pattern);
    std::vector<std::string> expand_wildcards(const std::string& pattern);

   private:
    Shell* shell;
    static constexpr size_t MAX_EXPANSION_SIZE = 10000000;

    
    void expand_and_append_results(const std::string& combined, std::vector<std::string>& result);

    template <typename T>
    void expand_range(T start, T end, const std::string& prefix, const std::string& suffix,
                      std::vector<std::string>& result);
};
