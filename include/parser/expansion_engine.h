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
};
