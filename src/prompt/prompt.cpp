#include "prompt.h"

#include "cjsh.h"
#include "theme.h"
#include "theme_parser.h"

Prompt::Prompt() : repo_root() {
}

Prompt::~Prompt() {
}

std::string Prompt::get_prompt() {
    if (g_current_theme.empty() || (!g_theme || !g_theme->get_enabled())) {
        return info.get_basic_prompt();
    }

    bool is_git_repo = is_git_repository(repo_root);

    if (is_git_repo) {
        std::unordered_map<std::string, std::string> vars = get_variables(PromptType::GIT, true);
        return g_theme->get_git_prompt_format(vars);
    }
    std::unordered_map<std::string, std::string> vars = get_variables(PromptType::PS1, false);
    return g_theme->get_ps1_prompt_format(vars);
}

std::string Prompt::get_newline_prompt() {
    if (g_current_theme.empty() && (!g_theme || !g_theme->get_enabled())) {
        return " ";
    }

    std::unordered_map<std::string, std::string> vars = get_variables(PromptType::NEWLINE);

    return g_theme->get_newline_prompt(vars);
}

std::string Prompt::get_inline_right_prompt() {
    if (g_current_theme.empty() || (!g_theme || !g_theme->get_enabled())) {
        return "";
    }

    std::unordered_map<std::string, std::string> vars = get_variables(PromptType::INLINE_RIGHT);

    return g_theme->get_inline_right_prompt(vars);
}

std::string Prompt::get_title_prompt() {
    if (g_current_theme.empty() || (!g_theme || !g_theme->get_enabled())) {
        return info.get_basic_title();
    }

    std::string prompt_format = g_theme->get_terminal_title_format();

    std::unordered_map<std::string, std::string> vars = get_variables(PromptType::TITLE);

    for (const auto& [key, value] : vars) {
        std::string placeholder = "{";
        placeholder += key;
        placeholder += "}";
        prompt_format = replace_placeholder(prompt_format, placeholder, value);
    }

    return prompt_format;
}

std::string Prompt::replace_placeholder(const std::string& format, const std::string& placeholder,
                                        const std::string& value) {
    std::string result = format;
    size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
        result.replace(pos, placeholder.length(), value);
        pos += value.length();
    }
    return result;
}

std::unordered_map<std::string, std::string> Prompt::get_variables(PromptType type,
                                                                   bool is_git_repo) {
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
        case PromptType::NEWLINE:
            segments.insert(segments.end(), g_theme->newline_segments.begin(),
                            g_theme->newline_segments.end());
            break;
        case PromptType::INLINE_RIGHT:
            segments.insert(segments.end(), g_theme->inline_right_segments.begin(),
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
            segments.insert(segments.end(), g_theme->newline_segments.begin(),
                            g_theme->newline_segments.end());
            segments.insert(segments.end(), g_theme->inline_right_segments.begin(),
                            g_theme->inline_right_segments.end());

            ThemeSegment title_segment("title");
            title_segment.content = g_theme->get_terminal_title_format();
            segments.push_back(title_segment);
            break;
    }

    return info.get_variables(segments, is_git_repo, repo_root);
}

bool Prompt::is_git_repository(std::filesystem::path& repo_root) {
    std::filesystem::path current_path = std::filesystem::current_path();
    std::filesystem::path git_head_path;

    repo_root = current_path;

    while (!info.is_root_path(repo_root)) {
        git_head_path = repo_root / ".git" / "HEAD";
        if (std::filesystem::exists(git_head_path)) {
            return true;
        }
        repo_root = repo_root.parent_path();
    }
    return false;
}
