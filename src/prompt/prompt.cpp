#include "prompt.h"

#include <iostream>

#include "cjsh.h"
#include "theme.h"
#include "theme_parser.h"

Prompt::Prompt() : repo_root() {
}

Prompt::~Prompt() {
}

std::string Prompt::get_prompt() {
    // it should never be empty
    if (g_current_theme.empty()) {
        if (!g_theme || !g_theme->get_enabled()) {
            return info.get_basic_prompt();
        } else {
            g_theme->load_theme("default", true);
        }
    }

    if (!g_theme || !g_theme->get_enabled()) {
        return info.get_basic_prompt();
    }

    bool is_git_repo = is_git_repository(repo_root);

    if (is_git_repo) {
        std::unordered_map<std::string, std::string> vars =
            get_variables(PromptType::GIT, true);
        return g_theme->get_git_prompt_format(vars);
    } else {
        std::unordered_map<std::string, std::string> vars =
            get_variables(PromptType::PS1, false);
        return g_theme->get_ps1_prompt_format(vars);
    }
}

std::string Prompt::get_ai_prompt() {
    std::string modelInfo = g_ai->get_model();
    std::string modeInfo = g_ai->get_assistant_type();

    // it should never be empty
    if (g_current_theme.empty()) {
        if (!g_theme || !g_theme->get_enabled()) {
            return info.get_basic_ai_prompt();
        } else {
            g_theme->load_theme("default", true);
        }
    }

    if (!g_theme || !g_theme->get_enabled()) {
        return info.get_basic_ai_prompt();
    }

    if (modelInfo.empty())
        modelInfo = "Unknown";
    if (modeInfo.empty())
        modeInfo = "Chat";

    // Get variables only for AI prompt
    std::unordered_map<std::string, std::string> vars =
        get_variables(PromptType::AI);

    // Add or update AI-specific variables
    vars["AI_MODEL"] = modelInfo;
    vars["AI_AGENT_TYPE"] = modeInfo;
    vars["AI_DIVIDER"] = ">";
    vars["AI_CONTEXT"] = g_ai->get_save_directory();
    vars["AI_CONTEXT_COMPARISON"] =
        (std::filesystem::current_path().string() + "/" ==
         g_ai->get_save_directory())
            ? "✔"
            : "✖";

    return g_theme->get_ai_prompt_format(vars);
}

std::string Prompt::get_newline_prompt() {
    // it should never be empty
    if (g_current_theme.empty()) {
        if (!g_theme || !g_theme->get_enabled()) {
            return " ";
        } else {
            g_theme->load_theme("default", true);
        }
    }

    if (!g_theme || !g_theme->get_enabled()) {
        return " ";
    }

    // Get variables only for newline prompt
    std::unordered_map<std::string, std::string> vars =
        get_variables(PromptType::NEWLINE);

    return g_theme->get_newline_prompt(vars);
}

std::string Prompt::get_inline_right_prompt() {
    if (g_current_theme.empty()) {
        if (!g_theme || !g_theme->get_enabled()) {
            return "";
        } else {
            g_theme->load_theme("default", true);
        }
    }

    if (!g_theme || !g_theme->get_enabled()) {
        return "";
    }

    // Get variables for inline right prompt
    std::unordered_map<std::string, std::string> vars =
        get_variables(PromptType::INLINE_RIGHT);

    return g_theme->get_inline_right_prompt(vars);
}

std::string Prompt::get_title_prompt() {
    if (g_current_theme.empty()) {
        if (!g_theme || !g_theme->get_enabled()) {
            return info.get_basic_title();
        } else {
            g_theme->load_theme("default", true);
        }
    }

    if (!g_theme || !g_theme->get_enabled()) {
        return info.get_basic_title();
    }
    std::string prompt_format = g_theme->get_terminal_title_format();

    // Get variables only for title prompt
    std::unordered_map<std::string, std::string> vars =
        get_variables(PromptType::TITLE);

    for (const auto& [key, value] : vars) {
        prompt_format =
            replace_placeholder(prompt_format, "{" + key + "}", value);
    }

    return prompt_format;
}

std::string Prompt::replace_placeholder(const std::string& format,
                                        const std::string& placeholder,
                                        const std::string& value) {
    std::string result = format;
    size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
        result.replace(pos, placeholder.length(), value);
        pos += value.length();
    }
    return result;
}

// Helper method to get variables for a specific prompt type
std::unordered_map<std::string, std::string> Prompt::get_variables(
    PromptType type, bool is_git_repo) {
    // Collect only the segments needed for the requested prompt type
    std::vector<ThemeSegment> segments;

    switch (type) {
        case PromptType::PS1:
            segments.insert(segments.end(), g_theme->ps1_segments.begin(),
                            g_theme->ps1_segments.end());
            break;
        case PromptType::GIT:
            segments.insert(segments.end(), g_theme->git_segments.begin(),
                            g_theme->git_segments.end());
            break;
        case PromptType::AI:
            segments.insert(segments.end(), g_theme->ai_segments.begin(),
                            g_theme->ai_segments.end());
            break;
        case PromptType::NEWLINE:
            segments.insert(segments.end(), g_theme->newline_segments.begin(),
                            g_theme->newline_segments.end());
            break;
        case PromptType::INLINE_RIGHT:
            segments.insert(segments.end(),
                            g_theme->inline_right_segments.begin(),
                            g_theme->inline_right_segments.end());
            break;
        case PromptType::TITLE: {
            ThemeSegment title_segment("title");
            title_segment.content = g_theme->get_terminal_title_format();
            segments.push_back(title_segment);
            break;
        }
        case PromptType::ALL:
            segments.insert(segments.end(), g_theme->ps1_segments.begin(),
                            g_theme->ps1_segments.end());
            segments.insert(segments.end(), g_theme->git_segments.begin(),
                            g_theme->git_segments.end());
            segments.insert(segments.end(), g_theme->ai_segments.begin(),
                            g_theme->ai_segments.end());
            segments.insert(segments.end(), g_theme->newline_segments.begin(),
                            g_theme->newline_segments.end());
            segments.insert(segments.end(),
                            g_theme->inline_right_segments.begin(),
                            g_theme->inline_right_segments.end());

            ThemeSegment title_segment("title");
            title_segment.content = g_theme->get_terminal_title_format();
            segments.push_back(title_segment);
            break;
    }

    // Get variables only for the specified segments
    return info.get_variables(segments, is_git_repo, repo_root);
}

bool Prompt::is_git_repository(std::filesystem::path& repo_root) {
    if (g_debug_mode)
        std::cerr << "DEBUG: is_git_repository START" << std::endl;

    std::filesystem::path current_path = std::filesystem::current_path();
    std::filesystem::path git_head_path;

    repo_root = current_path;

    while (!info.is_root_path(repo_root)) {
        git_head_path = repo_root / ".git" / "HEAD";
        if (std::filesystem::exists(git_head_path)) {
            if (g_debug_mode)
                std::cerr << "DEBUG: is_git_repository END: true, repo_root="
                          << repo_root.string() << std::endl;
            return true;
        }
        repo_root = repo_root.parent_path();
    }

    if (g_debug_mode)
        std::cerr << "DEBUG: is_git_repository END: false" << std::endl;
    return false;
}