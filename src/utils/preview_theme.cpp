#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cjsh.h"
#include "cjsh_filesystem.h"
#include "error_out.h"
#include "prompt.h"
#include "prompt_info.h"
#include "theme.h"
#include "theme_command.h"
#include "theme_parser.h"

int preview_theme(const std::string& theme_name) {
    auto temp_theme = std::make_unique<Theme>(cjsh_filesystem::g_cjsh_theme_path.string(), true);

    std::string canonical_theme = Theme::strip_theme_extension(theme_name);
    std::filesystem::path theme_file =
        cjsh_filesystem::g_cjsh_theme_path / Theme::ensure_theme_extension(canonical_theme);

    if (!std::filesystem::exists(theme_file)) {
        print_error({ErrorType::FILE_NOT_FOUND,
                     "preview_theme",
                     "Theme '" + canonical_theme + "' not found.",
                     {"Run 'theme' to list installed themes."}});
        return 1;
    }

    if (!temp_theme->load_theme(canonical_theme, false)) {
        print_error({ErrorType::RUNTIME_ERROR,
                     "preview_theme",
                     "Failed to load theme '" + canonical_theme +
                         "' for preview. See previous errors for details.",
                     {}});
        return 1;
    }

    std::cout << "\nPreview of theme '" << canonical_theme << "':" << '\n';
    temp_theme->view_theme_requirements(canonical_theme);
    std::cout << "==========================================\n" << '\n';

    PromptInfo prompt_info;
    Prompt prompt;

    std::filesystem::path repo_root = std::filesystem::current_path();
    bool is_git_repo = prompt.is_git_repository(repo_root);

    std::vector<ThemeSegment> all_segments;
    all_segments.insert(all_segments.end(), temp_theme->ps1_segments.begin(),
                        temp_theme->ps1_segments.end());
    all_segments.insert(all_segments.end(), temp_theme->git_segments.begin(),
                        temp_theme->git_segments.end());
    all_segments.insert(all_segments.end(), temp_theme->newline_segments.begin(),
                        temp_theme->newline_segments.end());
    ThemeSegment title_segment("title");
    title_segment.content = temp_theme->get_terminal_title_format();
    all_segments.push_back(title_segment);

    auto vars = prompt_info.get_variables(all_segments, is_git_repo, repo_root);

    if (!is_git_repo) {
        vars["GIT_BRANCH"] = "main";
        vars["GIT_STATUS"] = "*+";
        vars["LOCAL_PATH"] = vars["DIRECTORY"];
    }

    std::cout << "Terminal Title: ";
    std::string title_format = temp_theme->get_terminal_title_format();
    for (const auto& [key, value] : vars) {
        size_t pos = 0;
        std::string placeholder = "{" + key + "}";
        while ((pos = title_format.find(placeholder, pos)) != std::string::npos) {
            title_format.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    std::cout << title_format << '\n';
    std::cout << '\n';

    std::cout << "Standard Prompt:" << '\n';
    std::cout << temp_theme->get_ps1_prompt_format(vars) << '\n';
    std::cout << '\n';

    std::cout << "Git Prompt:" << '\n';
    std::cout << temp_theme->get_git_prompt_format(vars) << '\n';
    std::cout << '\n';

    if (temp_theme->uses_newline()) {
        std::cout << "Newline Prompt:" << '\n';
        std::cout << temp_theme->get_newline_prompt(vars) << '\n';
        std::cout << '\n';
    }

    std::cout << "==========================================" << '\n';
    std::cout << "Note: Actual appearance may vary based on your terminal and "
                 "environment.\n"
              << '\n';

    return 0;
}
