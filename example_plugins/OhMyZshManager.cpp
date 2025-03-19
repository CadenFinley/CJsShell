#include "plugininterface.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <vector>
#include <map>

class OhMyZshManager : public PluginInterface {
private:
    std::filesystem::path zshDir;
    std::filesystem::path customDir;
    std::filesystem::path pluginDir;
    std::vector<std::string> enabledPlugins;
    
    bool isOhMyZshInstalled() {
        return std::filesystem::exists(zshDir);
    }
    
    std::vector<std::string> getInstalledPlugins() {
        std::vector<std::string> plugins;
        if (!std::filesystem::exists(pluginDir)) {
            return plugins;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(pluginDir)) {
            if (entry.is_directory()) {
                plugins.push_back(entry.path().filename().string());
            }
        }
        return plugins;
    }
    
    std::vector<std::string> getEnabledPlugins() {
        if (enabledPlugins.empty()) {
            loadEnabledPlugins();
        }
        return enabledPlugins;
    }
    
    std::string loadEnabledPlugins() {
        enabledPlugins.clear();
        
        // Parse .zshrc file to find enabled plugins
        std::string homePath = getenv("HOME");
        std::filesystem::path zshrcPath = std::filesystem::path(homePath) / ".zshrc";
        
        if (!std::filesystem::exists(zshrcPath)) {
            return "No .zshrc file found.";
        }
        
        std::ifstream zshrc(zshrcPath);
        std::string line;
        while (std::getline(zshrc, line)) {
            if (line.find("plugins=(") != std::string::npos) {
                // Extract plugin list
                size_t start = line.find("(") + 1;
                size_t end = line.find(")");
                if (end > start) {
                    std::string pluginList = line.substr(start, end - start);
                    std::istringstream iss(pluginList);
                    std::string plugin;
                    while (iss >> plugin) {
                        // Remove any commas or quotes
                        plugin.erase(std::remove(plugin.begin(), plugin.end(), ','), plugin.end());
                        plugin.erase(std::remove(plugin.begin(), plugin.end(), '"'), plugin.end());
                        plugin.erase(std::remove(plugin.begin(), plugin.end(), '\''), plugin.end());
                        if (!plugin.empty()) {
                            enabledPlugins.push_back(plugin);
                        }
                    }
                }
                break;
            }
        }
        std::string out = "Enabled zsh plugins: ";
        for (const auto& p : enabledPlugins) {
            out += p + ", ";
        }
        return out;
    }
    
    bool enablePlugin(const std::string& plugin) {
        if (std::find(enabledPlugins.begin(), enabledPlugins.end(), plugin) != enabledPlugins.end()) {
            std::cout << "Plugin '" << plugin << "' is already enabled." << std::endl;
            return true;
        }
        
        // Check if plugin exists
        bool found = false;
        std::vector<std::string> installed = getInstalledPlugins();
        for (const auto& p : installed) {
            if (p == plugin) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            std::cout << "Plugin '" << plugin << "' is not installed." << std::endl;
            return false;
        }
        
        // Update .zshrc file to enable the plugin
        std::string homePath = getenv("HOME");
        std::filesystem::path zshrcPath = std::filesystem::path(homePath) / ".zshrc";
        std::filesystem::path zshrcBackupPath = std::filesystem::path(homePath) / ".zshrc.bak";
        
        // Create a backup
        std::filesystem::copy_file(zshrcPath, zshrcBackupPath, std::filesystem::copy_options::overwrite_existing);
        
        std::ifstream inFile(zshrcPath);
        std::vector<std::string> lines;
        std::string line;
        bool foundPluginLine = false;
        
        while (std::getline(inFile, line)) {
            if (line.find("plugins=(") != std::string::npos) {
                foundPluginLine = true;
                // Insert new plugin
                size_t endPos = line.find(")");
                if (endPos != std::string::npos) {
                    line.insert(endPos, " " + plugin);
                }
            }
            lines.push_back(line);
        }
        inFile.close();
        
        if (!foundPluginLine) {
            std::cout << "Could not find plugin configuration line in .zshrc" << std::endl;
            return false;
        }
        
        std::ofstream outFile(zshrcPath);
        for (const auto& l : lines) {
            outFile << l << "\n";
        }
        outFile.close();
        
        enabledPlugins.push_back(plugin);
        std::cout << "Plugin '" << plugin << "' enabled. Restart your terminal to apply changes." << std::endl;
        return true;
    }
    
    bool disablePlugin(const std::string& plugin) {
        auto it = std::find(enabledPlugins.begin(), enabledPlugins.end(), plugin);
        if (it == enabledPlugins.end()) {
            std::cout << "Plugin '" << plugin << "' is not enabled." << std::endl;
            return false;
        }
        
        // Update .zshrc file to disable the plugin
        std::string homePath = getenv("HOME");
        std::filesystem::path zshrcPath = std::filesystem::path(homePath) / ".zshrc";
        std::filesystem::path zshrcBackupPath = std::filesystem::path(homePath) / ".zshrc.bak";
        
        // Create a backup
        std::filesystem::copy_file(zshrcPath, zshrcBackupPath, std::filesystem::copy_options::overwrite_existing);
        
        std::ifstream inFile(zshrcPath);
        std::vector<std::string> lines;
        std::string line;
        
        while (std::getline(inFile, line)) {
            if (line.find("plugins=(") != std::string::npos) {
                // Remove the plugin
                std::string pattern = " " + plugin;
                size_t pos = line.find(pattern);
                if (pos != std::string::npos) {
                    line.erase(pos, pattern.length());
                } else {
                    // Try without leading space
                    pos = line.find(plugin);
                    if (pos != std::string::npos) {
                        line.erase(pos, plugin.length());
                        // Remove trailing space if any
                        if (pos < line.length() && line[pos] == ' ') {
                            line.erase(pos, 1);
                        }
                    }
                }
            }
            lines.push_back(line);
        }
        inFile.close();
        
        std::ofstream outFile(zshrcPath);
        for (const auto& l : lines) {
            outFile << l << "\n";
        }
        outFile.close();
        
        enabledPlugins.erase(it);
        std::cout << "Plugin '" << plugin << "' disabled. Restart your terminal to apply changes." << std::endl;
        return true;
    }
    
    bool installPlugin(const std::string& plugin, const std::string& url) {
        if (!isOhMyZshInstalled()) {
            std::cout << "Oh My Zsh is not installed." << std::endl;
            return false;
        }
        
        std::filesystem::path customPluginPath = customDir / "plugins" / plugin;
        
        if (std::filesystem::exists(customPluginPath)) {
            std::cout << "Plugin '" << plugin << "' is already installed." << std::endl;
            return false;
        }
        
        // Create custom plugins directory if it doesn't exist
        std::filesystem::path customPluginsDir = customDir / "plugins";
        if (!std::filesystem::exists(customPluginsDir)) {
            std::filesystem::create_directories(customPluginsDir);
        }
        
        std::string gitCmd = "git clone " + url + " " + customPluginPath.string();
        int result = system(gitCmd.c_str());
        
        if (result != 0) {
            std::cout << "Failed to install plugin '" << plugin << "'" << std::endl;
            return false;
        }
        
        std::cout << "Plugin '" << plugin << "' installed successfully." << std::endl;
        return true;
    }
    
    bool removePlugin(const std::string& plugin) {
        std::filesystem::path customPluginPath = customDir / "plugins" / plugin;
        
        if (!std::filesystem::exists(customPluginPath)) {
            std::cout << "Custom plugin '" << plugin << "' is not installed." << std::endl;
            return false;
        }
        
        // Disable plugin first if it's enabled
        auto it = std::find(enabledPlugins.begin(), enabledPlugins.end(), plugin);
        if (it != enabledPlugins.end()) {
            disablePlugin(plugin);
        }
        
        // Remove the plugin directory
        std::error_code ec;
        std::filesystem::remove_all(customPluginPath, ec);
        
        if (ec) {
            std::cout << "Failed to remove plugin '" << plugin << "': " << ec.message() << std::endl;
            return false;
        }
        
        std::cout << "Plugin '" << plugin << "' removed successfully." << std::endl;
        return true;
    }
    
    void showPluginInfo(const std::string& plugin) {
        std::filesystem::path pluginPath;
        bool found = false;
        
        // Check in main plugins directory
        if (std::filesystem::exists(pluginDir / plugin)) {
            pluginPath = pluginDir / plugin;
            found = true;
        }
        // Check in custom plugins directory
        else if (std::filesystem::exists(customDir / "plugins" / plugin)) {
            pluginPath = customDir / "plugins" / plugin;
            found = true;
        }
        
        if (!found) {
            std::cout << "Plugin '" << plugin << "' is not installed." << std::endl;
            return;
        }
        
        std::cout << "Plugin: " << plugin << std::endl;
        
        // Check if plugin is enabled
        bool enabled = std::find(enabledPlugins.begin(), enabledPlugins.end(), plugin) != enabledPlugins.end();
        std::cout << "Status: " << (enabled ? "Enabled" : "Disabled") << std::endl;
        
        // Check for README file
        std::filesystem::path readmePath = pluginPath / "README.md";
        if (std::filesystem::exists(readmePath)) {
            std::cout << "\nDescription from README.md:" << std::endl;
            std::cout << "------------------------" << std::endl;
            std::ifstream readme(readmePath);
            std::string line;
            int lineCount = 0;
            while (std::getline(readme, line) && lineCount < 10) {
                std::cout << line << std::endl;
                lineCount++;
            }
            if (lineCount == 10) {
                std::cout << "... (README truncated, see full version in " << readmePath.string() << ")" << std::endl;
            }
        }
    }

public:
    OhMyZshManager() {
        std::string homePath = getenv("HOME");
        zshDir = std::filesystem::path(homePath) / ".oh-my-zsh";
        customDir = zshDir / "custom";
        pluginDir = zshDir / "plugins";
    }
    
    std::string getName() const override {
        return "OhMyZshManager";
    }
    
    std::string getVersion() const override {
        return "1.0";
    }
    
    std::string getDescription() const override {
        return "Manages Oh My Zsh plugins directly from DevToolsTerminal";
    }
    
    std::string getAuthor() const override {
        return "Caden Finley";
    }
    
    bool initialize() override {
        if (!isOhMyZshInstalled()) {
            std::cout << "Oh My Zsh is not installed. This plugin requires Oh My Zsh." << std::endl;
            std::cout << "Install Oh My Zsh first with: sh -c \"$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)\"" << std::endl;
            return false;
        }
        std::string out = loadEnabledPlugins();
        std::cout << out << std::endl;
        return true;
    }
    
    void shutdown() override {
        // Nothing to clean up
    }
    
    bool handleCommand(std::queue<std::string>& args) override {
        if (args.empty()) {
            std::cout << "Available commands for Oh My Zsh Plugin Manager:" << std::endl;
            std::cout << "  zsh list - List all installed Oh My Zsh plugins" << std::endl;
            std::cout << "  zsh enabled - List enabled Oh My Zsh plugins" << std::endl;
            std::cout << "  zsh enable <plugin> - Enable a plugin" << std::endl;
            std::cout << "  zsh disable <plugin> - Disable a plugin" << std::endl;
            std::cout << "  zsh install <plugin> <git-url> - Install a custom plugin" << std::endl;
            std::cout << "  zsh remove <plugin> - Remove a custom plugin" << std::endl;
            std::cout << "  zsh info <plugin> - Show information about a plugin" << std::endl;
            return true;
        }
        
        std::string cmd = args.front();
        args.pop();

        if(cmd != "zsh"){
            return false;
        }

        cmd = args.front();
        args.pop();
        
        if (cmd == "list") {
            std::vector<std::string> plugins = getInstalledPlugins();
            std::cout << "Installed Oh My Zsh plugins:" << std::endl;
            for (const auto& plugin : plugins) {
                bool enabled = std::find(enabledPlugins.begin(), enabledPlugins.end(), plugin) != enabledPlugins.end();
                std::cout << "  " << plugin << (enabled ? " [enabled]" : "") << std::endl;
            }
            
            // List custom plugins
            std::filesystem::path customPluginsDir = customDir / "plugins";
            if (std::filesystem::exists(customPluginsDir)) {
                std::cout << "\nCustom plugins:" << std::endl;
                for (const auto& entry : std::filesystem::directory_iterator(customPluginsDir)) {
                    if (entry.is_directory()) {
                        std::string plugin = entry.path().filename().string();
                        bool enabled = std::find(enabledPlugins.begin(), enabledPlugins.end(), plugin) != enabledPlugins.end();
                        std::cout << "  " << plugin << (enabled ? " [enabled]" : "") << std::endl;
                    }
                }
            }
            return true;
        } 
        else if (cmd == "enabled") {
            std::vector<std::string> plugins = getEnabledPlugins();
            std::cout << "Enabled Oh My Zsh plugins:" << std::endl;
            for (const auto& plugin : plugins) {
                std::cout << "  " << plugin << std::endl;
            }
            return true;
        }
        else if (cmd == "enable" && !args.empty()) {
            std::string plugin = args.front();
            args.pop();
            return enablePlugin(plugin);
        }
        else if (cmd == "disable" && !args.empty()) {
            std::string plugin = args.front();
            args.pop();
            return disablePlugin(plugin);
        }
        else if (cmd == "install" && !args.empty()) {
            std::string plugin = args.front();
            args.pop();
            
            if (args.empty()) {
                std::cout << "Usage: ohmyzsh install <plugin> <git-url>" << std::endl;
                return false;
            }
            
            std::string url = args.front();
            args.pop();
            return installPlugin(plugin, url);
        }
        else if (cmd == "remove" && !args.empty()) {
            std::string plugin = args.front();
            args.pop();
            return removePlugin(plugin);
        }
        else if (cmd == "info" && !args.empty()) {
            std::string plugin = args.front();
            args.pop();
            showPluginInfo(plugin);
            return true;
        }
        else {
            std::cout << "Unknown command: " << cmd << std::endl;
            return false;
        }
    }
    
    std::vector<std::string> getCommands() const override {
        return {"zsh"};
    }

    std::vector<std::string> getSubscribedEvents() const override {
        return {};
    }

    int getInterfaceVersion() const override {
        return 1;
    }
    
    std::map<std::string, std::string> getDefaultSettings() const override {
        return {
            {"auto_load_enabled", "false"}
        };
    }
    
    void updateSetting(const std::string& key, const std::string& value) override {
        // No settings to update currently
    }
};

// Export the plugin functions
IMPLEMENT_PLUGIN(OhMyZshManager)
