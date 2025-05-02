#include "update.h"
#include "main.h"
#include "cjsh_filesystem.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <limits>

using json = nlohmann::json;

// helper to compare versions
static std::vector<int> parseVersion(const std::string& ver) {
    std::vector<int> parts;
    std::istringstream iss(ver);
    std::string token;
    while (std::getline(iss, token, '.')) {
        parts.push_back(std::stoi(token));
    }
    return parts;
}

static bool is_newer_version(const std::string& latest, const std::string& current) {
    auto a = parseVersion(latest), b = parseVersion(current);
    size_t n = std::max(a.size(), b.size());
    a.resize(n); b.resize(n);
    for (size_t i = 0; i < n; i++) {
        if (a[i] > b[i]) return true;
        if (a[i] < b[i]) return false;
    }
    return false;
}

bool check_for_update() {
    if (!g_silent_update_check) std::cout << "Checking for updates from GitHub...";
    std::string cmd = "curl -s " + c_update_url, result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) { std::cerr << "Error: Unable to execute update check.\n"; return false; }
    char buf[128];
    while (fgets(buf, sizeof(buf), pipe)) result += buf;
    pclose(pipe);
    try {
        auto data = json::parse(result);
        if (data.contains("tag_name")) {
            std::string latest = data["tag_name"].get<std::string>();
            if (!latest.empty() && latest[0]=='v') latest.erase(0,1);
            std::string current = c_version;
            if (!current.empty() && current[0]=='v') current.erase(0,1);
            g_cached_version = latest;
            if (is_newer_version(latest, current)) {
                std::cout << "\nLast Updated: " << g_last_updated << "\n"
                          << c_version << " -> " << latest << std::endl;
                return true;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing update data: " << e.what() << std::endl;
    }
    return false;
}

bool load_update_cache() {
    auto& path = cjsh_filesystem::g_cjsh_update_cache_path;
    if (!std::filesystem::exists(path)) return false;
    std::ifstream f(path);
    if (!f.is_open()) return false;
    try {
        json cache; f >> cache; f.close();
        if (cache.contains("update_available") &&
            cache.contains("latest_version") &&
            cache.contains("check_time")) {
            g_cached_update    = cache["update_available"].get<bool>();
            g_cached_version   = cache["latest_version"].get<std::string>();
            g_last_update_check= cache["check_time"].get<time_t>();
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading update cache: " << e.what() << std::endl;
    }
    return false;
}

void save_update_cache(bool upd, const std::string& latest) {
    json cache;
    cache["update_available"] = upd;
    cache["latest_version"]   = latest;
    cache["check_time"]       = std::time(nullptr);
    std::ofstream f(cjsh_filesystem::g_cjsh_update_cache_path);
    if (f.is_open()) {
        f << cache.dump();
        f.close();
        g_cached_update     = upd;
        g_cached_version    = latest;
        g_last_update_check = std::time(nullptr);
    } else {
        std::cerr << "Warning: Could not open update cache file for writing.\n";
    }
}

bool should_check_for_updates() {
    if (g_last_update_check == 0) return true;
    time_t now = std::time(nullptr);
    time_t elapsed = now - g_last_update_check;
    if (g_debug_mode) {
        std::cout << "Time since last update check: " << elapsed 
                  << " sec (interval: " << g_update_check_interval << ")\n";
    }
    return elapsed > g_update_check_interval;
}

bool download_latest_release() {
    std::cout << "Downloading latest release..." << std::endl;
    std::filesystem::path temp_dir = cjsh_filesystem::g_cjsh_data_path / "temp_update";
    if (std::filesystem::exists(temp_dir)) std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directory(temp_dir);

    std::string download_url;
    {
        std::string api_cmd = "curl -s " + c_update_url;
        std::string api_result;
        FILE* api_pipe = popen(api_cmd.c_str(), "r");
        if (api_pipe) {
            char buf[128];
            while (fgets(buf, sizeof(buf), api_pipe)) api_result += buf;
            pclose(api_pipe);
            try {
                auto api_json = json::parse(api_result);
#if defined(__APPLE__)
                std::string os_suffix = "macos";
#elif defined(__linux__)
                std::string os_suffix = "linux";
#else
                std::string os_suffix;
#endif
                std::string expected = "cjsh" + (os_suffix.empty() ? "" : "-" + os_suffix);
                for (const auto& asset : api_json["assets"]) {
                    std::string name = asset.value("name", "");
                    if (name == expected || (download_url.empty() && name == "cjsh")) {
                        download_url = asset.value("browser_download_url", "");
                        if (name == expected) break;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing asset data: " << e.what() << std::endl;
            }
        }
    }

    if (download_url.empty()) {
        std::cerr << "Error: Unable to determine download URL." << std::endl;
        return false;
    }

    auto asset_name = download_url.substr(download_url.find_last_of('/') + 1);
    std::filesystem::path asset_path = temp_dir / asset_name;
    {
        std::string dl_cmd = "curl -L -s " + download_url + " -o " + asset_path.string();
        FILE* dl = popen(dl_cmd.c_str(), "r");
        if (!dl) {
            std::cerr << "Error: Failed to execute download command." << std::endl;
            return false;
        }
        pclose(dl);
    }

    if (!std::filesystem::exists(asset_path)) {
        std::cerr << "Error: Download failed - output file not created." << std::endl;
        return false;
    }

    std::filesystem::path output_path = temp_dir / "cjsh";
    std::filesystem::rename(asset_path, output_path);

    try {
        std::filesystem::permissions(output_path,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
            std::filesystem::perms::owner_exec | std::filesystem::perms::group_read |
            std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
            std::filesystem::perms::others_exec);
    } catch (const std::exception& e) {
        std::cerr << "Error setting file permissions: " << e.what() << std::endl;
    }

    std::cout << "Administrator privileges required to install the update." << std::endl;
    std::cout << "Please enter your password if prompted." << std::endl;

    std::string sudo_cp = "sudo cp " + output_path.string() + " " + cjsh_filesystem::g_cjsh_path.string();
    FILE* sudo_pipe = popen(sudo_cp.c_str(), "r");
    int sudo_res = sudo_pipe ? pclose(sudo_pipe) : -1;
    if (sudo_res != 0) {
        std::cerr << "Error executing sudo command." << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (std::filesystem::exists(cjsh_filesystem::g_cjsh_path)) {
        auto new_sz = std::filesystem::file_size(output_path);
        auto dest_sz = std::filesystem::file_size(cjsh_filesystem::g_cjsh_path);
        if (new_sz == dest_sz) {
            std::string sudo_chmod = "sudo chmod 755 " + cjsh_filesystem::g_cjsh_path.string();
            FILE* chmod_pipe = popen(sudo_chmod.c_str(), "r");
            pclose(chmod_pipe);
            std::cout << "Update installed successfully." << std::endl;
            std::ofstream changelog((cjsh_filesystem::g_cjsh_data_path / "CHANGELOG.txt").string());
            if (changelog.is_open()) {
                changelog << "Updated to version " << g_cached_version
                          << " on " << get_current_time_string() << std::endl;
                changelog << "See GitHub for full release notes: "
                          << c_github_url << "/releases/tag/v" << g_cached_version << std::endl;
            }
        } else {
            std::cout << "Error: The file was not properly installed (size mismatch)." << std::endl;
        }
    } else {
        std::cout << "Error: Installation failed - destination file doesn't exist." << std::endl;
    }

    if (std::filesystem::exists(temp_dir)) std::filesystem::remove_all(temp_dir);
    if (std::filesystem::exists(cjsh_filesystem::g_cjsh_update_cache_path))
        std::filesystem::remove(cjsh_filesystem::g_cjsh_update_cache_path);

    return true;
}

bool execute_update_if_available(bool avail) {
    if (!avail) { std::cout << "\nYou are up to date!.\n"; return false; }
    std::cout << "\nAn update is available. Download? (Y/N): ";
    char r; std::cin >> r; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (r!='Y' && r!='y') return false;
    save_update_cache(false, g_cached_version);
    if (!download_latest_release()) {
        std::cout << "Failed to update. Visit: " << c_github_url << "/releases/latest\n";
        save_update_cache(true, g_cached_version);
        return false;
    }
    std::cout << "Update installed! Restart now? (Y/N): ";
    std::cin >> r; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (r=='Y' || r=='y') {
        if (g_shell->get_login_mode()) g_shell->restore_terminal_state();
        delete g_shell; delete g_ai; delete g_theme; delete g_plugin;
        execl(cjsh_filesystem::g_cjsh_path.c_str(),
              cjsh_filesystem::g_cjsh_path.c_str(), NULL);
        std::cerr << "Restart failed, please restart manually.\n"; exit(0);
    }
    std::cout << "Please restart to use the new version.\n";
    return true;
}

void display_changelog(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(f)), {});
    f.close();
    std::cout << "\n===== CHANGELOG =====\n" << content << "\n=====================\n";
}

void startup_update_process() {
    g_first_boot = !std::filesystem::exists(cjsh_filesystem::g_cjsh_data_path / ".first_boot_complete");
    if (g_first_boot) {
        std::cout << "\n" << get_colorized_splash() << "\nWelcome to CJ's Shell!\n";
        mark_first_boot_complete();
        g_title_line = false;
    }
    auto cl = (cjsh_filesystem::g_cjsh_data_path / "CHANGELOG.txt").string();
    if (std::filesystem::exists(cl)) {
        display_changelog(cl);
        std::filesystem::rename(cl, cjsh_filesystem::g_cjsh_data_path / "latest_changelog.txt");
        g_last_updated = get_current_time_string();
    } else if (g_check_updates) {
        bool upd = false;
        if (load_update_cache()) {
            if (should_check_for_updates()) {
                upd = check_for_update();
                save_update_cache(upd, g_cached_version);
            } else if (g_cached_update) {
                upd = true;
                if (!g_silent_update_check)
                    std::cout << "\nUpdate available: " << g_cached_version << " (cached)\n";
            }
        } else {
            save_update_cache(upd, g_cached_version);
        }
        if (upd)      execute_update_if_available(upd);
        else if (!g_silent_update_check)
            std::cout << " You are up to date!\n";
    }
}
