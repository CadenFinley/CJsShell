#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <map>
#include <queue>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <limits>
#include <ostream>
#include <nlohmann/json.hpp>
#include <future>
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#include <grp.h>

#include "include/terminal.h"
#include "include/openaipromptengine.h"
#include "include/pluginmanager.h"
#include "include/thememanager.h"
#include "../isocline/include/isocline.h"

using json = nlohmann::json;


// Constants
const std::string processId = std::to_string(getpid());
const std::string currentVersion = "2.0.1.4";
const std::string githubRepoURL = "https://github.com/CadenFinley/CJsShell";
const std::string updateURL_Github = "https://api.github.com/repos/cadenfinley/CJsShell/releases/latest";

// Flags
bool TESTING = false;
bool exitFlag = false;
bool defaultTextEntryOnAI = false;
bool displayWholePath = false;
bool saveLoop = false;
bool saveOnExit = false;
bool updateFromGithub = false;
bool isLoginShell = false;
bool isFileHandler = false;
bool shortcutsEnabled = true;
bool startCommandsOn = true;
bool usingChatCache = true;
bool checkForUpdates = true;
bool silentCheckForUpdates = true;
bool cachedUpdateAvailable = false;
bool historyExpansionEnabled = true;

// Update-related variables
time_t lastUpdateCheckTime = 0;
int UPDATE_CHECK_INTERVAL = 86400;
std::string cachedLatestVersion = "";

// Tab completion variables
std::vector<std::string> cachedCompletions;
int currentCompletionIndex = 0;
std::string currentCompletionPrefix;

// Paths
std::filesystem::path ACTUAL_SHELL_PATH = std::filesystem::path("/usr/local/bin/cjsh");
std::string homeDir = std::getenv("HOME");
std::filesystem::path INSTALL_PATH = std::getenv("CJSH_INSTALL_PATH") ? std::filesystem::path(std::getenv("CJSH_INSTALL_PATH")) : std::filesystem::path("/usr/local/bin") / "cjsh";
std::filesystem::path DATA_DIRECTORY = std::filesystem::path(homeDir) / ".cjsh_data";
std::filesystem::path CJSHRC_FILE = DATA_DIRECTORY / ".cjshrc";
std::filesystem::path UNINSTALL_SCRIPT_PATH = DATA_DIRECTORY / "cjsh_uninstall.sh";
std::filesystem::path UPDATE_SCRIPT_PATH = DATA_DIRECTORY / "cjsh_update.sh";
std::filesystem::path USER_DATA = DATA_DIRECTORY / "USER_DATA.json";
std::filesystem::path USER_COMMAND_HISTORY = DATA_DIRECTORY / "USER_COMMAND_HISTORY.txt";
std::filesystem::path THEMES_DIRECTORY = DATA_DIRECTORY / "themes";
std::filesystem::path PLUGINS_DIRECTORY = DATA_DIRECTORY / "plugins";
std::filesystem::path UPDATE_CACHE_FILE = DATA_DIRECTORY / "update_cache.json";

// Theming
std::string currentTheme = "default";
std::string GREEN_COLOR_BOLD = "\033[1;32m";
std::string RESET_COLOR = "\033[0m";
std::string RED_COLOR_BOLD = "\033[1;31m";
std::string PURPLE_COLOR_BOLD = "\033[1;35m";
std::string BLUE_COLOR_BOLD = "\033[1;34m";
std::string YELLOW_COLOR_BOLD = "\033[1;33m";
std::string CYAN_COLOR_BOLD = "\033[1;36m";

// Command-related variables
std::string shortcutsPrefix = "@";
std::string lastCommandParsed;
std::string titleLine = "CJ's Shell v" + currentVersion + " - Caden J Finley (c) 2025";
std::string createdLine = "Created 2025 @ " + PURPLE_COLOR_BOLD + "Abilene Christian University" + RESET_COLOR;
std::string lastUpdated = "N/A";

// Data structures
std::queue<std::string> commandsQueue;
std::vector<std::string> startupCommands;
std::vector<std::string> savedChatCache;
std::vector<std::string> commandLines;
std::map<std::string, std::string> aliases;
std::map<std::string, std::vector<std::string>> multiScriptShortcuts;
std::map<std::string, std::map<std::string, std::string>> availableThemes;

// Objects
OpenAIPromptEngine c_assistant;
Terminal terminal;
PluginManager* pluginManager = nullptr;
ThemeManager* themeManager = nullptr;

// Terminal and job control
std::mutex rawModeMutex;
pid_t shell_pgid = 0;
struct termios shell_tmodes;
int shell_terminal;
bool jobControlEnabled = false;

// Command Parsing and Execution
void mainProcessLoop();
void startupCommandsHandler();
void commandParser(const std::string& command);
void commandProcesser(const std::string& command);
void multiScriptShortcutProcesser(const std::string& command);
void sendTerminalCommand(const std::string& command);
void getNextCommand();
std::vector<std::string> commandSplicer(const std::string& command);

// User Input and History
void addUserInputToHistory(const std::string& input);
void createNewUSER_HISTORYfile();
void writeUserData();
void createNewUSER_DATAFile();
std::string readAndReturnUserDataFile();

// AI Process
void chatProcess(const std::string& message);
void showChatHistory();

// Plugin Management
void pluginCommands();
void loadPluginsAsync(std::function<void()> callback);

// Theme Management
void themeCommands();
void loadTheme(const std::string& themeName);
void applyColorToStrings();
void loadThemeAsync(const std::string& themeName, std::function<void(bool)> callback);

// Update Management
bool checkForUpdate();
void manualUpdateCheck();
void setUpdateInterval(int intervalHours);
bool shouldCheckForUpdates();
bool loadUpdateCache();
void saveUpdateCache(bool updateAvailable, const std::string& latestVersion);
bool executeUpdateIfAvailable(bool updateAvailable);
void asyncCheckForUpdates(std::function<void(bool)> callback);
bool checkFromUpdate_Github(std::function<bool(const std::string&, const std::string&)> isNewerVersion);
bool downloadLatestRelease();
void processChangelogAsync();

// Environment and Initialization
void setupEnvironmentVariables();
void initializeLoginEnvironment();
void initializeDataDirectories();
void setupSignalHandlers();
void setupJobControl();
void resetTerminalOnExit();
void createDefaultCJSHRC();
void processProfileFile(const std::string& filePath);

// Login and File Handling
bool isRunningAsLoginShell(char* argv0);
bool checkIsFileHandler(int argc, char* argv[]);
void setupLoginShell();
void cleanupLoginShell();
void handleFileExecution(const std::string& filePath);
void loadUserDataAsync(std::function<void()> callback);

// Signal Handling
void handleSIGHUP(int sig);
void handleSIGTERM(int sig);
void handleSIGINT(int sig);
void handleSIGCHLD(int sig);

// Utility Functions
bool authenticateUser();
bool startsWith(const std::string& str, const std::string& prefix);
std::string expandEnvVariables(const std::string& input);
std::string expandHistoryCommand(const std::string& command);
std::string performCommandSubstitution(const std::string& command);
void parentProcessWatchdog();
bool isParentProcessAlive();
void printHelp();

// Alias Management
void loadAliasesFromFile(const std::string& filePath);
void saveAliasToCJSHRC(const std::string& name, const std::string& value);

// Tab Completion
char* command_generator(const char* text, int state);
char** command_completion(const char* text, int start, int end);
std::vector<std::string> get_completion_matches(const std::string& prefix);

// Command Handlers
void updateCommands();
void aiSettingsCommands();
void aiChatCommands();
void userSettingsCommands();
void textCommands();
void shortcutCommands();
void userDataCommands();

int main(int argc, char* argv[]) {
    
    isLoginShell = isRunningAsLoginShell(argv[0]);
    isFileHandler = checkIsFileHandler(argc, argv);

    setupSignalHandlers();
    
    if (isLoginShell) {
        try {
            setupLoginShell();
        } catch (const std::exception& e) {
            std::cerr << "Error in login shell setup: " << e.what() << std::endl;
            isLoginShell = false;
        }
    }

    bool executeCommand = false;
    std::string cmdToExecute;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-c" && i + 1 < argc) {
            executeCommand = true;
            cmdToExecute = argv[i + 1];
            i++;
        } else if ((arg == "-l" || arg == "--login") && !isLoginShell) {
            isLoginShell = true;
            setupLoginShell();
        } else if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        } else if (arg == "--no-update") {
            checkForUpdates = false;
        } else if (arg == "--silent-update") {
            silentCheckForUpdates = true;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << titleLine << std::endl;
            std::cout << createdLine << std::endl;
            std::cout << "Data directory at: " << DATA_DIRECTORY << std::endl;
            return 0;
        } else if (arg == "-d" || arg == "--debug") {
            TESTING = true;
        }
    }
    
    startupCommands = {};
    multiScriptShortcuts = {};
    c_assistant = OpenAIPromptEngine("", "chat", "You are an AI personal assistant within a shell.", {}, DATA_DIRECTORY);

    initializeDataDirectories();
    setupEnvironmentVariables();
    
    if (isFileHandler) {
        std::string filePath = argv[1];
        if (!std::filesystem::path(filePath).is_absolute()) {
            filePath = (std::filesystem::current_path() / filePath).string();
        }
        
        std::cout << "cjsh launched as file handler for: " << filePath << std::endl;
        handleFileExecution(filePath);
        
        if (isLoginShell) {
            cleanupLoginShell();
        }
        return 0;
    }
    
    if (executeCommand) {
        sendTerminalCommand(cmdToExecute);
        if (isLoginShell) {
            cleanupLoginShell();
        }
        return 0;
    }

    std::future<void> userDataFuture = std::async(std::launch::async, [&]() {
        if (std::filesystem::exists(USER_DATA)) {
            loadUserDataAsync([]() {});
        } else {
            createNewUSER_DATAFile();
        }
        
        if (!std::filesystem::exists(USER_COMMAND_HISTORY)) {
            createNewUSER_HISTORYfile();
        }
    });
    
    std::future<void> changelogFuture;
    if (std::filesystem::exists(DATA_DIRECTORY / "CHANGELOG.txt")) {
        changelogFuture = std::async(std::launch::async, processChangelogAsync);
    }
    
    std::future<void> pluginFuture = std::async(std::launch::async, [&]() {
        pluginManager = new PluginManager(PLUGINS_DIRECTORY);
        loadPluginsAsync([]() {});
    });

    userDataFuture.wait();

    std::future<void> themeFuture = std::async(std::launch::async, [&]() {
        themeManager = new ThemeManager(THEMES_DIRECTORY);
        loadThemeAsync(currentTheme, [&](bool success) {
            if (success) {
                applyColorToStrings();
            }
        });
    });

    std::future<void> updateFuture;
    if (checkForUpdates) {
        updateFuture = std::async(std::launch::async, [&]() {
            bool cacheLoaded = loadUpdateCache();
            
            if (!cacheLoaded || shouldCheckForUpdates()) {
                asyncCheckForUpdates([](bool updateAvailable) {
                    if (updateAvailable) {
                        executeUpdateIfAvailable(updateAvailable);
                    } else {
                        if(!silentCheckForUpdates){
                            std::cout << " -> You are up to date!" << std::endl;
                        }
                    }
                });
            } else if (cachedUpdateAvailable) {
                if (!silentCheckForUpdates) {
                    std::cout << "\nUpdate available: " << cachedLatestVersion << " (cached)" << std::endl;
                }
                executeUpdateIfAvailable(true);
            }
        });
    }

    if (changelogFuture.valid()) changelogFuture.wait();
    pluginFuture.wait();
    themeFuture.wait();
    if (updateFuture.valid()) updateFuture.wait();

    std::thread watchdogThread(parentProcessWatchdog);
    watchdogThread.detach();

    if (!startupCommands.empty() && startCommandsOn) {
        for (const auto& command : startupCommands) {
            commandParser(command);
        }
    }

    if(!exitFlag){
        std::cout << titleLine << std::endl;
        std::cout << createdLine << std::endl;
    
        mainProcessLoop();
    }

    std::cout << "CJ's Shell Exiting..." << std::endl;

    if(saveOnExit){
        savedChatCache = c_assistant.getChatCache();
        writeUserData();
    }
    
    if (isLoginShell) {
        cleanupLoginShell();
    }
    
    delete pluginManager;
    delete themeManager;
    return 0;
}

void notifyPluginsTriggerMainProcess(std::string trigger, std::string data = "") {
    if (pluginManager == nullptr) {
        std::cerr << "PluginManager is not initialized." << std::endl;
        return;
    }
    if (pluginManager->getEnabledPlugins().empty()) {
        return;
    }
    pluginManager->triggerSubscribedGlobalEvent("main_process_" + trigger, data);
}

void mainProcessLoop() {
    notifyPluginsTriggerMainProcess("pre_run", processId);
    
    ic_set_prompt_marker("", NULL);
    ic_enable_hint(true);
    ic_set_hint_delay(100);
    ic_enable_completion_preview(true);
    
    while (true) {
        notifyPluginsTriggerMainProcess("start", processId);
        if (saveLoop) {
            writeUserData();
        }
        
        if (TESTING) {
            std::cout << RED_COLOR_BOLD << "DEV MODE ENABLED" << RESET_COLOR << std::endl;
        }

        std::string prompt;
        if (defaultTextEntryOnAI) {
            std::string modelInfo = c_assistant.getModel();
            std::string modeInfo = c_assistant.getAssistantType();
            
            if (modelInfo.empty()) modelInfo = "Unknown";
            if (modeInfo.empty()) modeInfo = "Chat";
            
            prompt = GREEN_COLOR_BOLD + "[" + YELLOW_COLOR_BOLD + modelInfo + 
                    GREEN_COLOR_BOLD + " | " + BLUE_COLOR_BOLD + modeInfo + 
                    GREEN_COLOR_BOLD + "] >" + RESET_COLOR;
            
            if (TESTING) {
                std::cout << "AI Prompt: " << prompt << std::endl;
                std::cout << "Model: '" << modelInfo << "', Mode: '" << modeInfo << "'" << std::endl;
            }
        } else {
            prompt = terminal.returnCurrentTerminalPosition();
        }

        std::string fullPrompt = prompt + " ";
        char* input = ic_readline(fullPrompt.c_str());
        
        if (input != nullptr) {
            std::string command(input);
            if (!command.empty()) {
                notifyPluginsTriggerMainProcess("command_processed", command);
                commandParser(command);

                ic_history_add(command.c_str());
            }
            ic_free(input);
            if (exitFlag) {
                break;
            }
        } else {
            exitFlag = true;
        }

        notifyPluginsTriggerMainProcess("end", processId);
        if (exitFlag) {
            break;
        }
    }
}

bool authenticateUser(){
    return true;
}

bool isRunningAsLoginShell(char* argv0) {
    if (argv0 && argv0[0] == '-') {
        return true;
    }
    return false;
}

bool checkIsFileHandler(int argc, char* argv[]) {
    if (argc == 2) {
        std::string arg = argv[1];
        if (arg[0] != '-' && std::filesystem::exists(arg)) {
            return true;
        }
    }
    return false;
}

void handleFileExecution(const std::string& filePath) {
    std::filesystem::path path(filePath);
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (access(filePath.c_str(), X_OK) == 0) {
        std::cout << "Executing file: " << filePath << std::endl;
        terminal.executeCommand(filePath).join();
        return;
    }
    
    std::ifstream file(filePath);
    if (file.is_open()) {
        std::string firstLine;
        std::getline(file, firstLine);
        file.close();
        
        if (firstLine.size() > 2 && firstLine[0] == '#' && firstLine[1] == '!') {
            std::string interpreter = firstLine.substr(2);
            interpreter.erase(0, interpreter.find_first_not_of(" \t"));
            
            std::string interpreterCmd;
            size_t spacePos = interpreter.find(' ');
            if (spacePos != std::string::npos) {
                interpreterCmd = interpreter.substr(0, spacePos);
                std::string args = interpreter.substr(spacePos + 1);
                terminal.executeCommand(interpreterCmd + " " + args + " \"" + filePath + "\"").join();
            } else {
                terminal.executeCommand(interpreter + " \"" + filePath + "\"").join();
            }
            return;
        }
    }
    
    if (extension == ".sh") {
        terminal.executeCommand("sh \"" + filePath + "\"").join();
    } else if (extension == ".py") {
        terminal.executeCommand("python3 \"" + filePath + "\"").join();
    } else if (extension == ".js") {
        terminal.executeCommand("node \"" + filePath + "\"").join();
    } else if (extension == ".html") {
        terminal.executeCommand("open \"" + filePath + "\"").join();
    } else if (extension == ".txt" || extension == ".md" || extension == ".json" || extension == ".xml") {
        terminal.executeCommand("open \"" + filePath + "\"").join();
    } else {
        std::cout << "Opening file with system default application" << std::endl;
        terminal.executeCommand("open \"" + filePath + "\"").join();
    }
}

void setupLoginShell() {
    initializeLoginEnvironment();
    setupEnvironmentVariables();
    setupSignalHandlers();
    setupJobControl();
    
    processProfileFile("/etc/profile");
    
    std::string homeDir = std::getenv("HOME") ? std::getenv("HOME") : "";
    if (!homeDir.empty()) {
        createDefaultCJSHRC();
        
        std::vector<std::string> profileFiles = {
            homeDir + "/.profile",
            homeDir + "/.bashrc",
            homeDir + "/.zprofile",
            homeDir + "/.zshrc",
            CJSHRC_FILE.string()
        };
        
        for (const auto& profile : profileFiles) {
            if (std::filesystem::exists(profile)) {
                processProfileFile(profile);
            }
        }
        
        std::vector<std::string> brewPaths = {
            "/opt/homebrew/bin",
            "/usr/local/bin",
            homeDir + "/.homebrew/bin",
            "/opt/homebrew/sbin",
            "/usr/local/sbin"
        };
        
        std::string currentPath = std::getenv("PATH") ? std::getenv("PATH") : "";
        for (const auto& brewPath : brewPaths) {
            if (std::filesystem::exists(brewPath) && 
                currentPath.find(brewPath) == std::string::npos) {
                currentPath = brewPath + ":" + currentPath;
            }
        }
        setenv("PATH", currentPath.c_str(), 1);
    }
    
    loadAliasesFromFile(CJSHRC_FILE.string());
}

void cleanupLoginShell() {
    try {
        resetTerminalOnExit();
    } catch (const std::exception& e) {
        std::cerr << "Error cleaning up terminal: " << e.what() << std::endl;
    }
}

std::string expandEnvVariables(const std::string& input) {
    std::string result = input;
    size_t varPos = result.find('$');
    
    while (varPos != std::string::npos) {
        if (varPos + 1 < result.size() && result[varPos + 1] == '{') {
            size_t closeBrace = result.find('}', varPos + 2);
            if (closeBrace != std::string::npos) {
                std::string varName = result.substr(varPos + 2, closeBrace - varPos - 2);
                const char* envValue = getenv(varName.c_str());
                result.replace(varPos, closeBrace - varPos + 1, envValue ? envValue : "");
            } else {
                varPos++;
            }
        }
        else {
            size_t endPos = varPos + 1;
            while (endPos < result.size() && 
                  (isalnum(result[endPos]) || result[endPos] == '_')) {
                endPos++;
            }
            
            if (endPos > varPos + 1) {
                std::string varName = result.substr(varPos + 1, endPos - varPos - 1);
                const char* envValue = getenv(varName.c_str());
                result.replace(varPos, endPos - varPos, envValue ? envValue : "");
            } else {
                varPos++;
            }
        }
        
        varPos = result.find('$', varPos);
    }
    
    return result;
}

std::string expandHistoryCommand(const std::string& command) {
    if (!historyExpansionEnabled || command.empty()) {
        return command;
    }
    
    if (command == "!!") {
        if (terminal.getTerminalCacheUserInput().empty()) {
            std::cerr << "No previous command in history" << std::endl;
            return "";
        }
        return terminal.getTerminalCacheUserInput().back();
    } 
    else if (command[0] == '!') {
        if (command.length() > 1 && isdigit(command[1])) {
            try {
                int cmdNum = std::stoi(command.substr(1));
                auto history = terminal.getTerminalCacheUserInput();
                if (cmdNum > 0 && cmdNum <= static_cast<int>(history.size())) {
                    return history[cmdNum - 1];
                } else {
                    std::cerr << "Invalid history number: " << cmdNum << std::endl;
                    return "";
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing history number: " << e.what() << std::endl;
                return "";
            }
        } 
        else if (command.length() > 1) {
            std::string prefix = command.substr(1);
            auto history = terminal.getTerminalCacheUserInput();
            for (auto it = history.rbegin(); it != history.rend(); ++it) {
                if (startsWith(*it, prefix)) {
                    return *it;
                }
            }
            std::cerr << "No matching command in history for: " << prefix << std::endl;
            return "";
        }
    }
    
    return command;
}

std::string performCommandSubstitution(const std::string& command) {
    std::string result = command;
    std::string::size_type pos = 0;
    
    while ((pos = result.find("$(", pos)) != std::string::npos) {
        int depth = 1;
        std::string::size_type end = pos + 2;
        
        while (end < result.length() && depth > 0) {
            if (result[end] == '(') depth++;
            else if (result[end] == ')') depth--;
            end++;
        }
        
        if (depth == 0) {
            std::string subCommand = result.substr(pos + 2, end - pos - 3);
            
            FILE* pipe = popen(subCommand.c_str(), "r");
            if (!pipe) {
                std::cerr << "Error executing command substitution" << std::endl;
                continue;
            }
            
            std::string output;
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
            }
            pclose(pipe);
            
            while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
                output.pop_back();
            }

            result.replace(pos, end - pos, output);
            pos += output.length();
        } else {
            pos = end;
        }
    }
    
    pos = 0;
    while ((pos = result.find('`', pos)) != std::string::npos) {
        std::string::size_type end = result.find('`', pos + 1);
        if (end != std::string::npos) {
            std::string subCommand = result.substr(pos + 1, end - pos - 1);
            
            FILE* pipe = popen(subCommand.c_str(), "r");
            if (!pipe) {
                std::cerr << "Error executing command substitution" << std::endl;
                pos = end + 1;
                continue;
            }
            
            std::string output;
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
            }
            pclose(pipe);
            
            while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
                output.pop_back();
            }
            
            result.replace(pos, end - pos + 1, output);
            pos += output.length();
        } else {
            break;
        }
    }
    
    return result;
}

void loadAliasesFromFile(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        return;
    }
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        if (line.find("alias ") == 0) {
            line = line.substr(6);
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string name = line.substr(0, eqPos);
                std::string value = line.substr(eqPos + 1);
                
                name.erase(0, name.find_first_not_of(" \t"));
                name.erase(name.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                if (value.size() >= 2 && 
                    ((value.front() == '"' && value.back() == '"') || 
                     (value.front() == '\'' && value.back() == '\''))) {
                    value = value.substr(1, value.size() - 2);
                }
                
                aliases[name] = value;

                std::string aliasCmd = "alias " + name + "='" + value + "'";
                sendTerminalCommand(aliasCmd);
            }
        }
    }
    
    file.close();
}

void saveAliasToCJSHRC(const std::string& name, const std::string& value) {
    std::filesystem::path cjshrcPath = CJSHRC_FILE;

    if (!std::filesystem::exists(cjshrcPath)) {
        createDefaultCJSHRC();
    }
    
    std::ifstream inFile(cjshrcPath);
    std::string content;
    if (inFile.is_open()) {
        std::string line;
        bool aliasExists = false;
        std::stringstream newContent;
        
        while (std::getline(inFile, line)) {
            if (line.find("alias " + name + "=") == 0) {
                newContent << "alias " << name << "='" << value << "'" << std::endl;
                aliasExists = true;
            } else {
                newContent << line << std::endl;
            }
        }
        
        if (!aliasExists) {
            newContent << "alias " << name << "='" << value << "'" << std::endl;
        }
        
        content = newContent.str();
        inFile.close();
    } else {
        content = "# CJ's Shell RC File\n\n";
        content += "alias " + name + "='" + value + "'\n";
    }
    
    std::ofstream outFile(cjshrcPath, std::ios::trunc);
    if (outFile.is_open()) {
        outFile << content;
        outFile.close();
    } else {
        std::cerr << "Error: Could not save alias to " << cjshrcPath << std::endl;
    }
}

void saveEnvironmentVariableToCJSHRC(const std::string& name, const std::string& value) {
    std::filesystem::path cjshrcPath = CJSHRC_FILE;

    if (!std::filesystem::exists(cjshrcPath)) {
        createDefaultCJSHRC();
    }
    
    std::ifstream inFile(cjshrcPath);
    std::string content;
    if (inFile.is_open()) {
        std::string line;
        bool exportExists = false;
        std::stringstream newContent;
        
        while (std::getline(inFile, line)) {
            if (line.find("export " + name + "=") == 0) {
                newContent << "export " << name << "='" << value << "'" << std::endl;
                exportExists = true;
            } else {
                newContent << line << std::endl;
            }
        }
        
        if (!exportExists) {
            newContent << "export " << name << "='" << value << "'" << std::endl;
        }
        
        content = newContent.str();
        inFile.close();
    } else {
        content = "# CJ's Shell RC File\n\n";
        content += "export " + name + "='" + value + "'\n";
    }
    
    std::ofstream outFile(cjshrcPath, std::ios::trunc);
    if (outFile.is_open()) {
        outFile << content;
        outFile.close();
    } else {
        std::cerr << "Error: Could not save environment variable to " << cjshrcPath << std::endl;
    }
}

void createNewUSER_DATAFile() {
    std::ofstream file(USER_DATA);
    if (file.is_open()) {
        writeUserData();
        file.close();
    }
}

void createNewUSER_HISTORYfile() {
    std::ofstream file(USER_COMMAND_HISTORY);
    if (!file.is_open()) {
        return;
    }
}

void writeUserData() {
    std::ofstream file(USER_DATA);
    if (file.is_open()) {
        json userData;
        userData["OpenAI_API_KEY"] = c_assistant.getAPIKey();
        userData["Chat_Cache"] = savedChatCache;
        userData["Startup_Commands"] = startupCommands;
        userData["Shortcuts_Enabled"] = shortcutsEnabled;
        userData["Text_Buffer"] = false;
        userData["Text_Entry"] = defaultTextEntryOnAI;
        userData["Shortcuts_Prefix"] = shortcutsPrefix;
        userData["Multi_Script_Shortcuts"] = multiScriptShortcuts;
        userData["Last_Updated"] = lastUpdated;
        userData["Current_Theme"] = currentTheme;
        userData["Auto_Update_Check"] = checkForUpdates;
        userData["Update_From_Github"] = updateFromGithub;
        userData["Silent_Update_Check"] = silentCheckForUpdates;
        userData["Startup_Commands_Enabled"] = startCommandsOn;
        userData["Last_Update_Check_Time"] = lastUpdateCheckTime;
        userData["Update_Check_Interval"] = UPDATE_CHECK_INTERVAL;
        file << userData.dump(4);
        file.close();
    } else {
        std::cerr << "Error: Unable to write to the user data file at " << USER_DATA << std::endl;
    }
}

void goToApplicationDirectory() {
    commandProcesser("terminal cd /");
    commandProcesser("terminal cd " + DATA_DIRECTORY.string());
}

std::string readAndReturnUserDataFile() {
    std::ifstream file(USER_DATA);
    if (file.is_open()) {
        std::string userData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        return userData.empty() ? "No data found." : userData;
    } else {
        std::cerr << "Error: Unable to read the user data file at " << USER_DATA << std::endl;
        return "";
    }
}

std::vector<std::string> commandSplicer(const std::string& command) {
    std::vector<std::string> commands;
    std::istringstream iss(command);
    std::string word;
    std::string combined;
    bool inQuotes = false;
    char quoteChar = '\0';

    while (iss >> word) {
        if (!inQuotes && (word.front() == '\'' || word.front() == '"' || word.front() == '(' || word.front() == '[')) {
            inQuotes = true;
            quoteChar = word.front();
            combined = word.substr(1);
        } else if (inQuotes && word.back() == quoteChar) {
            combined += " " + word.substr(0, word.size() - 1);
            commands.push_back(combined);
            inQuotes = false;
            quoteChar = '\0';
        } else if (inQuotes) {
            combined += " " + word;
        } else {
            commands.push_back(word);
        }
    }

    if (inQuotes) {
        commands.push_back(combined);
    }

    return commands;
}

void commandParser(const std::string& command) {
    // easy early breaks
    if (command.empty()) {
        return;
    }
    if (command == "exit" || command == "quit") {
        exitFlag = true;
        return;
    }
    if (command == "clear") {
        sendTerminalCommand("clear");
        return;
    }

    // ai mode
    if (defaultTextEntryOnAI && command != "terminal") {
        chatProcess(command);
        terminal.addCommandToHistory(command);
        return; // Add return to prevent processing as a shell command
    }

    // check if the command is a multi-script shortcut
    if (command.rfind(shortcutsPrefix, 0) == 0) {
        multiScriptShortcutProcesser(command);
        return;
    }
    
    // check if the command has history expansion
    std::string expandedCommand = expandHistoryCommand(command);
    if (expandedCommand.empty()) {
        return;
    }
    
    // do command substitution and environment variable expansion
    expandedCommand = performCommandSubstitution(expandedCommand);
    expandedCommand = expandEnvVariables(expandedCommand);
    
    // If we get here, process the command directly
    commandProcesser(expandedCommand); // Use expandedCommand instead of original command
}

void addUserInputToHistory(const std::string& input) {
    std::ofstream file(USER_COMMAND_HISTORY, std::ios_base::app);
    if (file.is_open()) {
        file << std::to_string(time(nullptr)) << " " << input << "\n";
        file.close();
    } else {
        std::cerr << "Error: Unable to write to the user input history file at " << USER_COMMAND_HISTORY << std::endl;
    }
}

inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

inline void trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}

void multiScriptShortcutProcesser(const std::string& command){
    if (!shortcutsEnabled) {
        std::cerr << "Shortcuts are disabled." << std::endl;
        return;
    }
    if (!multiScriptShortcuts.empty()) {
        std::string strippedCommand = command.substr(1);
        trim(strippedCommand);
        if (strippedCommand.empty()) {
            std::cerr << "No shortcut given." << std::endl;
            return;
        }
        if (multiScriptShortcuts.find(strippedCommand) != multiScriptShortcuts.end()) {
            std::vector<std::string> commands = multiScriptShortcuts[strippedCommand];
            for (const auto& cmd : commands) {
                commandParser(cmd);
            }
        } else {
            std::cerr << "No command for given shortcut: " << strippedCommand << std::endl;
        }
    } else {
        std::cerr << "No shortcuts have been created." << std::endl;
    }
}

void commandProcesser(const std::string& originalPassedCommand) {
    commandsQueue = std::queue<std::string>();
    auto commands = commandSplicer(originalPassedCommand);

    for (const auto& cmd : commands) {
        commandsQueue.push(cmd);
    }
    
    if (TESTING) {
        std::cout << "Commands Queue: ";
        for (const auto& cmd : commands) {
            std::cout << cmd << " ";
        }
        std::cout << std::endl;
    }
    if (commandsQueue.empty()) {
        std::cerr << "Unknown command. Please try again." << std::endl;
    }

    getNextCommand();
    if (lastCommandParsed == "approot") {
        goToApplicationDirectory();
        return;
    } else if (lastCommandParsed == "ai") {
        aiSettingsCommands();
        writeUserData();
        return;
    } else if (lastCommandParsed == "user") {
        userSettingsCommands();
        writeUserData();
        return;
    } else if (lastCommandParsed == "aihelp"){
        if (!defaultTextEntryOnAI && !c_assistant.getAPIKey().empty() ){
            std::string message = ("I am encountering these errors in the " + terminal.getTerminalName() + " and would like some help solving these issues. I entered: " + terminal.returnMostRecentUserInput() + " and got this " + terminal.returnMostRecentTerminalOutput());
            if (TESTING) {
                std::cout << message << std::endl;
            }
            std::cout << c_assistant.forceDirectChatGPT(message, false) << std::endl;
            return;
        }
        std::cout << "AI help is only available in AI mode." << std::endl;
        return;
    } else if(lastCommandParsed == "version") {
        std::cout << "CJ's Shell v" + currentVersion << std::endl;
        return;
    } else if (lastCommandParsed == "terminal") {
        defaultTextEntryOnAI = false;
        return;
    } else if(lastCommandParsed == "plugin") {
        pluginCommands();
        return;
    } else if (lastCommandParsed == "theme") {
        themeCommands();
        return;
    } else if (lastCommandParsed == "history") {
        auto history = terminal.getTerminalCacheUserInput();
        if (history.empty()) {
            std::cout << "No command history" << std::endl;
        } else {
            for (size_t i = 0; i < history.size(); i++) {
                std::cout << (i + 1) << "  " << history[i] << std::endl;
            }
        }
        return;
    } else if (lastCommandParsed == "help") {
        printHelp();
        return;
    } else if (lastCommandParsed == "uninstall") {
        if (pluginManager->getEnabledPlugins().size() > 0) {
            std::cerr << "Please disable all plugins before uninstalling." << std::endl;
            return;
        }
        std::cout << "Are you sure you want to uninstall cjsh? (y/n): ";
        char confirmation;
        std::cin >> confirmation;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        if (confirmation == 'y' || confirmation == 'Y') {
            if(!std::filesystem::exists(UNINSTALL_SCRIPT_PATH)){
                std::cerr << "Uninstall script not found." << std::endl;
                return;
            }
            std::cout << "Do you want to remove all user data? (y/n): ";
            char removeUserData;
            std::cin >> removeUserData;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::string uninstallCommand = UNINSTALL_SCRIPT_PATH;
            if (removeUserData == 'y' || removeUserData == 'Y') {
                uninstallCommand += " --all";
            }
            std::cout << "Running uninstall script..." << std::endl;
            sendTerminalCommand(uninstallCommand);
            exitFlag = true;
        } else {
            std::cout << "Uninstall cancelled." << std::endl;
        }
        return;
    } else {

        std::vector<std::string> enabledPlugins = pluginManager->getEnabledPlugins();
        if (!enabledPlugins.empty()) {
            std::queue<std::string> tempQueue;
            tempQueue.push(lastCommandParsed);
            while (!commandsQueue.empty()) {
                tempQueue.push(commandsQueue.front());
                commandsQueue.pop();
            }
            for(const auto& plugin : enabledPlugins){
                std::vector<std::string> pluginCommands = pluginManager->getPluginCommands(plugin);
                if(std::find(pluginCommands.begin(), pluginCommands.end(), lastCommandParsed) != pluginCommands.end()){
                    pluginManager->handlePluginCommand(plugin, tempQueue);
                    return;
                }
            }
        }

        //send to terminal
        sendTerminalCommand(originalPassedCommand);
        
        const char* saveAlias = getenv("CJSH_SAVE_ALIAS");
        if (saveAlias && strcmp(saveAlias, "1") == 0) {
            const char* aliasName = getenv("CJSH_SAVE_ALIAS_NAME");
            const char* aliasValue = getenv("CJSH_SAVE_ALIAS_VALUE");
                
            if (aliasName && aliasValue) {
                saveAliasToCJSHRC(aliasName, aliasValue);
                    
                unsetenv("CJSH_SAVE_ALIAS");
                unsetenv("CJSH_SAVE_ALIAS_NAME");
                unsetenv("CJSH_SAVE_ALIAS_VALUE");
            }
        }

        const char* saveEnv = getenv("CJSH_SAVE_ENV");
        if (saveEnv && strcmp(saveEnv, "1") == 0) {
            const char* envName = getenv("CJSH_SAVE_ENV_NAME");
            const char* envValue = getenv("CJSH_SAVE_ENV_VALUE");
                
            if (envName && envValue) {
                saveEnvironmentVariableToCJSHRC(envName, envValue);
                    
                unsetenv("CJSH_SAVE_ENV");
                unsetenv("CJSH_SAVE_ENV_NAME");
                unsetenv("CJSH_SAVE_ENV_VALUE");
            }
        }
    }
}

void printHelp() {
    // print available startup arguements
    std::cout << "Available startup arguments:" << std::endl;
    std::cout << " -h, --help: Show this help message" << std::endl;
    std::cout << " -v, --version: Show the version of the application" << std::endl;
    std::cout << " -d, --debug: Enable debug mode" << std::endl;
    std::cout << " -c, --command: Specify a command to execute" << std::endl;
    std::cout << " -l, --login: Run as a login shell" << std::endl;

    // print available interactive session commands
    std::cout << " Available interactive session commands:" << std::endl;
    std::cout << " ai: Access AI command settings and chat or switch to the ai menu" << std::endl;
    std::cout << " approot: Switch to the application directory" << std::endl;
    std::cout << " user: Access user settings" << std::endl;
    std::cout << " aihelp: Get AI troubleshooting help" << std::endl;
    std::cout << " theme: Manage themes (load/save)" << std::endl;
    std::cout << " version: Display application version" << std::endl;
    std::cout << " plugin: Manage plugins" << std::endl;
    std::cout << " env: Manage environment variables" << std::endl;
    std::cout << " uninstall: Uninstall the application" << std::endl;
    std::cout << " history: Display command history" << std::endl;

    //print unix executable commands
    std::cout << " Unix executable commands:" << std::endl;
    std::cout << " clear: Clear the terminal screen" << std::endl;
    std::cout << " exit: Exit the application" << std::endl;
    std::cout << " quit: Exit the application" << std::endl;
    std::cout << " help: Show this help message" << std::endl;
    std::cout << " <command>: Execute a command in the terminal" << std::endl;
    std::cout << " !!: Repeat the last command" << std::endl;
    std::cout << " !<number>: Repeat the command at the specified history number" << std::endl;
    std::cout << " !<string>: Repeat the last command that starts with the specified string" << std::endl;
    std::cout << " $<variable>: Expand the specified environment variable" << std::endl;
    std::cout << " $(<command>): Execute the specified command and replace it with its output" << std::endl;
    std::cout << " `command`: Execute the specified command and replace it with its output" << std::endl;
    std::cout << " <alias>: Execute the specified alias" << std::endl;
    std::cout << " <shortcut>: Execute the specified multi-script shortcut" << std::endl;
    std::cout << " <file>: Execute the specified file (if executable)" << std::endl;
    std::cout << " <file>.sh: Execute the specified shell script" << std::endl;
    std::cout << " <file>.py: Execute the specified Python script" << std::endl;
}

void pluginCommands(){
    getNextCommand();
    if(lastCommandParsed.empty()) {
        std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    }
    if(lastCommandParsed == "help") {
        std::cout << "Plugin commands:" << std::endl;
        std::cout << " available: List available plugins" << std::endl;
        std::cout << " enabled: List enabled plugins" << std::endl;
        std::cout << " enable [NAME]: Enable a plugin" << std::endl;
        std::cout << " disable [NAME]: Disable a plugin" << std::endl;
        std::cout << " info [NAME]: Show plugin information" << std::endl;
        std::cout << " commands [NAME]: List commands for a plugin" << std::endl;
        std::cout << " settings [NAME] set [SETTING] [VALUE]: Modify a plugin setting" << std::endl;
        std::cout << " help: Show this help message" << std::endl;
        std::cout << " install [PATH]: Install a new plugin" << std::endl;
        std::cout << " uninstall [NAME]: Remove an installed plugin" << std::endl;
        return;
    }
    if(lastCommandParsed == "install") {
        getNextCommand();
        if(lastCommandParsed.empty()) {
            std::cerr << "Error: No plugin file path provided." << std::endl;
            return;
        }
        std::string pluginPath = terminal.getFullPathOfFile(lastCommandParsed);
        pluginManager->installPlugin(pluginPath);
        return;
    }
    if(lastCommandParsed == "uninstall") {
        getNextCommand();
        if(lastCommandParsed.empty()) {
            std::cerr << "Error: No plugin name provided." << std::endl;
            return;
        }
        std::string pluginName = lastCommandParsed;
        pluginManager->uninstallPlugin(pluginName);
        return;
    }
    if(lastCommandParsed == "info") {
        getNextCommand();
        if(lastCommandParsed.empty()) {
            std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        std::string pluginToGetInfo = lastCommandParsed;
        std::cout << pluginManager->getPluginInfo(pluginToGetInfo) << std::endl;
        return;
    }
    if(lastCommandParsed == "available") {
        auto plugins = pluginManager->getAvailablePlugins();
        std::cout << "Available plugins:" << std::endl;
        for(const auto& name : plugins) {
            std::cout << name << std::endl;
        }
        return;
    }
    if(lastCommandParsed == "enabled") {
        auto plugins = pluginManager->getEnabledPlugins();
        std::cout << "Enabled plugins:" << std::endl;
        for(const auto& name : plugins) {
            std::cout << name << std::endl;
        }
        return;
    }
    if(lastCommandParsed == "settings") {
        std::cout << "Settings for plugins:" << std::endl;
        auto settings = pluginManager->getAllPluginSettings();
        for (const auto& [plugin, settingMap] : settings) {
            std::cout << plugin << ":" << std::endl;
            for (const auto& [key, value] : settingMap) {
                std::cout << "  " << key << " = " << value << std::endl;
            }
        }
        return;
    }
    if(lastCommandParsed == "enableall") {
        for(const auto& plugin : pluginManager->getAvailablePlugins()){
            pluginManager->enablePlugin(plugin);
        }
        return;
    }
    if(lastCommandParsed == "disableall") {
        for(const auto& plugin : pluginManager->getEnabledPlugins()){
            pluginManager->disablePlugin(plugin);
        }
        return;
    }
    std::string pluginToModify = lastCommandParsed;
    std::vector<std::string> enabledPlugins = pluginManager->getEnabledPlugins();
    if (std::find(enabledPlugins.begin(), enabledPlugins.end(), pluginToModify) != enabledPlugins.end()) {
        getNextCommand();
        if(lastCommandParsed == "enable") {
            pluginManager->enablePlugin(pluginToModify);
            return;
        }
        if(lastCommandParsed == "disable") {
            pluginManager->disablePlugin(pluginToModify);
            return;
        }
        if(lastCommandParsed == "info") {
            std::cout << pluginManager->getPluginInfo(pluginToModify) << std::endl;
            return;
        }
        if(lastCommandParsed == "commands" || lastCommandParsed == "cmds" || lastCommandParsed == "help") {
            std::cout << "Commands for " << pluginToModify << ":" << std::endl;
            std::vector<std::string> listOfPluginCommands = pluginManager->getPluginCommands(pluginToModify);
            for (const auto& cmd : listOfPluginCommands) {
                std::cout << "  " << cmd << std::endl;
            }
            return;
        }
        if(lastCommandParsed == "settings") {
            getNextCommand();
            if(lastCommandParsed.empty()) {
                std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
                return;
            }
            if(lastCommandParsed == "set") {
                getNextCommand();
                if(lastCommandParsed.empty()) {
                    std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
                    return;
                }
                std::string settingToModify = lastCommandParsed;
                getNextCommand();
                if(lastCommandParsed.empty()) {
                    std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
                    return;
                }
                std::string settingValue = lastCommandParsed;
                if(pluginManager->updatePluginSetting(pluginToModify, settingToModify, settingValue)){
                    std::cout << "Setting " << settingToModify << " set to " << settingValue << " for plugin " << pluginToModify << std::endl;
                    return;
                } else {
                    std::cout << "Setting " << settingToModify << " not found for plugin " << pluginToModify << std::endl;
                    return;
                }
            }
        }
        std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    } else {
        std::vector<std::string> availablePlugins = pluginManager->getAvailablePlugins();
        if(std::find(availablePlugins.begin(), availablePlugins.end(), pluginToModify) != availablePlugins.end()){
            getNextCommand();
            if(lastCommandParsed == "enable") {
                pluginManager->enablePlugin(pluginToModify);
                return;
            }
            std::cerr << "Plugin: "<< pluginToModify << " is disabled." << std::endl;
            return;
        } else {
            std::cerr << "Plugin " << pluginToModify << " does not exist." << std::endl;
            return;
        }
    }
}

void sendTerminalCommand(const std::string& command) {
    if (TESTING) {
        std::cout << "Sending Command: " << command << std::endl;
    }

    std::string expandedCommand = expandEnvVariables(command);
    
    if (TESTING && expandedCommand != command) {
        std::cout << "Expanded Command: " << expandedCommand << std::endl;
    }
    
    terminal.setAliases(aliases);
    std::thread commandThread = terminal.executeCommand(expandedCommand);
    if (commandThread.joinable()) {
        commandThread.join();
    }
}

void userSettingsCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    }
    if (lastCommandParsed == "startup") {
        startupCommandsHandler();
        return;
    }
    if (lastCommandParsed == "text") {
        textCommands();
        return;
    }
    if (lastCommandParsed == "shortcut") {
        shortcutCommands();
        return;
    }
    if (lastCommandParsed == "testing") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        if (lastCommandParsed == "enable") {
            TESTING = true;
            std::cout << "Testing mode enabled." << std::endl;
            return;
        }
        if (lastCommandParsed == "disable") {
            TESTING = false;
            std::cout << "Testing mode disabled." << std::endl;
            return;
        }
        std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    }
    if (lastCommandParsed == "data") {
        userDataCommands();
        return;
    }
    if(lastCommandParsed == "saveloop"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Save loop is currently " << (saveLoop ? "enabled." : "disabled.") << std::endl;
            return;
        }
        if (lastCommandParsed == "enable") {
            saveLoop = true;
            std::cout << "Save loop enabled." << std::endl;
            return;
        }
        if (lastCommandParsed == "disable") {
            saveLoop = false;
            std::cout << "Save loop disabled." << std::endl;
            return;
        }
    }
    if(lastCommandParsed == "saveonexit"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Save on exit is currently " << (saveOnExit ? "enabled." : "disabled.") << std::endl;
            return;
        }
        if (lastCommandParsed == "enable") {
            saveOnExit = true;
            std::cout << "Save on exit enabled." << std::endl;
            return;
        }
        if (lastCommandParsed == "disable") {
            saveOnExit = false;
            std::cout << "Save on exit disabled." << std::endl;
            return;
        }
    }
    if(lastCommandParsed == "checkforupdates"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Check for updates is currently " << (checkForUpdates ? "enabled." : "disabled.") << std::endl;
            return;
        }
        if (lastCommandParsed == "enable") {
            checkForUpdates = true;
            std::cout << "Check for updates enabled." << std::endl;
            return;
        }
        if (lastCommandParsed == "disable") {
            checkForUpdates = false;
            std::cout << "Check for updates disabled." << std::endl;
            return;
        }
    }
    if(lastCommandParsed == "silentupdatecheck") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Silent update check is currently " << (silentCheckForUpdates ? "enabled." : "disabled.") << std::endl;
            return;
        }
        if (lastCommandParsed == "enable") {
            silentCheckForUpdates = true;
            std::cout << "Silent update check enabled." << std::endl;
            return;
        }
        if (lastCommandParsed == "disable") {
            silentCheckForUpdates = false;
            std::cout << "Silent update check disabled." << std::endl;
            return;
        }
    }
    if (lastCommandParsed == "updatepath") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Updates are fetched from GitHub." << std::endl;
            return;
        }
        std::cerr << "Updates are only available from GitHub." << std::endl;
        return;
    }
    if(lastCommandParsed == "update") {
        updateCommands();
        return;
    }
    if (lastCommandParsed == "help") {
        std::cout << "User settings commands:" << std::endl;
        std::cout << " startup: Manage startup commands (add, remove, clear, enable, disable, list, runall)" << std::endl;
        std::cout << " text: Configure text settings (displayfullpath, defaultentry, shortcutsprefix)" << std::endl;
        std::cout << " shortcut: Manage shortcuts (add, remove, list)" << std::endl;
        std::cout << " testing: Toggle testing mode (enable/disable)" << std::endl;
        std::cout << " data: Manage user data (get userdata/userhistory/all, clear)" << std::endl;
        std::cout << " saveloop: Toggle auto-save loop (enable/disable)" << std::endl;
        std::cout << " saveonexit: Toggle save on exit (enable/disable)" << std::endl;
        std::cout << " checkforupdates: Toggle update checking (enable/disable)" << std::endl;
        std::cout << " updatepath: Set update path (github/cadenfinley)" << std::endl;
        std::cout << " silentupdatecheck: Toggle silent update check (enable/disable)" << std::endl;
        std::cout << " update: Manage update settings and perform manual update checks" << std::endl;
        return;
    }
    std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
}

void updateCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cout << "Update settings:" << std::endl;
        std::cout << " Auto-check for updates: " << (checkForUpdates ? "Enabled" : "Disabled") << std::endl;
        std::cout << " Silent update check: " << (silentCheckForUpdates ? "Enabled" : "Disabled") << std::endl;
        std::cout << " Update check interval: " << (UPDATE_CHECK_INTERVAL / 3600) << " hours" << std::endl;
        std::cout << " Last update check: " << (lastUpdateCheckTime > 0 ? 
            std::string(ctime(&lastUpdateCheckTime)) : "Never") << std::endl;
        if (cachedUpdateAvailable) {
            std::cout << " Update available: " << cachedLatestVersion << std::endl;
        }
        return;
    }
    
    if (lastCommandParsed == "check") {
        manualUpdateCheck();
        return;
    }
    
    if (lastCommandParsed == "interval") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Current update check interval: " << (UPDATE_CHECK_INTERVAL / 3600) << " hours" << std::endl;
            return;
        }
        
        try {
            int hours = std::stoi(lastCommandParsed);
            if (hours < 1) {
                std::cerr << "Interval must be at least 1 hour" << std::endl;
                return;
            }
            setUpdateInterval(hours);
            std::cout << "Update check interval set to " << hours << " hours" << std::endl;
            return;
        } catch (const std::exception& e) {
            std::cerr << "Invalid interval value. Please specify hours as a number" << std::endl;
            return;
        }
    }
    
    if (lastCommandParsed == "help") {
        std::cout << "Update commands:" << std::endl;
        std::cout << " check: Manually check for updates now" << std::endl;
        std::cout << " interval [HOURS]: Set update check interval in hours" << std::endl;
        std::cout << " help: Show this help message" << std::endl;
        return;
    }
    
    std::cerr << "Unknown update command. Try 'help' for available commands." << std::endl;
}

void manualUpdateCheck() {
    std::cout << "Checking for updates now..." << std::endl;
    bool updateAvailable = checkForUpdate();
    
    if (updateAvailable) {
        std::cout << "An update is available!" << std::endl;
        executeUpdateIfAvailable(true);
    } else {
        std::cout << "You are up to date." << std::endl;
    }
    
    std::string latestVersion = "";
    try {
        std::string command = "curl -s " + updateURL_Github;
        std::string result;
        FILE *pipe = popen(command.c_str(), "r");
        if (pipe) {
            char buffer[128];
            while (fgets(buffer, 128, pipe) != nullptr) {
                result += buffer;
            }
            pclose(pipe);
            
            json jsonData = json::parse(result);
            if (jsonData.contains("tag_name")) {
                latestVersion = jsonData["tag_name"].get<std::string>();
                if (!latestVersion.empty() && latestVersion[0] == 'v') {
                    latestVersion = latestVersion.substr(1);
                }
            }
        }
    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    saveUpdateCache(updateAvailable, latestVersion);
}

void setUpdateInterval(int intervalHours) {
    UPDATE_CHECK_INTERVAL = intervalHours * 3600;
    writeUserData();
}

void userDataCommands(){
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cerr << "Error: No arguments provided. Try 'help' for a list of commands." << std::endl;
        return;
    }
    if (lastCommandParsed == "get") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Error: No arguments provided. Try 'help' for a list of commands." << std::endl;
            return;
        }
        if (lastCommandParsed == "userdata") {
            std::cout << readAndReturnUserDataFile() << std::endl;
            return;
        }
        if (lastCommandParsed == "userhistory") {
            std::ifstream file(USER_COMMAND_HISTORY);
            if (file.is_open()) {
                std::string history((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                file.close();
                std::cout << history << std::endl;
            } else {
                std::cerr << "Error: Unable to read the user history file at " << USER_COMMAND_HISTORY << std::endl;
            }
            return;
        }
        if (lastCommandParsed == "all") {
            std::cout << readAndReturnUserDataFile() << std::endl;
            std::ifstream file(USER_COMMAND_HISTORY);
            if (file.is_open()) {
                std::string history((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                file.close();
                std::cout << history << std::endl;
            } else {
                std::cerr << "Error: Unable to read the user history file at " << USER_COMMAND_HISTORY << std::endl;
            }
            return;
        }
    }
    if (lastCommandParsed == "clear") {
        std::filesystem::remove(USER_DATA);
        createNewUSER_DATAFile();
        std::cout << "User data file cleared." << std::endl;
        std::filesystem::remove(USER_COMMAND_HISTORY);
        createNewUSER_HISTORYfile();
        std::cout << "User history file cleared." << std::endl;
        return;
    }
    if(lastCommandParsed == "help") {
        std::cout << "User data commands: " << std::endl;
        std::cout << " get: View user data" << std::endl;
        std::cout << "  userdata: View JSON user data file" << std::endl;
        std::cout << "  userhistory: View command history" << std::endl;
        std::cout << "  all: View all user data" << std::endl;
        std::cout << " clear: Clear all user data files" << std::endl;
        return;
    }
    std::cerr << "Error: Unknown command. Try 'help' for a list of commands." << std::endl;
}

void startupCommandsHandler() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        if (!startupCommands.empty()) {
            std::cout << "Startup commands:" << std::endl;
            for (const auto& command : startupCommands) {
                std::cout << command << std::endl;
            }
        } else {
            std::cerr << "No startup commands." << std::endl;
        }
        return;
    }
    if (lastCommandParsed == "add") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        startupCommands.push_back(lastCommandParsed);
        std::cout << "Command added to startup commands." << std::endl;
        return;
    }
    if (lastCommandParsed == "remove") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        std::vector<std::string> newStartupCommands;
        bool removed = false;
        for (const auto& command : startupCommands) {
            if (command != lastCommandParsed) {
                newStartupCommands.push_back(command);
            } else {
                removed = true;
                std::cout << "Command removed from startup commands." << std::endl;
            }
        }
        if (!removed) {
            std::cerr << "Command not found in startup commands." << std::endl;
        }
        startupCommands = newStartupCommands;
        return;
    }
    if (lastCommandParsed == "clear") {
        startupCommands.clear();
        std::cout << "Startup commands cleared." << std::endl;
        return;
    }
    if (lastCommandParsed == "enable") {
        startCommandsOn = true;
        std::cout << "Startup commands enabled." << std::endl;
        return;
    }
    if (lastCommandParsed == "disable") {
        startCommandsOn = false;
        std::cout << "Startup commands disabled." << std::endl;
        return;
    }
    if (lastCommandParsed == "list") {
        if (!startupCommands.empty()) {
            std::cout << "Startup commands:" << std::endl;
            for (const auto& command : startupCommands) {
                std::cout << command << std::endl;
            }
        } else {
            std::cerr << "No startup commands." << std::endl;
        }
        return;
    }
    if (lastCommandParsed == "runall") {
        if (!startupCommands.empty()) {
            std::cout << "Running startup commands..." << std::endl;
            for (const auto& command : startupCommands) {
                commandParser(command);
            }
        } else {
            std::cerr << "No startup commands." << std::endl;
        }
        return;
    }
    if (lastCommandParsed == "help") {
        std::cout << "Startup commands:" << std::endl;
        std::cout << " add [CMD]: Add a startup command" << std::endl;
        std::cout << " remove [CMD]: Remove a startup command" << std::endl;
        std::cout << " clear: Remove all startup commands" << std::endl;
        std::cout << " enable: Enable startup commands" << std::endl;
        std::cout << " disable: Disable startup commands" << std::endl;
        std::cout << " list: Show all startup commands" << std::endl;
        std::cout << " runall: Execute all startup commands now" << std::endl;
        return;
    }
    std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
}

void shortcutCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        if(!multiScriptShortcuts.empty()){
            std::cout << "Shortcuts:" << std::endl;
            for (const auto& [key, value] : multiScriptShortcuts) {
                std::cout << key + " = ";
                for(const auto& command : value){
                    std::cout << "'"+command + "' ";
                }
                std::cout << std::endl;
            }
        } else {
            std::cerr << "No shortcuts." << std::endl;
        }
        return;
    }
    if (lastCommandParsed == "enable") {
        shortcutsEnabled = true;
        std::cout << "Shortcuts enabled." << std::endl;
        return;
    }
    if (lastCommandParsed == "disable") {
        shortcutsEnabled = false;
        std::cout << "Shortcuts disabled." << std::endl;
        return;
    }
    if (lastCommandParsed == "list") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            if(!multiScriptShortcuts.empty()){
                std::cout << "Shortcuts:" << std::endl;
                for (const auto& [key, value] : multiScriptShortcuts) {
                    std::cout << key + " = ";
                    for(const auto& command : value){
                        std::cout << "'"+command + "' ";
                    }
                    std::cout << std::endl;
                }
            } else {
                std::cerr << "No shortcuts." << std::endl;
            }
            return;
        }
        if (multiScriptShortcuts.find(lastCommandParsed) != multiScriptShortcuts.end()) {
            std::cout << lastCommandParsed + " = ";
            for(const auto& command : multiScriptShortcuts[lastCommandParsed]){
                std::cout << "'"+command + "' ";
            }
            std::cout << std::endl;
        } else {
            std::cerr << "Shortcut not found." << std::endl;
        }
        return;
    }
    if(lastCommandParsed == "add"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        std::string shortcut = lastCommandParsed;
        std::vector<std::string> commands;
        getNextCommand();
        while(!lastCommandParsed.empty()){
            commands.push_back(lastCommandParsed);
            getNextCommand();
        }
        if(commands.empty()){
            std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        multiScriptShortcuts[shortcut] = commands;
        std::cout << "Shortcut added." << std::endl;
        return;
    }
    if(lastCommandParsed == "remove"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        if (lastCommandParsed == "all") {
            multiScriptShortcuts.clear();
            std::cout << "All shortcuts removed." << std::endl;
            return;
        }
        if(multiScriptShortcuts.find(lastCommandParsed) == multiScriptShortcuts.end()){
            std::cerr << "Shortcut not found." << std::endl;
            return;
        }
        multiScriptShortcuts.erase(lastCommandParsed);
        std::cout << "Shortcut removed." << std::endl;
        return;
    }
    if (lastCommandParsed == "help") {
        std::cout << "Shortcut commands:" << std::endl;
        std::cout << " enable: Enable shortcuts" << std::endl;
        std::cout << " disable: Disable shortcuts" << std::endl;
        std::cout << " list: List all shortcuts" << std::endl;
        std::cout << " add [NAME] [CMD1] [CMD2] ... : Add a shortcut" << std::endl;
        std::cout << " remove [NAME]: Remove a shortcut" << std::endl;
        return;
    }
    std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
}

bool validatePrefix(const std::string& prefix) {
    if (prefix.length() > 1) {
        std::cerr << "Invalid prefix. Must be a single character." << std::endl;
        return false;
    } else if (prefix == " ") {
        std::cerr << "Invalid prefix. Must not be a space." << std::endl;
        return false;
    }
    return true;
}

void handleToggleCommand(const std::string& name, bool& setting, 
                        std::function<void(bool)> setter = nullptr) {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cout << name << " is currently " << (setting ? "enabled." : "disabled.") << std::endl;
        return;
    }
    
    if (lastCommandParsed == "enable") {
        setting = true;
        if (setter) setter(true);
        std::cout << name << " enabled." << std::endl;
    } else if (lastCommandParsed == "disable") {
        setting = false;
        if (setter) setter(false);
        std::cout << name << " disabled." << std::endl;
    } else {
        std::cerr << "Unknown option. Use 'enable' or 'disable'." << std::endl;
    }
}

void textCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    }
    
    if (lastCommandParsed == "shortcutsprefix") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Shortcut prefix is currently " + shortcutsPrefix << std::endl;
            return;
        }
        
        if (validatePrefix(lastCommandParsed)) {
            shortcutsPrefix = lastCommandParsed;
            std::cout << "Shortcut prefix set to " + shortcutsPrefix << std::endl;
        }
        return;
    }
    
    if (lastCommandParsed == "displayfullpath") {
        handleToggleCommand("Display whole path", displayWholePath, [&](bool value) { terminal.setDisplayWholePath(value); });
        return;
    }
    
    if (lastCommandParsed == "defaultentry") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Default text entry is currently " << (defaultTextEntryOnAI ? "AI." : "terminal.") << std::endl;
            return;
        }
        
        if (lastCommandParsed == "ai") {
            defaultTextEntryOnAI = true;
            std::cout << "Default text entry set to AI." << std::endl;
        } else if (lastCommandParsed == "terminal") {
            defaultTextEntryOnAI = false;
            std::cout << "Default text entry set to terminal." << std::endl;
        } else {
            std::cerr << "Unknown option. Use 'ai' or 'terminal'." << std::endl;
        }
        return;
    }
    
    if (lastCommandParsed == "help") {
        std::cout << "Text commands:" << std::endl;
        std::cout << " shortcutsprefix [CHAR]: Set the shortcut prefix" << std::endl;
        std::cout << " displayfullpath enable/disable: Toggle full path display" << std::endl;
        std::cout << " defaultentry ai/terminal: Set default text entry mode" << std::endl;
        return;
    }
    
    std::cerr << "Unknown command. Use 'help' for available commands." << std::endl;
}

void getNextCommand() {
    if (!commandsQueue.empty()) {
        lastCommandParsed = commandsQueue.front();
        commandsQueue.pop();
        if (TESTING) {
            std::cout << "Processed Command: " << lastCommandParsed << std::endl;
        }
    } else {
        lastCommandParsed = "";
    }
}

void aiSettingsCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        defaultTextEntryOnAI = true;
        showChatHistory();
        return;
    }
    if (lastCommandParsed == "log") {
        std::string lastChatSent = c_assistant.getLastPromptUsed();
        std::string lastChatReceived = c_assistant.getLastResponseReceived();
        std::string fileName = (DATA_DIRECTORY / ("OpenAPI_Chat_" + std::to_string(time(nullptr)) + ".txt")).string();
        std::ofstream file(fileName);
        if (file.is_open()) {
            file << "Chat Sent: " << lastChatSent << "\n";
            file << "Chat Received: " << lastChatReceived << "\n";
            file.close();
            std::cout << "Chat log saved to " << fileName << std::endl;
        } else {
            std::cerr << "Error: Unable to create the chat log file at " << fileName << std::endl;
            return;
        }
    }
    if (lastCommandParsed == "apikey") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << c_assistant.getAPIKey() << std::endl;
            return;
        }
        if (lastCommandParsed == "set") {
            getNextCommand();
            if (lastCommandParsed.empty()) {
                std::cerr << "Error: No API key provided. Try 'help' for a list of commands." << std::endl;
                return;
            }
            c_assistant.setAPIKey(lastCommandParsed);
            if (c_assistant.testAPIKey(c_assistant.getAPIKey())) {
                std::cout << "OpenAI API key set successfully." << std::endl;
                writeUserData();
                return;
            } else {
                std::cerr << "Error: Invalid API key." << std::endl;
                return;
            }
        }
        if (lastCommandParsed == "get") {
            std::cout << c_assistant.getAPIKey() << std::endl;
            return;
        }
        std::cerr << "Error: Unknown command. Try 'help' for a list of commands." << std::endl;
        return;
    }
    if (lastCommandParsed == "chat") {
        aiChatCommands();
        return;
    }
    if (lastCommandParsed == "get") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Error: No arguments provided. Try 'help' for a list of commands." << std::endl;
            return;
        }
        std::cout << c_assistant.getResponseData(lastCommandParsed) << std::endl;
        return;
    }
    if (lastCommandParsed == "dump") {
        std::cout << c_assistant.getResponseData("all") << std::endl;
        std::cout << c_assistant.getLastPromptUsed() << std::endl;
        return;
    }
    if (lastCommandParsed == "mode") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "The current assistant mode is " << c_assistant.getAssistantType() << std::endl;
            return;
        }
        c_assistant.setAssistantType(lastCommandParsed);
        std::cout << "Assistant mode set to " << lastCommandParsed << std::endl;
        return;
    }
    if (lastCommandParsed == "file") {
        getNextCommand();
        std::vector<std::string> filesAtPath = terminal.getFilesAtCurrentPath(true, true, false);
        if (lastCommandParsed.empty()) {
            std::vector<std::string> activeFiles = c_assistant.getFiles();
            std::cout << "Active Files: " << std::endl;
            for(const auto& file : activeFiles){
                std::cout << file << std::endl;
            }
            std::cout << "Total characters processed: " << c_assistant.getFileContents().length() << std::endl;
            std::cout << "Files at current path: " << std::endl;
            for(const auto& file : filesAtPath){
                std::cout << file << std::endl;
            }
            return;
        }
        if (lastCommandParsed == "add"){
            getNextCommand();
            if (lastCommandParsed.empty()) {
                std::cerr << "Error: No file specified. Try 'help' for a list of commands." << std::endl;
                return;
            }
            if (lastCommandParsed == "all"){
                int charsProcessed = c_assistant.addFiles(filesAtPath);
                std::cout << "Processed " << charsProcessed <<  " characters from " << filesAtPath.size() << " files."  << std::endl;
                return;
            }
            std::string fileToAdd = terminal.getFullPathOfFile(lastCommandParsed);
            if(fileToAdd.empty()){
                std::cerr << "Error: File not found: " << lastCommandParsed << std::endl;
                return;
            }
            int charsProcessed = c_assistant.addFile(fileToAdd);
            std::cout << "Processed " << charsProcessed << " characters from file: " << lastCommandParsed << std::endl;
            return;
        }
        if (lastCommandParsed == "remove"){
            getNextCommand();
            if (lastCommandParsed.empty()) {
                std::cerr << "Error: No file specified. Try 'help' for a list of commands." << std::endl;
                return;
            }
            if (lastCommandParsed == "all"){
                int fileCount = c_assistant.getFiles().size();
                c_assistant.clearFiles();
                std::cout << "Removed all " << fileCount << " files from context." << std::endl;
                return;
            }
            std::string fileToRemove = terminal.getFullPathOfFile(lastCommandParsed);
            if(fileToRemove.empty()){
                std::cerr << "Error: File not found: " << lastCommandParsed << std::endl;
                return;
            }
            c_assistant.removeFile(fileToRemove);
            return;
        }
        if (lastCommandParsed == "active"){
            std::vector<std::string> activeFiles = c_assistant.getFiles();
            std::cout << "Active Files: " << std::endl;
            if(activeFiles.empty()) {
                std::cout << "  No active files." << std::endl;
            } else {
                for(const auto& file : activeFiles){
                    std::cout << "  " << file << std::endl;
                }
                std::cout << "Total characters processed: " << c_assistant.getFileContents().length() << std::endl;
            }
            return;
        }
        if (lastCommandParsed == "available"){
            std::cout << "Files at current path: " << std::endl;
            for(const auto& file : filesAtPath){
                std::cout << file << std::endl;
            }
            return;
        }
        if(lastCommandParsed == "refresh"){
            c_assistant.refreshFiles();
            std::cout << "Files refreshed." << std::endl;
            return;
        }
        if(lastCommandParsed == "clear"){
            c_assistant.clearFiles();
            std::cout << "Files cleared." << std::endl;
            return;
        }
        std::cerr << "Error: Unknown command. Try 'help' for a list of commands." << std::endl;
        return;
    }
    if(lastCommandParsed == "directory"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "The current directory is " << c_assistant.getSaveDirectory() << std::endl;
            return;
        }
        if(lastCommandParsed == "set") {
            c_assistant.setSaveDirectory(terminal.getCurrentFilePath());
            std::cout << "Directory set to " << terminal.getCurrentFilePath() << std::endl;
            return;
        }
        if(lastCommandParsed == "clear") {
            c_assistant.setSaveDirectory(DATA_DIRECTORY);
            std::cout << "Directory set to default." << std::endl;
            return;
        }
    }
    if(lastCommandParsed == "model"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "The current model is " << c_assistant.getModel() << std::endl;
            return;
        }
        c_assistant.setModel(lastCommandParsed);
        std::cout << "Model set to " << lastCommandParsed << std::endl;
        return;
    }
    if(lastCommandParsed == "rejectchanges"){
        c_assistant.rejectChanges();
        std::cout << "Changes rejected." << std::endl;
        return;
    }
    if(lastCommandParsed == "timeoutflag"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "The current timeout flag is " << c_assistant.getTimeoutFlagSeconds() << std::endl;
            return;
        }
        c_assistant.setTimeoutFlagSeconds(std::stoi(lastCommandParsed));
        std::cout << "Timeout flag set to " << lastCommandParsed << " seconds."<< std::endl;
        return;
    }
    if (lastCommandParsed == "help") {
        std::cout << "AI settings commands:" << std::endl;
        std::cout << " log: Save recent chat exchange to a file" << std::endl;
        std::cout << " apikey: Manage OpenAI API key (set/get)" << std::endl;
        std::cout << " chat: Access AI chat commands" << std::endl;
        std::cout << " get [KEY]: Retrieve specific response data" << std::endl;
        std::cout << " dump: Display all response data and last prompt" << std::endl;
        std::cout << " mode [TYPE]: Set the assistant mode" << std::endl;
        std::cout << " file: Manage files for context (add, remove, active, available, refresh, clear)" << std::endl;
        std::cout << " directory: Manage save directory (set, clear)" << std::endl;
        std::cout << " model [MODEL]: Set the AI model" << std::endl;
        std::cout << " rejectchanges: Reject AI suggested changes" << std::endl;
        std::cout << " timeoutflag [SECONDS]: Set the timeout duration" << std::endl;
        return;
    }
    std::cerr << "Error: Unknown command. Try 'help' for a list of commands." << std::endl;
}

void aiChatCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cerr << "Error: No arguments provided. Try 'help' for a list of commands." << std::endl;
        return;
    }
    if (lastCommandParsed == "history") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            showChatHistory();
            return;
        }
        if (lastCommandParsed == "clear") {
            c_assistant.clearChatCache();
            savedChatCache.clear();
            writeUserData();
            std::cout << "Chat history cleared." << std::endl;
            return;
        }
    }
    if (lastCommandParsed == "cache") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Error: No arguments provided. Try 'help' for a list of commands." << std::endl;
            return;
        }
        if (lastCommandParsed == "enable") {
            c_assistant.setCacheTokens(true);
            std::cout << "Cache tokens enabled." << std::endl;
            return;
        }
        if (lastCommandParsed == "disable") {
            c_assistant.setCacheTokens(false);
            std::cout << "Cache tokens disabled." << std::endl;
            return;
        }
        if (lastCommandParsed == "clear") {
            c_assistant.clearAllCachedTokens();
            std::cout << "Chat history cleared." << std::endl;
            return;
        }
    }
    if (lastCommandParsed == "help") {
        std::cout << "AI chat commands:" << std::endl;
        std::cout << " history: Show chat history" << std::endl;
        std::cout << " history clear: Clear chat history" << std::endl;
        std::cout << " cache enable: Enable token caching" << std::endl;
        std::cout << " cache disable: Disable token caching" << std::endl;
        std::cout << " cache clear: Clear all cached tokens" << std::endl;
        std::cout << " [MESSAGE]: Send a direct message to AI" << std::endl;
        return;
    }
    for(int i = 0; i < commandsQueue.size(); i++){
        lastCommandParsed += " " + commandsQueue.front();
        commandsQueue.pop();
    }
    std::cout << "Sent message to GPT: " << lastCommandParsed << std::endl;
    chatProcess(lastCommandParsed);
    return;
}

void chatProcess(const std::string& message) {
    if (message.empty()) {
        return;
    }
    if(message == "exit"){
        exitFlag = true;
        return;
    }
    if(message == "clear"){
        sendTerminalCommand("clear");
        return;
    }
    if (c_assistant.getAPIKey().empty()) {
        std::cerr << "Error: No OpenAPI key set. Please set the API key using 'ai apikey set [KEY]'." << std::endl;
        return;
    }
    std::string response = c_assistant.chatGPT(message,false);
    std::cout << "ChatGPT:\n" << response << std::endl;
}

void showChatHistory() {
    if (!c_assistant.getChatCache().empty()) {
        std::cout << "Chat history:" << std::endl;
        for (const auto& message : c_assistant.getChatCache()) {
            std::cout << message << std::endl;
        }
    }
}

bool checkFromUpdate_Github(std::function<bool(const std::string&, const std::string&)> isNewerVersion) {
    std::string command = "curl -s " + updateURL_Github;
    std::string result;
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Error: Unable to execute update check or no internet connection." << std::endl;
        return false;
    }
    char buffer[128];
    while (fgets(buffer, 128, pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    try {
        json jsonData = json::parse(result);
        if (jsonData.contains("tag_name")) {
            std::string latestTag = jsonData["tag_name"].get<std::string>();
            if (!latestTag.empty() && latestTag[0] == 'v') {
                latestTag = latestTag.substr(1);
            }
            std::string currentVer = currentVersion;
            if (!currentVer.empty() && currentVer[0] == 'v') {
                currentVer = currentVer.substr(1);
            }
            if (isNewerVersion(latestTag, currentVer)) {
                std::cout << "\nLast Updated: " << lastUpdated << std::endl;
                std::cout << currentVersion << " -> " << latestTag << std::endl;
                return true;
            }
        }
    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return false;
}

bool checkForUpdate() {
    if(!silentCheckForUpdates){
        std::cout << "Checking for updates from GitHub...";
    }
    auto isNewerVersion = [](const std::string &latest, const std::string &current) -> bool {
        auto splitVersion = [](const std::string &ver) {
            std::vector<int> parts;
            std::istringstream iss(ver);
            std::string token;
            while (std::getline(iss, token, '.')) {
                parts.push_back(std::stoi(token));
            }
            return parts;
        };
        std::vector<int> latestParts = splitVersion(latest);
        std::vector<int> currentParts = splitVersion(current);
        size_t len = std::max(latestParts.size(), currentParts.size());
        latestParts.resize(len, 0);
        currentParts.resize(len, 0);
        for (size_t i = 0; i < len; i++) {
            if (latestParts[i] > currentParts[i]) return true;
            if (latestParts[i] < currentParts[i]) return false;
        }
        return false;
    };

    return checkFromUpdate_Github(isNewerVersion);
    std::cerr << "Something went wrong." << std::endl;
    return false;
}

bool downloadLatestRelease() {
    std::cout << "Downloading latest release..." << std::endl;
    
    // Create temporary directory for the download
    std::filesystem::path tempDir = DATA_DIRECTORY / "temp_update";
    if (std::filesystem::exists(tempDir)) {
        std::filesystem::remove_all(tempDir);
    }
    std::filesystem::create_directory(tempDir);
    
    // Download the latest release using TerminalPassthrough
    std::string outputPath = (tempDir / "cjsh").string();
    std::string curlCommand = "curl -L -s https://github.com/CadenFinley/CJsShell/releases/latest/download/cjsh -o " + outputPath;
    
    std::thread downloadThread = terminal.executeCommand(curlCommand);
    downloadThread.join();
    
    if (!std::filesystem::exists(outputPath)) {
        std::cerr << "Error: Download failed - output file not created." << std::endl;
        return false;
    }
    
    // Make the downloaded file executable
    std::string chmodCommand = "chmod 755 " + outputPath;
    std::thread chmodThread = terminal.executeCommand(chmodCommand);
    chmodThread.join();
    
    // Try direct copy first
    bool updateSuccess = false;
    std::string installDir = INSTALL_PATH.parent_path().string();
    
    // Check if we can write to the install directory
    bool hasDirectWriteAccess = (access(installDir.c_str(), W_OK) == 0);
    
    if (hasDirectWriteAccess) {
        std::cout << "Installing update..." << std::endl;
        std::string cpCommand = "cp " + outputPath + " " + INSTALL_PATH.string();
        std::thread cpThread = terminal.executeCommand(cpCommand);
        cpThread.join();
        
        // Verify the copy succeeded by checking timestamps
        if (std::filesystem::exists(INSTALL_PATH)) {
            auto newFileTime = std::filesystem::last_write_time(outputPath);
            auto destFileTime = std::filesystem::last_write_time(INSTALL_PATH);
            
            if (newFileTime == destFileTime) {
                // Set permissions on the installed file
                std::string finalChmodCommand = "chmod 755 " + INSTALL_PATH.string();
                std::thread finalChmodThread = terminal.executeCommand(finalChmodCommand);
                finalChmodThread.join();
                updateSuccess = true;
            }
        }
    }
    
    // If direct copy failed, try with sudo
    if (!updateSuccess) {
        std::cout << "Administrator privileges required to install the update." << std::endl;
        std::cout << "Please enter your password if prompted." << std::endl;
        
        // Try with sudo
        std::string sudoCommand = "sudo cp " + outputPath + " " + INSTALL_PATH.string();
        std::thread sudoThread = terminal.executeCommand(sudoCommand);
        sudoThread.join();
        
        // Add a small delay to ensure file system updates are registered
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Verify sudo copy worked by checking timestamps
        if (std::filesystem::exists(INSTALL_PATH)) {
            // Compare file sizes as a fallback verification method since sudo might change timestamp
            auto newFileSize = std::filesystem::file_size(outputPath);
            auto destFileSize = std::filesystem::file_size(INSTALL_PATH);
            
            if (newFileSize == destFileSize) {
                // Set permissions with sudo
                std::string sudoChmodCommand = "sudo chmod 755 " + INSTALL_PATH.string();
                std::thread sudoChmodThread = terminal.executeCommand(sudoChmodCommand);
                sudoChmodThread.join();
                updateSuccess = true;
            } else {
                std::cout << "Error: The file was not properly installed (size mismatch)." << std::endl;
            }
        } else {
            std::cout << "Error: Installation failed - destination file doesn't exist." << std::endl;
        }
    }
    
    // If sudo also failed, offer alternative installation
    if (!updateSuccess) {
        std::cout << "Update installation failed. You can manually install the update by running:" << std::endl;
        std::cout << "sudo cp " << outputPath << " " << INSTALL_PATH.string() << std::endl;
        std::cout << "sudo chmod 755 " << INSTALL_PATH.string() << std::endl;
        std::cout << "Please ensure you have the necessary permissions." << std::endl;
    }
    
    // Clean up
    std::string cleanupCommand = "rm -rf " + tempDir.string();
    std::thread cleanupThread = terminal.executeCommand(cleanupCommand);
    cleanupThread.join();

    // Remove old update cache
    if (std::filesystem::exists(UPDATE_CACHE_FILE)) {
        std::filesystem::remove(UPDATE_CACHE_FILE);
        if (TESTING) {
            std::cout << "Removed old update cache file: " << UPDATE_CACHE_FILE << std::endl;
        }
    }

    return updateSuccess;
}

bool executeUpdateIfAvailable(bool updateAvailable) {
    if (!updateAvailable) return false;
    
    std::cout << "\nAn update is available. Would you like to download it? (Y/N): ";
    char response;
    std::cin >> response;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    
    if (response != 'Y' && response != 'y') return false;
    
    saveUpdateCache(false, cachedLatestVersion);
    
    if (!downloadLatestRelease()) {
        std::cout << "Failed to download or install the update. Please try again later or manually update." << std::endl;
        std::cout << "You can download the latest version from: " << githubRepoURL << "/releases/latest" << std::endl;
        saveUpdateCache(true, cachedLatestVersion);
        return false;
    }
    
    std::cout << "Update installed successfully! Would you like to restart now? (Y/N): ";
    std::cin >> response;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    
    if (response == 'Y' || response == 'y') {
        std::cout << "Restarting application..." << std::endl;
        
        // Save any necessary data before exit
        if (saveOnExit) {
            savedChatCache = c_assistant.getChatCache();
            writeUserData();
        }
        
        // Clean up resources
        if (isLoginShell) {
            cleanupLoginShell();
        }
        
        delete pluginManager;
        delete themeManager;
        
        // Execute the updated binary (restart the application)
        execl(INSTALL_PATH.c_str(), INSTALL_PATH.c_str(), NULL);
        
        // If execl fails
        std::cerr << "Failed to restart. Please restart the application manually." << std::endl;
        exit(0);
    } else {
        std::cout << "Please restart the application to use the new version." << std::endl;
    }
    
    return true;
}

void displayChangeLog(const std::string& changeLog) {
    std::cout << "Change Log: View full details at " << BLUE_COLOR_BOLD << githubRepoURL << RESET_COLOR << std::endl;
    std::cout << "Key changes in this version:" << std::endl;
    
    std::istringstream iss(changeLog);
    std::string line;
    int lineCount = 0;
    while (std::getline(iss, line) && lineCount < 5) {
        if (!line.empty()) {
            std::cout << "• " << line << std::endl;
            lineCount++;
        }
    }
    
    if (lineCount == 5 && std::getline(iss, line)) {
        std::cout << "... (see complete changelog on GitHub)" << std::endl;
    }
    
    if (lineCount == 0) {
        std::cout << "No key changes listed." << std::endl;
    }
}

void applyColorToStrings() {
    GREEN_COLOR_BOLD = themeManager->getColor("GREEN_COLOR_BOLD");
    RESET_COLOR = themeManager->getColor("RESET_COLOR");
    RED_COLOR_BOLD = themeManager->getColor("RED_COLOR_BOLD");
    PURPLE_COLOR_BOLD = themeManager->getColor("PURPLE_COLOR_BOLD");
    BLUE_COLOR_BOLD = themeManager->getColor("BLUE_COLOR_BOLD");
    YELLOW_COLOR_BOLD = themeManager->getColor("YELLOW_COLOR_BOLD");
    CYAN_COLOR_BOLD = themeManager->getColor("CYAN_COLOR_BOLD");
    terminal.setShellColor(themeManager->getColor("SHELL_COLOR"));
    terminal.setDirectoryColor(themeManager->getColor("DIRECTORY_COLOR"));
    terminal.setBranchColor(themeManager->getColor("BRANCH_COLOR"));
    terminal.setGitColor(themeManager->getColor("GIT_COLOR"));
    terminal.setPromptFormat(themeManager->getColor("PROMPT_FORMAT"));
}

void loadTheme(const std::string& themeName) {
    if (themeManager->loadTheme(themeName)) {
        currentTheme = themeName;
        applyColorToStrings();
    }
}

void themeCommands() {
    getNextCommand();
    if (lastCommandParsed == "load") {
        getNextCommand();
        if (!lastCommandParsed.empty()) {
            loadTheme(lastCommandParsed);
            writeUserData();
        } else {
            std::cerr << "No theme name provided to load." << std::endl;
        }
    } else if (!lastCommandParsed.empty()) {
        loadTheme(lastCommandParsed);
        writeUserData();
    } else {
        std::cout << "Current theme: " << currentTheme << std::endl;
        std::cout << "Available themes: " << std::endl;
        for (const auto& theme : themeManager->getAvailableThemes()) {
            std::cout << "  " << theme.first << std::endl;
        }
    }
}

bool startsWithCaseInsensitive(const std::string& str, const std::string& prefix) {
    if (str.size() < prefix.size()) {
        return false;
    }
    
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (tolower(str[i]) != tolower(prefix[i])) {
            return false;
        }
    }
    
    return true;
}

bool startsWith(const std::string& str, const std::string& prefix) {
    return startsWithCaseInsensitive(str, prefix);
}

void initializeDataDirectories() {
    if (!std::filesystem::exists(DATA_DIRECTORY)) {
        std::string applicationDirectory = std::filesystem::current_path().string();
        if (applicationDirectory.find(":") != std::string::npos) {
            applicationDirectory = applicationDirectory.substr(applicationDirectory.find(":") + 1);
        }
        std::filesystem::create_directory(applicationDirectory / DATA_DIRECTORY);
    }
    
    if (!std::filesystem::exists(THEMES_DIRECTORY)) {
        std::filesystem::create_directory(THEMES_DIRECTORY);
    }
    
    if (!std::filesystem::exists(PLUGINS_DIRECTORY)) {
        std::filesystem::create_directory(PLUGINS_DIRECTORY);
    }
}

void asyncCheckForUpdates(std::function<void(bool)> callback) {
    if (loadUpdateCache() && !shouldCheckForUpdates()) {
        if (!silentCheckForUpdates && cachedUpdateAvailable) {
            std::cout << "\nUpdate available: " << cachedLatestVersion << " (cached)" << std::endl;
        }
        callback(cachedUpdateAvailable);
        return;
    }
    
    bool updateAvailable = checkForUpdate();
    
    std::string latestVersion = "";
    try {
        std::string command = "curl -s " + updateURL_Github;
        std::string result;
        FILE *pipe = popen(command.c_str(), "r");
        if (pipe) {
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }
            pclose(pipe);
            
            json jsonData = json::parse(result);
            if (jsonData.contains("tag_name")) {
                latestVersion = jsonData["tag_name"].get<std::string>();
                if (!latestVersion.empty() && latestVersion[0] == 'v') {
                    latestVersion = latestVersion.substr(1);
                }
            }
        }
    } catch (std::exception &e) {
        std::cerr << "Error getting latest version: " << e.what() << std::endl;
    }
    
    saveUpdateCache(updateAvailable, latestVersion);
    
    callback(updateAvailable);
}

void loadPluginsAsync(std::function<void()> callback) {
    if (pluginManager) {
        pluginManager->discoverPlugins();
    }
    callback();
}

void loadThemeAsync(const std::string& themeName, std::function<void(bool)> callback) {
    bool success = false;
    if (themeManager) {
        success = themeManager->loadTheme(themeName);
    }
    callback(success);
}

void processChangelogAsync() {
    std::ifstream changelogFile(DATA_DIRECTORY / "CHANGELOG.txt");
    if (!changelogFile.is_open()) return;
    
    std::time_t now = std::time(nullptr);
    std::tm* now_tm = std::localtime(&now);
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now_tm);
    
    std::string changeLog((std::istreambuf_iterator<char>(changelogFile)), std::istreambuf_iterator<char>());
    changelogFile.close();
    
    std::ofstream savedChangelogFile(DATA_DIRECTORY / "latest_changelog.txt");
    if (savedChangelogFile.is_open()) {
        savedChangelogFile << changeLog;
        savedChangelogFile.close();
    }
    
    lastUpdated = buffer;
    
    std::filesystem::remove(DATA_DIRECTORY / "CHANGELOG.txt");
    
    std::ofstream flagFile(DATA_DIRECTORY / ".new_changelog");
    if (flagFile.is_open()) {
        flagFile << "1";
        flagFile.close();
    }
}

void loadUserDataAsync(std::function<void()> callback) {
    std::ifstream file(USER_DATA);
    if (file.is_open()) {
        if (file.peek() == std::ifstream::traits_type::eof()) {
            file.close();
            createNewUSER_DATAFile();
        } else {
            try {
                json userData;
                file >> userData;
                if(userData.contains("OpenAI_API_KEY")) {
                    c_assistant.setAPIKey(userData["OpenAI_API_KEY"].get<std::string>());
                }
                if(userData.contains("Chat_Cache")) {
                    savedChatCache = userData["Chat_Cache"].get<std::vector<std::string> >();
                    c_assistant.setChatCache(savedChatCache);
                }
                if(userData.contains("Startup_Commands")) {
                    startupCommands = userData["Startup_Commands"].get<std::vector<std::string> >();
                }
                if(userData.contains("Shortcuts_Enabled")) {
                    shortcutsEnabled = userData["Shortcuts_Enabled"].get<bool>();
                }
                if(userData.contains("Shortcuts_Prefix")) {
                    shortcutsPrefix = userData["Shortcuts_Prefix"].get<std::string>();
                }
                if(userData.contains("Multi_Script_Shortcuts")) {
                    multiScriptShortcuts = userData["Multi_Script_Shortcuts"].get<std::map<std::string, std::vector<std::string>>>();
                }
                if(userData.contains("Last_Updated")) {
                    lastUpdated = userData["Last_Updated"].get<std::string>();
                }
                if(userData.contains("Current_Theme")) {
                    currentTheme = userData["Current_Theme"].get<std::string>();
                }
                if(userData.contains("Auto_Update_Check")) {
                    checkForUpdates = userData["Auto_Update_Check"].get<bool>();
                }
                if(userData.contains("Update_From_Github")) {
                    updateFromGithub = userData["Update_From_Github"].get<bool>();
                }
                if(userData.contains("Silent_Update_Check")) {
                    silentCheckForUpdates = userData["Silent_Update_Check"].get<bool>();
                }
                if(userData.contains("Startup_Commands_Enabled")) {
                    startCommandsOn = userData["Startup_Commands_Enabled"].get<bool>();
                }
                if(userData.contains("Last_Update_Check_Time")) {
                    lastUpdateCheckTime = userData["Last_Update_Check_Time"].get<time_t>();
                }
                if(userData.contains("Update_Check_Interval")) {
                    UPDATE_CHECK_INTERVAL = userData["Update_Check_Interval"].get<int>();
                }
                file.close();
            }
            catch(const json::parse_error& e) {
                file.close();
                createNewUSER_DATAFile();
            }
        }
    }
    callback();
}

void saveUpdateCache(bool updateAvailable, const std::string& latestVersion) {
    json cacheData;
    cacheData["update_available"] = updateAvailable;
    cacheData["latest_version"] = latestVersion;
    cacheData["check_time"] = std::time(nullptr);
    
    std::ofstream cacheFile(UPDATE_CACHE_FILE);
    if (cacheFile.is_open()) {
        cacheFile << cacheData.dump();
        cacheFile.close();
        
        cachedUpdateAvailable = updateAvailable;
        cachedLatestVersion = latestVersion;
        lastUpdateCheckTime = std::time(nullptr);
        
        writeUserData();
    } else {
        std::cerr << "Warning: Could not open update cache file for writing." << std::endl;
    }
}

bool loadUpdateCache() {
    if (!std::filesystem::exists(UPDATE_CACHE_FILE)) {
        return false;
    }
    
    std::ifstream cacheFile(UPDATE_CACHE_FILE);
    if (!cacheFile.is_open()) return false;
    
    try {
        json cacheData;
        cacheFile >> cacheData;
        cacheFile.close();
        
        if (cacheData.contains("update_available") && 
            cacheData.contains("latest_version") && 
            cacheData.contains("check_time")) {
            
            cachedUpdateAvailable = cacheData["update_available"].get<bool>();
            cachedLatestVersion = cacheData["latest_version"].get<std::string>();
            lastUpdateCheckTime = cacheData["check_time"].get<time_t>();
            
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error loading update cache: " << e.what() << std::endl;
        return false;
    }
}

bool shouldCheckForUpdates() {
    if (lastUpdateCheckTime == 0) {
        return true;
    }
    
    time_t currentTime = std::time(nullptr);
    time_t elapsedTime = currentTime - lastUpdateCheckTime;
    
    if (TESTING) {
        std::cout << "Time since last update check: " << elapsedTime 
                  << " seconds (interval: " << UPDATE_CHECK_INTERVAL << " seconds)" << std::endl;
    }
    
    return elapsedTime > UPDATE_CHECK_INTERVAL;
}

void setupEnvironmentVariables() {
    uid_t uid = getuid();
    struct passwd* pw = getpwuid(uid);
    
    if (pw != nullptr) {
        setenv("USER", pw->pw_name, 1);
        setenv("LOGNAME", pw->pw_name, 1);
        setenv("HOME", pw->pw_dir, 1);
        setenv("SHELL", ACTUAL_SHELL_PATH.string().c_str(), 1);
        
        if (getenv("PATH") == nullptr) {
            setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
        }
        
        setenv("CJSH_INSTALL_PATH", INSTALL_PATH.string().c_str(), 1);
        setenv("CJSH_DATA_DIR", DATA_DIRECTORY.string().c_str(), 1);
        setenv("CJSH_VERSION", currentVersion.c_str(), 1);
        
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            setenv("HOSTNAME", hostname, 1);
        }
        
        setenv("TERM", "xterm-256color", 0);
        
        setenv("PWD", std::filesystem::current_path().string().c_str(), 1);
        
        if (getenv("TZ") == nullptr) {
            std::string tzFile = "/etc/localtime";
            if (std::filesystem::exists(tzFile)) {
                setenv("TZ", tzFile.c_str(), 1);
            }
        }
        
        if (TESTING) {
            std::cout << "Environment setup: USER=" << getenv("USER") 
                      << " HOME=" << getenv("HOME")
                      << " SHELL=" << getenv("SHELL") << std::endl;
        }
    }
}

void initializeLoginEnvironment() {
    if (isLoginShell) {
        pid_t pid = getpid();
        
        if (setsid() < 0) {
            if (errno != EPERM) {
                perror("Failed to become session leader");
            }
        }
        
        shell_terminal = STDIN_FILENO;
        
        if (!isatty(shell_terminal)) {
            std::cerr << "Warning: Not running on a terminal device" << std::endl;
        }
    }
}

void handleSIGHUP(int sig) {
    std::cerr << "Received SIGHUP, terminal disconnected" << std::endl;
    exitFlag = true;
    
    if (pluginManager != nullptr) {
        delete pluginManager;
        pluginManager = nullptr;
    }
    
    if (themeManager != nullptr) {
        delete themeManager;
        themeManager = nullptr;
    }
    
    terminal.terminateAllChildProcesses();
    
    if (jobControlEnabled) {
        try {
            tcsetattr(shell_terminal, TCSANOW, &shell_tmodes);
        } catch (...) {
        }
    }
    
    _exit(0);
}

void handleSIGTERM(int sig) {
    std::cerr << "Received SIGTERM, exiting" << std::endl;
    exitFlag = true;
    
    terminal.terminateAllChildProcesses();
    
    _exit(0);
}

void handleSIGINT(int sig) {
    std::cerr << "Received SIGINT, interrupting current operation" << std::endl;
    signal(SIGINT, handleSIGINT);
}

void handleSIGCHLD(int sig) {
    signal(SIGCHLD, handleSIGCHLD);
}

void setupSignalHandlers() {
    signal(SIGHUP, handleSIGHUP);
    signal(SIGTERM, handleSIGTERM);
    signal(SIGINT, handleSIGINT);
    signal(SIGCHLD, handleSIGCHLD);
    
    struct sigaction sa;
    sa.sa_handler = handleSIGHUP;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGHUP, &sa, NULL);
    
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

void setupJobControl() {
    if (!isatty(STDIN_FILENO)) {
        jobControlEnabled = false;
        return;
    }
    
    shell_pgid = getpid();
    
    if (setpgid(shell_pgid, shell_pgid) < 0) {
        if (errno != EPERM) {
            perror("Couldn't put the shell in its own process group");
        }
    }
    
    try {
        int tpgrp = tcgetpgrp(shell_terminal);
        if (tpgrp != -1) {
            if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
                perror("Couldn't grab terminal control");
            }
        }
        
        if (tcgetattr(shell_terminal, &shell_tmodes) < 0) {
            perror("Couldn't get terminal attributes");
        }
        
        jobControlEnabled = true;
    } catch (const std::exception& e) {
        std::cerr << "Error setting up terminal: " << e.what() << std::endl;
        jobControlEnabled = false;
    }
}

void resetTerminalOnExit() {
    if (jobControlEnabled) {
        try {
            if (tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes) < 0) {
                perror("Could not restore terminal settings");
            }
        } catch (const std::exception& e) {
            std::cerr << "Error restoring terminal: " << e.what() << std::endl;
        }
    }
    
    terminal.setTerminationFlag(true);
    
    // Be more aggressive in terminating all child processes
    terminal.terminateAllChildProcesses();
    
    // Secondary failsafe: Kill any remaining processes in our process group
    pid_t pgid = getpgid(0);
    if (pgid > 0 && pgid != 1) {
        killpg(pgid, SIGTERM);
        usleep(10000); // Brief 10ms grace period
        killpg(pgid, SIGKILL); // Force kill any remaining processes
    }
    
    // Final sweep of remaining jobs as last resort
    for (const auto& job : terminal.getActiveJobs()) {
        kill(-job.pid, SIGKILL);
        kill(job.pid, SIGKILL);
    }
}

bool isParentProcessAlive() {
    pid_t ppid = getppid();
    return ppid != 1;
}

void parentProcessWatchdog() {
    while (!exitFlag) {
        if (!isParentProcessAlive()) {
            std::cerr << "Parent process terminated, shutting down..." << std::endl;
            exitFlag = true;
            
            if (pluginManager != nullptr) {
                delete pluginManager;
                pluginManager = nullptr;
            }
            
            if (themeManager != nullptr) {
                delete themeManager;
                themeManager = nullptr;
            }
            
            terminal.terminateAllChildProcesses();
            exit(0);
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

void createDefaultCJSHRC() {
    if (std::filesystem::exists(CJSHRC_FILE)) {
        return;
    }

    std::ofstream file(CJSHRC_FILE);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to create " << CJSHRC_FILE << std::endl;
        return;
    }

    file << "# CJ's Shell RC File\n";
    file << "# This file is sourced when CJ's Shell starts as a login shell\n\n";
    
    file << "# Default aliases\n";
    file << "alias ll='ls -la'\n";
    file << "alias la='ls -a'\n";
    file << "alias l='ls'\n";
    file << "alias c='clear'\n\n";
    
    file << "# Environment variables\n";
    file << "export CJSH_INITIALIZED=true\n\n";
    
    file.close();
    std::cout << "Created default .cjshrc file at " << CJSHRC_FILE << std::endl;
}

void processProfileFile(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        return;
    }
    
    if (std::filesystem::is_directory(filePath)) {
        for (const auto& entry : std::filesystem::directory_iterator(filePath)) {
            if (entry.path().extension() == ".sh") {
                processProfileFile(entry.path().string());
            }
        }
        return;
    }
    
    std::ifstream file(filePath);
    if (!file) {
        return;
    }
    
    std::string line;
    std::vector<std::string> conditionalStack;
    std::vector<bool> conditionMetStack;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        bool skipDueToConditional = false;
        for (size_t i = 0; i < conditionalStack.size(); i++) {
            if (!conditionMetStack[i]) {
                skipDueToConditional = true;
                break;
            }
        }

        if (line.find("if ") == 0 || line.find("if(") == 0) {
            conditionalStack.push_back("if");
            bool conditionMet = false;

            if (line.find("[ -d ") != std::string::npos || line.find(" -d ") != std::string::npos) {
                size_t startPos = line.find("-d ") + 3;
                size_t endPos = line.find("]", startPos);
                if (endPos != std::string::npos) {
                    std::string path = line.substr(startPos, endPos - startPos);
                    path.erase(0, path.find_first_not_of(" \t\"'"));
                    path.erase(path.find_last_not_of(" \t\"'") + 1);
                    
                    path = expandEnvVariables(path);
                    
                    conditionMet = std::filesystem::exists(path) && std::filesystem::is_directory(path);
                }
            } 
            else if (line.find("[ -f ") != std::string::npos || line.find(" -f ") != std::string::npos) {
                size_t startPos = line.find("-f ") + 3;
                size_t endPos = line.find("]", startPos);
                if (endPos != std::string::npos) {
                    std::string path = line.substr(startPos, endPos - startPos);
                    path.erase(0, path.find_first_not_of(" \t\"'"));
                    path.erase(path.find_last_not_of(" \t\"'") + 1);
                    
                    path = expandEnvVariables(path);
                    
                    conditionMet = std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
                }
            } 
            else if (line.find("command -v") != std::string::npos || line.find("which") != std::string::npos) {
                size_t cmdStartPos = 0;
                if (line.find("command -v") != std::string::npos) {
                    cmdStartPos = line.find("command -v") + 10;
                } else {
                    cmdStartPos = line.find("which") + 5;
                }
                
                size_t cmdEndPos = line.find(">", cmdStartPos);
                if (cmdEndPos == std::string::npos) {
                    cmdEndPos = line.find(" ", cmdStartPos);
                }
                if (cmdEndPos == std::string::npos) {
                    cmdEndPos = line.length();
                }
                
                std::string cmd = line.substr(cmdStartPos, cmdEndPos - cmdStartPos);
                cmd.erase(0, cmd.find_first_not_of(" \t"));
                cmd.erase(cmd.find_last_not_of(" \t") + 1);
                
                std::string pathEnv = getenv("PATH") ? getenv("PATH") : "";
                std::istringstream pathStream(pathEnv);
                std::string dir;
                conditionMet = false;
                
                while (std::getline(pathStream, dir, ':')) {
                    if (dir.empty()) continue;
                    std::string fullPath = dir + "/" + cmd;
                    if (access(fullPath.c_str(), X_OK) == 0) {
                        conditionMet = true;
                        break;
                    }
                }
            }
            
            conditionMetStack.push_back(conditionMet);
            continue;
        }
        
        if (line == "else" && !conditionalStack.empty()) {
            if (conditionalStack.back() == "if") {
                conditionMetStack.back() = !conditionMetStack.back();
            }
            continue;
        }
        
        if ((line == "fi" || line == "endif") && !conditionalStack.empty()) {
            conditionalStack.pop_back();
            conditionMetStack.pop_back();
            continue;
        }
        
        if (skipDueToConditional) {
            continue;
        }
        
        if (line.find("export ") == 0) {
            std::string assignments = line.substr(7);
            std::istringstream iss(assignments);
            std::string assignment;
            
            while (iss >> assignment) {
                size_t eqPos = assignment.find('=');
                if (eqPos != std::string::npos) {
                    std::string name = assignment.substr(0, eqPos);
                    std::string value = assignment.substr(eqPos + 1);
                    
                    if (value.size() >= 2 && 
                        ((value.front() == '"' && value.back() == '"') || 
                         (value.front() == '\'' && value.back() == '\''))) {
                        value = value.substr(1, value.size() - 2);
                    }
                    
                    value = expandEnvVariables(value);
                    setenv(name.c_str(), value.c_str(), 1);
                }
            }
        } 
        else if (line.find('=') != std::string::npos && line.find("let ") != 0 && 
                 line.find(" = ") == std::string::npos) {
            size_t eqPos = line.find('=');
            std::string name = line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);
            
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            if (value.size() >= 2 && 
                ((value.front() == '"' && value.back() == '"') || 
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.size() - 2);
            }
            
            value = expandEnvVariables(value);
            
            if (!name.empty() && isalpha(name[0])) {
                setenv(name.c_str(), value.c_str(), 1);
            }
        } 
        else if (line.find("PATH=") == 0 || line.find("PATH=$PATH:") == 0) {
            std::string pathValue = line.substr(line.find('=') + 1);
            pathValue = expandEnvVariables(pathValue);
            setenv("PATH", pathValue.c_str(), 1);
        } 
        else if (line.find("alias ") == 0) {
            line = line.substr(6);
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string aliasName = line.substr(0, eqPos);
                std::string aliasValue = line.substr(eqPos + 1);
                
                aliasName.erase(0, aliasName.find_first_not_of(" \t"));
                aliasName.erase(aliasName.find_last_not_of(" \t") + 1);
                aliasValue.erase(0, aliasValue.find_first_not_of(" \t"));
                aliasValue.erase(aliasValue.find_last_not_of(" \t") + 1);
                
                if (aliasValue.size() >= 2 && 
                    ((aliasValue.front() == '"' && aliasValue.back() == '"') || 
                     (aliasValue.front() == '\'' && aliasValue.back() == '\''))) {
                    aliasValue = aliasValue.substr(1, aliasValue.size() - 2);
                }
                
                aliases[aliasName] = aliasValue;
            }
        } 
        else if (line.find("source ") == 0 || line.find(". ") == 0) {
            size_t startPos = line.find(' ') + 1;
            std::string sourcePath = line.substr(startPos);
            sourcePath.erase(0, sourcePath.find_first_not_of(" \t"));
            sourcePath.erase(sourcePath.find_last_not_of(" \t") + 1);
            
            sourcePath = expandEnvVariables(sourcePath);
            
            if (sourcePath[0] != '/' && sourcePath[0] != '~') {
                std::filesystem::path baseDir = std::filesystem::path(filePath).parent_path();
                sourcePath = (baseDir / sourcePath).string();
            } else if (sourcePath[0] == '~') {
                std::string homeDir = getenv("HOME") ? getenv("HOME") : "";
                sourcePath.replace(0, 1, homeDir);
            }
            
            if (std::filesystem::exists(sourcePath)) {
                processProfileFile(sourcePath);
            }
        }
    }
    
    loadAliasesFromFile(filePath);
}