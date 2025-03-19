#include "plugininterface.h"
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <queue>
#include <functional>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <cstring>
#include <termios.h>
#include <unistd.h>
#include <filesystem>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class PasswordPlugin : public PluginInterface {
private:
    bool enabled;
    std::map<std::string, std::string> settings;
    std::string passwordHash;
    bool authenticated;
    std::filesystem::path passwordFilePath;
    
    // Helper method to hide password input
    std::string getPasswordInput() {
        struct termios oldSettings, newSettings;
        tcgetattr(STDIN_FILENO, &oldSettings);
        newSettings = oldSettings;
        newSettings.c_lflag &= ~ECHO; // Disable echo
        tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);
        
        std::string password;
        std::getline(std::cin, password);
        
        tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings); // Restore settings
        std::cout << std::endl;
        return password;
    }

    // New helper method to read password input with prompt and then erase the prompt line
    std::string readAndErasePasswordInput(const std::string &prompt) {
        std::cout << prompt;
        std::string pwd = getPasswordInput();
        std::cout << "\033[A\033[2K"; // Move cursor up one line and clear it
        return pwd;
    }
    
    // SHA-256 hashing for secure password storage
    std::string hashPassword(const std::string& password) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, password.c_str(), password.size());
        SHA256_Final(hash, &sha256);
        
        std::stringstream ss;
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }
    
    // Verify password
    bool verifyPassword(const std::string& password) {
        if (passwordHash.empty()) {
            return true; // No password set
        }
        return hashPassword(password) == passwordHash;
    }
    
    // Create password directory and file if they don't exist
    void ensurePasswordDirectoryExists() {
        std::filesystem::path pwDir = ".DTT-Data/pw";
        if (!std::filesystem::exists(pwDir)) {
            std::filesystem::create_directories(pwDir);
        }
        passwordFilePath = pwDir / "password.json";
    }
    
    // Load password from file
    void loadPassword() {
        ensurePasswordDirectoryExists();
        
        // Try to load from file first
        if (std::filesystem::exists(passwordFilePath)) {
            try {
                std::ifstream file(passwordFilePath);
                if (file.is_open()) {
                    json pwJson;
                    file >> pwJson;
                    file.close();
                    
                    if (pwJson.contains("password_hash")) {
                        passwordHash = pwJson["password_hash"];
                        settings["password_hash"] = passwordHash;
                        return;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error loading password file: " << e.what() << std::endl;
            }
        }
        
        // Fall back to settings if file loading fails
        if (settings.find("password_hash") != settings.end()) {
            passwordHash = settings["password_hash"];
            // Save to file for future use
            savePasswordToFile();
        } else {
            passwordHash = "";
        }
    }
    
    // Save password to file
    void savePasswordToFile() {
        ensurePasswordDirectoryExists();
        
        try {
            json pwJson;
            pwJson["password_hash"] = passwordHash;
            
            std::ofstream file(passwordFilePath);
            if (file.is_open()) {
                file << pwJson.dump(4);
                file.close();
                std::cout << "Password saved to file." << std::endl;
            } else {
                std::cerr << "Unable to open password file for writing." << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error saving password to file: " << e.what() << std::endl;
        }
    }
    
    // Set new password
    void setPassword(const std::string& password) {
        passwordHash = hashPassword(password);
        settings["password_hash"] = passwordHash;
        savePasswordToFile();
    }
    
    // Handle password prompt at startup
    bool handlePasswordPrompt() {
        if (passwordHash.empty()) {
            return true;
        } else {
            int attempts = 0;
            const int maxAttempts = 3;
            
            while (attempts < maxAttempts) {
                int remainingAttempts = maxAttempts - attempts;
                std::string inputPassword = readAndErasePasswordInput("(" + std::to_string(remainingAttempts) + ") Enter password: ");
                
                if (verifyPassword(inputPassword)) {
                    return true;
                } else {
                    attempts++;
                }
            }
            
            std::cout << "Too many failed attempts. Exiting..." << std::endl;
            return false;
        }
    }

public:
    PasswordPlugin() : enabled(false), authenticated(false) {
        settings = getDefaultSettings();
        ensurePasswordDirectoryExists();
    }
    
    virtual ~PasswordPlugin() {}
    
    virtual std::string getName() const override {
        return "PasswordProtection";
    }
    
    virtual std::string getVersion() const override {
        return "1.0.0";
    }
    
    virtual std::string getDescription() const override {
        return "Protects DevToolsTerminal with password authentication";
    }
    
    virtual std::string getAuthor() const override {
        return "Caden Finley";
    }
    
    virtual bool initialize() override {
        loadPassword();
        enabled = true;
        return true;
    }
    
    virtual void shutdown() override {
        enabled = false;
    }
    
    virtual bool handleCommand(std::queue<std::string>& args) override {
        if (args.empty()) {
            return false;
        }
        
        std::string command = args.front();
        args.pop();
        
        if (command == "event" && !args.empty()) {
            std::string eventType = args.front();
            args.pop();
            
            if (eventType == "main_process_pre_run") {
                authenticated = handlePasswordPrompt();
                if (!authenticated) {
                    exit(0); // Exit if authentication fails
                }
                return true;
            }
            return true;
        } else if (command == "password") {
            if (args.empty()) {
                std::cout << "Usage: password [set|change|remove|status]" << std::endl;
                return true;
            }
            
            std::string action = args.front();
            args.pop();
            
            if (action == "set") {
                if (!passwordHash.empty()) {
                    std::cout << "Password is already set. Use 'password change' to update it." << std::endl;
                } else {
                    std::string newPassword = readAndErasePasswordInput("Enter new password: ");
                    if (!newPassword.empty()) {
                        setPassword(newPassword);
                        std::cout << "Password set successfully!" << std::endl;
                    }
                }
                return true;
            } else if (action == "change") {
                if (!passwordHash.empty()) {
                    std::string currentPassword = readAndErasePasswordInput("Enter current password: ");
                    if (verifyPassword(currentPassword)) {
                        std::string newPassword = readAndErasePasswordInput("Enter new password: ");
                        if (!newPassword.empty()) {
                            setPassword(newPassword);
                            std::cout << "Password changed successfully!" << std::endl;
                        }
                    } else {
                        std::cout << "Incorrect password!" << std::endl;
                    }
                } else {
                    std::cout << "No password is currently set. Use 'password set' first." << std::endl;
                }
                return true;
            } else if (action == "remove") {
                if (!passwordHash.empty()) {
                    std::string currentPassword = readAndErasePasswordInput("Enter current password to remove password protection: ");
                    if (verifyPassword(currentPassword)) {
                        passwordHash = "";
                        settings["password_hash"] = "";
                        savePasswordToFile();
                        std::cout << "Password protection removed." << std::endl;
                    } else {
                        std::cout << "Incorrect password!" << std::endl;
                    }
                } else {
                    std::cout << "No password is currently set." << std::endl;
                }
                return true;
            } else if (action == "status") {
                if (!passwordHash.empty()) {
                    std::cout << "Password protection is enabled." << std::endl;
                } else {
                    std::cout << "Password protection is disabled." << std::endl;
                }
                return true;
            } else if (action == "location") {
                std::cout << "Password file location: " << passwordFilePath << std::endl;
                return true;
            }
        }
        
        return false;
    }
    
    virtual std::vector<std::string> getCommands() const override {
        return {"password"};
    }

    virtual std::vector<std::string> getSubscribedEvents() const override {
        return {"main_process_pre_run"};
    }

    virtual int getInterfaceVersion() const override {
        return 1;
    }
    
    virtual std::map<std::string, std::string> getDefaultSettings() const override {
        std::map<std::string, std::string> defaults;
        defaults["password_hash"] = "";
        return defaults;
    }
    
    virtual void updateSetting(const std::string& key, const std::string& value) override {
        settings[key] = value;
        if (key == "password_hash") {
            passwordHash = value;
            savePasswordToFile();
        }
    }
};

// Plugin factory functions
IMPLEMENT_PLUGIN(PasswordPlugin)
