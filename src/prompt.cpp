#include "prompt.h"
#include "main.h"
#include "theme.h"
#include <iostream>

Prompt::Prompt() {
}

Prompt::~Prompt() {
}

std::string Prompt::get_prompt() {
    std::filesystem::path repo_root;
    bool is_git_repo = info.is_git_repository(repo_root);
    
    // Base format and segments to check for variable usage
    std::vector<nlohmann::json> segments_to_check;
    
    if (is_git_repo) {
        segments_to_check = g_theme->git_segments;
    } else {
        segments_to_check = g_theme->ps1_segments;
    }
    
    // Get all variables needed for the prompt
    std::unordered_map<std::string, std::string> vars = info.get_variables(segments_to_check, is_git_repo, repo_root);
    
    if (is_git_repo) {
        return g_theme->get_git_prompt_format(vars);
    } else {
        return g_theme->get_ps1_prompt_format(vars);
    }
}

std::string Prompt::get_ai_prompt() {
    std::string modelInfo = g_ai->getModel();
    std::string modeInfo = g_ai->getAssistantType();
            
    if (modelInfo.empty()) modelInfo = "Unknown";
    if (modeInfo.empty()) modeInfo = "Chat";
    
    std::unordered_map<std::string, std::string> vars;
    std::vector<nlohmann::json> segments_to_check = g_theme->ai_segments;
    
    if (info.is_variable_used("AI_MODEL", segments_to_check)) {
        vars["AI_MODEL"] = modelInfo;
    }
    
    if (info.is_variable_used("AI_AGENT_TYPE", segments_to_check)) {
        vars["AI_AGENT_TYPE"] = modeInfo;
    }
    
    if (info.is_variable_used("AI_DIVIDER", segments_to_check)) {
        vars["AI_DIVIDER"] = ">";
    }
    
    // Get common variables like TIME and DATE
    std::unordered_map<std::string, std::string> common_vars = info.get_variables(segments_to_check);
    
    // Merge common variables with AI-specific ones
    for (const auto& [key, value] : common_vars) {
        vars[key] = value;
    }
    
    return g_theme->get_ai_prompt_format(vars);
}

std::string Prompt::get_newline_prompt() {
    std::vector<nlohmann::json> segments_to_check = g_theme->newline_segments;
    std::unordered_map<std::string, std::string> vars = info.get_variables(segments_to_check);
    
    return g_theme->get_newline_prompt(vars);
}

std::string Prompt::get_title_prompt() {
    std::string prompt_format = g_theme->get_terminal_title_format();
    
    // Create a mock segments structure to check which variables are needed
    std::vector<nlohmann::json> segments;
    nlohmann::json segment;
    segment["content"] = prompt_format;
    segments.push_back(segment);
    
    std::unordered_map<std::string, std::string> vars = info.get_variables(segments);
    
    for (const auto& [key, value] : vars) {
        prompt_format = replace_placeholder(prompt_format, "{" + key + "}", value);
    }
    
    return prompt_format;
}

// Helper method to replace placeholders in format strings
std::string Prompt::replace_placeholder(const std::string& format, const std::string& placeholder, const std::string& value) {
    std::string result = format;
    size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
        result.replace(pos, placeholder.length(), value);
        pos += value.length();
    }
    return result;
}