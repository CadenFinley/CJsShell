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

#include "include/terminalpassthrough.h"
#include "include/openaipromptengine.h"
#include "include/pluginmanager.h"
#include "include/thememanager.h"

//start work on renaming project to cjsh "caden joshua's shell" "cj's shell"

using json = nlohmann::json;

const std::string processId = std::to_string(getpid());
const std::string currentVersion = "2.0.0.0";
const std::string githubRepoURL = "https://github.com/CadenFinley/CJsShell";
const std::string updateURL_Github = "https://api.github.com/repos/cadenfinley/CJsShell/releases/latest";

bool TESTING = false;
bool runningStartup = false;
bool exitFlag = false;
bool defaultTextEntryOnAI = false;
bool displayWholePath = false;
bool rawModeEnabled = false;
bool saveLoop = false;
bool saveOnExit = false;
bool updateFromGithub = false;
bool executablesCacheInitialized = false;
bool completionBrowsingMode = false;
bool cachedUpdateAvailable = false;
bool isLoginShell = false;
bool isFileHandler = false;

bool shortcutsEnabled = true;
bool aliasesEnabled = true;
bool startCommandsOn = true;
bool usingChatCache = true;
bool checkForUpdates = true;
bool silentCheckForUpdates = true;

bool hasSuggestion = false;
std::string currentSuggestion = "";

time_t lastUpdateCheckTime = 0;
int UPDATE_CHECK_INTERVAL = 86400;
std::string cachedLatestVersion = "";

std::string currentTheme = "default";
std::string GREEN_COLOR_BOLD = "\033[1;32m";
std::string RESET_COLOR = "\033[0m";
std::string RED_COLOR_BOLD = "\033[1;31m";
std::string PURPLE_COLOR_BOLD = "\033[1;35m";
std::string BLUE_COLOR_BOLD = "\033[1;34m";
std::string YELLOW_COLOR_BOLD = "\033[1;33m";
std::string CYAN_COLOR_BOLD = "\033[1;36m";

std::string commandPrefix = "!";
std::string shortcutsPrefix = "-";
std::string lastCommandParsed;
std::string titleLine = "CJ's Shell v" + currentVersion + " - Caden J Finley (c) 2025";
std::string createdLine = "Created 2025 @ " + PURPLE_COLOR_BOLD + "Abilene Christian University" + RESET_COLOR;
std::string lastUpdated = "N/A";

std::string homeDir = std::getenv("HOME");
std::filesystem::path DATA_DIRECTORY = std::filesystem::path(homeDir) / ".cjsh_data";
std::filesystem::path UNINSTALL_SCRIPT_PATH = DATA_DIRECTORY / "cjsh_uninstall.sh";
std::filesystem::path UPDATE_SCRIPT_PATH = DATA_DIRECTORY / "cjsh_update.sh";
std::filesystem::path USER_DATA = DATA_DIRECTORY / "USER_DATA.json";
std::filesystem::path USER_COMMAND_HISTORY = DATA_DIRECTORY / "USER_COMMAND_HISTORY.txt";
std::filesystem::path THEMES_DIRECTORY = DATA_DIRECTORY / "themes";
std::filesystem::path PLUGINS_DIRECTORY = DATA_DIRECTORY / "plugins";
std::filesystem::path UPDATE_CACHE_FILE = DATA_DIRECTORY / "update_cache.json";

std::queue<std::string> commandsQueue;
std::vector<std::string> startupCommands;
std::vector<std::string> savedChatCache;
std::vector<std::string> commandLines;
std::vector<std::string> executablesCache;
std::map<std::string, std::vector<std::string>> multiScriptShortcuts;
std::map<std::string, std::string> aliases;
std::map<std::string, std::map<std::string, std::string>> availableThemes;

size_t tabCompletionIndex = 0;
std::vector<std::string> currentCompletions;
std::string originalInput;

OpenAIPromptEngine c_assistant;
TerminalPassthrough terminal;
PluginManager* pluginManager = nullptr;
ThemeManager* themeManager = nullptr;

std::mutex rawModeMutex;

pid_t shell_pgid = 0;
struct termios shell_tmodes;
int shell_terminal;
bool jobControlEnabled = false;

std::vector<std::string> getTabCompletions(const std::string& input);
std::string completeFilePath(const std::string& input);
std::string getCommonPrefix(const std::vector<std::string>& strings);
void displayCompletionOptions(const std::vector<std::string>& completions);
void applyCompletion(size_t index, std::string& command, size_t& cursorPositionX, size_t& cursorPositionY);
void showInlineSuggestion(const std::string& input);
std::string readAndReturnUserDataFile();
std::vector<std::string> commandSplicer(const std::string& command);
void mainProcessLoop();
void setRawMode(bool enable);
void enableRawMode();
void disableRawMode();
bool isRawModeEnabled();
void createNewUSER_DATAFile();
void createNewUSER_HISTORYfile();
void writeUserData();
void goToApplicationDirectory();
void commandParser(const std::string& command);
void addUserInputToHistory(const std::string& input);
void shortcutProcesser(const std::string& command);
void commandProcesser(const std::string& command);
void sendTerminalCommand(const std::string& command);
void userSettingsCommands();
void startupCommandsHandler();
void shortcutCommands();
void textCommands();
void getNextCommand();
void aiSettingsCommands();
void aiChatCommands();
void chatProcess(const std::string& message);
void showChatHistory();
void userDataCommands();
std::string handleArrowKey(char arrow, size_t& cursorPositionX, size_t& cursorPositionY, std::vector<std::string>& commandLines, std::string& command, const std::string& terminalTag);
void placeCursor(size_t& cursorPositionX, size_t& cursorPositionY);
void reprintCommandLines(const std::vector<std::string>& commandLines, const std::string& terminalSetting);
void clearLines(const std::vector<std::string>& commandLines);
void displayChangeLog(const std::string& changeLog);
bool checkForUpdate();
bool downloadLatestRelease();
void pluginCommands();
std::string getClipboardContent();
void themeCommands();
void loadTheme(const std::string& themeName);
void saveTheme(const std::string& themeName);
void createDefaultTheme();
void discoverAvailableThemes();
void applyColorToStrings();
void multiScriptShortcutProcesser(const std::string& command);
void aliasCommands();
bool checkFromUpdate_Github(std::function<bool(const std::string&, const std::string&)> isNewerVersion);
void refreshExecutablesCache();
void initializeDataDirectories();
void asyncCheckForUpdates(std::function<void(bool)> callback);
void loadPluginsAsync(std::function<void()> callback);
void loadThemeAsync(const std::string& themeName, std::function<void(bool)> callback);
void processChangelogAsync();
void loadUserDataAsync(std::function<void()> callback);
bool hasPathChanged();
void loadExecutableCacheFromDisk();
void saveExecutableCacheToDisk();
bool executeUpdateIfAvailable(bool updateAvailable);
bool startsWith(const std::string& str, const std::string& prefix);
void updateCommands();
void manualUpdateCheck();
void setUpdateInterval(int intervalHours);
bool shouldCheckForUpdates();
bool loadUpdateCache();
void saveUpdateCache(bool updateAvailable, const std::string &latestVersion);
void setupEnvironmentVariables();
void initializeLoginEnvironment();
void setupSignalHandlers();
bool authenticateUser();
void handleLoginSession();
void setupJobControl();
void resetTerminalOnExit();
void processProfileFile(const std::string& filePath);
void handleSIGHUP(int sig);
void handleSIGTERM(int sig);
void handleSIGINT(int sig);
void handleSIGCHLD(int sig);
void parentProcessWatchdog();
void printHelp();

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
        std::vector<std::string> profileFiles = {
            homeDir + "/.profile",
            homeDir + "/.bash_profile",
            homeDir + "/.zshrc",
            homeDir + "/.bashrc"
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
}

void cleanupLoginShell() {
    try {
        resetTerminalOnExit();
    } catch (const std::exception& e) {
        std::cerr << "Error cleaning up terminal: " << e.what() << std::endl;
    }
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
    std::string currentConditionalBlock = "";
    bool inConditionalBlock = false;
    bool conditionMet = false;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.find("if ") == 0 || line.find("if(") == 0) {
            inConditionalBlock = true;
            currentConditionalBlock = "if";
            if (line.find("[ -d ") != std::string::npos || line.find(" -d ") != std::string::npos) {
                size_t startPos = line.find("-d ") + 3;
                size_t endPos = line.find("]", startPos);
                if (endPos != std::string::npos) {
                    std::string path = line.substr(startPos, endPos - startPos);
                    path.erase(0, path.find_first_not_of(" \t\"'"));
                    path.erase(path.find_last_not_of(" \t\"'") + 1);
                    
                    if (path.find("$HOME") != std::string::npos) {
                        std::string homeDir = getenv("HOME") ? getenv("HOME") : "";
                        size_t pos = path.find("$HOME");
                        path.replace(pos, 5, homeDir);
                    }
                    
                    conditionMet = std::filesystem::exists(path) && std::filesystem::is_directory(path);
                }
            } else if (line.find("[ -f ") != std::string::npos || line.find(" -f ") != std::string::npos) {
                size_t startPos = line.find("-f ") + 3;
                size_t endPos = line.find("]", startPos);
                if (endPos != std::string::npos) {
                    std::string path = line.substr(startPos, endPos - startPos);
                    path.erase(0, path.find_first_not_of(" \t\"'"));
                    path.erase(path.find_last_not_of(" \t\"'") + 1);
                    
                    if (path.find("$HOME") != std::string::npos) {
                        std::string homeDir = getenv("HOME") ? getenv("HOME") : "";
                        size_t pos = path.find("$HOME");
                        path.replace(pos, 5, homeDir);
                    }
                    
                    conditionMet = std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
                }
            } else if (line.find("command -v") != std::string::npos) {
                size_t startPos = line.find("command -v") + 10;
                size_t endPos = line.find(">", startPos);
                if (endPos == std::string::npos) endPos = line.length();
                
                std::string cmd = line.substr(startPos, endPos - startPos);
                cmd.erase(0, cmd.find_first_not_of(" \t"));
                cmd.erase(cmd.find_last_not_of(" \t") + 1);
                
                char* pathEnv = getenv("PATH");
                if (pathEnv) {
                    std::string path(pathEnv);
                    std::stringstream ss(path);
                    std::string dir;
                    conditionMet = false;
                    
                    while (std::getline(ss, dir, ':')) {
                        std::string fullPath = dir + "/" + cmd;
                        if (access(fullPath.c_str(), X_OK) == 0) {
                            conditionMet = true;
                            break;
                        }
                    }
                }
            }
            continue;
        }
        
        if (line == "else" && inConditionalBlock) {
            conditionMet = !conditionMet;
            continue;
        }
        
        if (line == "fi" && inConditionalBlock) {
            inConditionalBlock = false;
            conditionMet = false;
            continue;
        }
        
        if (inConditionalBlock && !conditionMet) {
            continue;
        }
        
        if (line.find("export ") == 0) {
            line = line.substr(7);
        
            std::istringstream iss(line);
            std::string varAssignment;
            while (iss >> varAssignment) {
                size_t pos = varAssignment.find('=');
                if (pos != std::string::npos) {
                    std::string name = varAssignment.substr(0, pos);
                    std::string value = varAssignment.substr(pos + 1);
                    
                    if (value.size() >= 2 && 
                        ((value.front() == '"' && value.back() == '"') || 
                         (value.front() == '\'' && value.back() == '\''))) {
                        value = value.substr(1, value.size() - 2);
                    }
                    
                    size_t varPos = value.find('$');
                    while (varPos != std::string::npos) {
                        size_t endPos = value.find_first_of(" \t/\"'", varPos + 1);
                        if (endPos == std::string::npos) endPos = value.size();
                        
                        std::string varName = value.substr(varPos + 1, endPos - varPos - 1);
                        if (varName.front() == '{' && varName.back() == '}') {
                            varName = varName.substr(1, varName.size() - 2);
                        }
                        
                        const char* envValue = getenv(varName.c_str());
                        value.replace(varPos, endPos - varPos, envValue ? envValue : "");
                        
                        varPos = value.find('$', varPos + 1);
                    }
                    
                    setenv(name.c_str(), value.c_str(), 1);
                }
            }
        } else if (line.find('=') != std::string::npos && line.find("let ") != 0) {
            size_t pos = line.find('=');
            std::string name = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            if (value.size() >= 2 && 
                ((value.front() == '"' && value.back() == '"') || 
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.size() - 2);
            }
            
            size_t varPos = value.find('$');
            while (varPos != std::string::npos) {
                size_t endPos = value.find_first_of(" \t/\"'", varPos + 1);
                if (endPos == std::string::npos) endPos = value.size();
                
                std::string varName = value.substr(varPos + 1, endPos - varPos - 1);
                if (varName.front() == '{' && varName.back() == '}') {
                    varName = varName.substr(1, varName.size() - 2);
                }
                
                const char* envValue = getenv(varName.c_str());
                value.replace(varPos, endPos - varPos, envValue ? envValue : "");
                
                varPos = value.find('$', varPos + 1);
            }
            
            if (!name.empty() && isalpha(name[0])) {
                setenv(name.c_str(), value.c_str(), 1);
            }
        } else if (line.find("PATH=") == 0 || line.find("PATH=$PATH:") == 0) {
            std::string pathValue = line.substr(line.find('=') + 1);
            
            std::string currentPath = getenv("PATH") ? getenv("PATH") : "";
            size_t pathVarPos = pathValue.find("$PATH");
            while (pathVarPos != std::string::npos) {
                pathValue.replace(pathVarPos, 5, currentPath);
                pathVarPos = pathValue.find("$PATH", pathVarPos + currentPath.length());
            }
            
            std::string homeDir = getenv("HOME") ? getenv("HOME") : "";
            size_t homeVarPos = pathValue.find("$HOME");
            while (homeVarPos != std::string::npos) {
                pathValue.replace(homeVarPos, 5, homeDir);
                homeVarPos = pathValue.find("$HOME", homeVarPos + homeDir.length());
            }
            
            size_t varPos = pathValue.find('$');
            while (varPos != std::string::npos) {
                size_t endPos = pathValue.find_first_of(" \t:/\"'", varPos + 1);
                if (endPos == std::string::npos) endPos = pathValue.size();
                
                std::string varName = pathValue.substr(varPos + 1, endPos - varPos - 1);
                if (varName.front() == '{' && varName.back() == '}') {
                    varName = varName.substr(1, varName.size() - 2);
                }
                
                const char* envValue = getenv(varName.c_str());
                pathValue.replace(varPos, endPos - varPos, envValue ? envValue : "");
                
                varPos = pathValue.find('$', varPos + 1);
            }
            
            setenv("PATH", pathValue.c_str(), 1);
        } else if (line.find("alias ") == 0) {
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
                
                if (aliasesEnabled) {
                    aliases[aliasName] = aliasValue;
                }
            }
        } else if (line.find("source ") == 0 || line.find(". ") == 0) {
            size_t startPos = line.find(' ') + 1;
            std::string sourcePath = line.substr(startPos);
            sourcePath.erase(0, sourcePath.find_first_not_of(" \t"));
            sourcePath.erase(sourcePath.find_last_not_of(" \t") + 1);
            
            if (sourcePath.find('$') != std::string::npos) {
                size_t varPos = sourcePath.find('$');
                while (varPos != std::string::npos) {
                    size_t endPos = sourcePath.find_first_of(" \t/\"'", varPos + 1);
                    if (endPos == std::string::npos) endPos = sourcePath.size();
                    
                    std::string varName = sourcePath.substr(varPos + 1, endPos - varPos - 1);
                    if (varName.front() == '{' && varName.back() == '}') {
                        varName = varName.substr(1, varName.size() - 2);
                    }
                    
                    const char* envValue = getenv(varName.c_str());
                    sourcePath.replace(varPos, endPos - varPos, envValue ? envValue : "");
                    
                    varPos = sourcePath.find('$', varPos + 1);
                }
            }
            
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
}

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
        }
    }
    
    startupCommands = {};
    multiScriptShortcuts = {};
    aliases = {};
    c_assistant = OpenAIPromptEngine("", "chat", "You are an AI personal assistant within a shell.", {}, DATA_DIRECTORY);

    initializeDataDirectories();
    
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
    
    std::future<void> execCacheFuture = std::async(std::launch::async, [&]() {
        if (std::filesystem::exists(DATA_DIRECTORY / "executables_cache.json")) {
            loadExecutableCacheFromDisk();
            executablesCacheInitialized = true;
        }
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
    execCacheFuture.wait();
    themeFuture.wait();
    if (updateFuture.valid()) updateFuture.wait();

    std::thread watchdogThread(parentProcessWatchdog);
    watchdogThread.detach();

    if (!startupCommands.empty() && startCommandsOn) {
        runningStartup = true;
        for (const auto& command : startupCommands) {
            commandParser(commandPrefix + command);
        }
        runningStartup = false;
    }

    if(!exitFlag){
        std::cout << titleLine << std::endl;
        std::cout << createdLine << std::endl;
    
        mainProcessLoop();
    }

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

int getTerminalWidth(){
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
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
    std::string terminalSetting;
    int terminalSettingLength;
    notifyPluginsTriggerMainProcess("pre_run", processId);
    
    while (true) {
        notifyPluginsTriggerMainProcess("start", processId);
        if (saveLoop) {
            writeUserData();
        }
        
        if (TESTING) {
            std::cout << RED_COLOR_BOLD << "DEV MODE ENABLED" << RESET_COLOR << std::endl;
        }
        if (defaultTextEntryOnAI) {
            std::string modelInfo = c_assistant.getModel();
            std::string modeInfo = c_assistant.getAssistantType();
            terminalSetting = GREEN_COLOR_BOLD + "[" + YELLOW_COLOR_BOLD + modelInfo + GREEN_COLOR_BOLD + " | " + BLUE_COLOR_BOLD + modeInfo + GREEN_COLOR_BOLD + "] > " + RESET_COLOR;
            terminalSettingLength = 10 + modelInfo.length() + modeInfo.length();
        } else {
            terminalSetting = terminal.returnCurrentTerminalPosition();
            terminalSettingLength = terminal.getTerminalCurrentPositionRawLength();
        }
        std::cout << terminalSetting;
        char c;
        size_t cursorPositionX = 0;
        size_t cursorPositionY = 0;
        commandLines.clear();
        commandLines.push_back("");
        tabCompletionIndex = 0;
        currentCompletions.clear();
        originalInput = "";
        completionBrowsingMode = false;
        enableRawMode();
        while (true) {
            if (!std::cin.rdbuf()->in_avail()) {
                usleep(10000);
            }
            std::cin.get(c);
            notifyPluginsTriggerMainProcess("took_input" , std::string(1, c));
            if (c == 22) {
                std::string clipboardContent = getClipboardContent();
                if (!clipboardContent.empty()) {
                    clearLines(commandLines);
                    
                    std::istringstream stream(clipboardContent);
                    std::string line;
                    bool isFirstLine = true;
                    
                    while (std::getline(stream, line)) {
                        if (isFirstLine) {
                            commandLines[cursorPositionY].insert(cursorPositionX, line);
                            cursorPositionX += line.length();
                            isFirstLine = false;
                        } else {
                            cursorPositionY++;
                            if (cursorPositionY >= commandLines.size()) {
                                commandLines.push_back(line);
                            } else {
                                commandLines.insert(commandLines.begin() + cursorPositionY, line);
                            }
                            cursorPositionX = line.length();
                        }
                    }
                    
                    reprintCommandLines(commandLines, terminalSetting);
                    placeCursor(cursorPositionX, cursorPositionY);
                }
            } else if (c == '\033') {
                std::cin.get(c);
                if (c == '[') {
                    std::cin.get(c);
                    if (c == 'Z') {
                        if (!commandLines[cursorPositionY].empty()) {
                            if (currentCompletions.empty()) {
                                originalInput = commandLines[cursorPositionY];
                                currentCompletions = getTabCompletions(originalInput);
                            }
                            
                            if (!currentCompletions.empty()) {
                                clearLines(commandLines);
                                
                                if (tabCompletionIndex == 0) {
                                    tabCompletionIndex = currentCompletions.size() - 1;
                                } else {
                                    tabCompletionIndex = (tabCompletionIndex - 1) % currentCompletions.size();
                                }
                                
                                completionBrowsingMode = true;
                                
                                std::string completion = currentCompletions[tabCompletionIndex];
                                if (completion.length() > originalInput.length() && startsWith(completion, originalInput)) {
                                    std::string suggestion = completion.substr(originalInput.length());
                                    hasSuggestion = true;
                                    currentSuggestion = suggestion;
                                }
                                
                                reprintCommandLines(commandLines, terminalSetting);
                                if (hasSuggestion && !currentSuggestion.empty()) {
                                    std::cout << "\033[7m\033[97m" << currentSuggestion << "\033[0m";
                                    std::cout << "\033[" << currentSuggestion.length() << "D";
                                }
                                placeCursor(cursorPositionX, cursorPositionY);
                            }
                        }
                    } else {
                        std::string returnedArrowContent = handleArrowKey(c, cursorPositionX, cursorPositionY, commandLines, commandLines[cursorPositionY], terminalSetting);
                        if(returnedArrowContent != ""){
                            clearLines(commandLines);
                            commandLines.clear();
                            commandLines.push_back("");
                            cursorPositionX = 0;
                            cursorPositionY = 0;
                            
                            std::istringstream stream(returnedArrowContent);
                            std::string line;
                            bool isFirstLine = true;
                            while (std::getline(stream, line)) {
                                if (isFirstLine) {
                                    commandLines[0] = line;
                                    cursorPositionX = line.length();
                                    isFirstLine = false;
                                } else {
                                    cursorPositionY++;
                                    commandLines.push_back(line);
                                    cursorPositionX = line.length();
                                }
                            }
                            reprintCommandLines(commandLines, terminalSetting);
                            placeCursor(cursorPositionX, cursorPositionY);
                        }
                    }
                }
            } else if (c == '\n') {
                if (hasSuggestion) {
                    std::cout << "\033[K";
                    hasSuggestion = false;
                    currentSuggestion = "";
                }
                std::cout << std::endl;
                break;
            } else if (c == 127) {
                if (hasSuggestion) {
                    std::cout << "\033[K";
                    hasSuggestion = false;
                    currentSuggestion = "";
                }
                
                clearLines(commandLines);
                if (commandLines[cursorPositionY].length() > 0 && cursorPositionX > 0) {
                    commandLines[cursorPositionY].erase(cursorPositionX - 1, 1);
                    cursorPositionX--;
                } else if (cursorPositionX == 0 && cursorPositionY > 0) {
                    cursorPositionX = commandLines[cursorPositionY - 1].length();
                    commandLines[cursorPositionY - 1] += commandLines[cursorPositionY];
                    commandLines.erase(commandLines.begin() + cursorPositionY);
                    cursorPositionY--;
                }
                reprintCommandLines(commandLines, terminalSetting);
                placeCursor(cursorPositionX, cursorPositionY);
                tabCompletionIndex = 0;
                currentCompletions.clear();
                originalInput = "";
                
                showInlineSuggestion(commandLines[cursorPositionY]);
            } else if (c == '\t') {
                if (!commandLines[cursorPositionY].empty()) {
                    if (hasSuggestion && !currentSuggestion.empty() && !completionBrowsingMode) {
                        clearLines(commandLines);
                        commandLines[cursorPositionY] += currentSuggestion;
                        cursorPositionX += currentSuggestion.length();
                        
                        hasSuggestion = false;
                        currentSuggestion = "";
                        
                        reprintCommandLines(commandLines, terminalSetting);
                        placeCursor(cursorPositionX, cursorPositionY);
                        
                        tabCompletionIndex = 0;
                        currentCompletions.clear();
                        originalInput = "";
                    } else if (completionBrowsingMode && !currentCompletions.empty()) {
                        clearLines(commandLines);
                        applyCompletion(tabCompletionIndex, commandLines[cursorPositionY], cursorPositionX, cursorPositionY);
                        completionBrowsingMode = false;
                        
                        reprintCommandLines(commandLines, terminalSetting);
                        placeCursor(cursorPositionX, cursorPositionY);
                        
                        tabCompletionIndex = 0;
                        currentCompletions.clear();
                        originalInput = "";
                    } else {
                        if (currentCompletions.empty()) {
                            originalInput = commandLines[cursorPositionY];
                            currentCompletions = getTabCompletions(originalInput);
                        }
                        
                        if (!currentCompletions.empty()) {
                            clearLines(commandLines);
                            
                            tabCompletionIndex = (tabCompletionIndex + 1) % currentCompletions.size();
                            completionBrowsingMode = true;
                            
                            std::string completion = currentCompletions[tabCompletionIndex];
                            if (completion.length() > originalInput.length() && startsWith(completion, originalInput)) {
                                std::string suggestion = completion.substr(originalInput.length());
                                hasSuggestion = true;
                                currentSuggestion = suggestion;
                            }
                            
                            reprintCommandLines(commandLines, terminalSetting);
                            if (hasSuggestion && !currentSuggestion.empty()) {
                                std::cout << "\033[7m\033[97m" << currentSuggestion << "\033[0m";
                                std::cout << "\033[" << currentSuggestion.length() << "D";
                            }
                            placeCursor(cursorPositionX, cursorPositionY);
                        }
                    }
                }
            } else {
                if (hasSuggestion) {
                    std::cout << "\033[K";
                    hasSuggestion = false;
                    currentSuggestion = "";
                }
                
                clearLines(commandLines);
                commandLines[cursorPositionY].insert(cursorPositionX, 1, c);
                int currentLineLength;
                if(cursorPositionY == 0){
                    currentLineLength = commandLines[cursorPositionY].length() + terminalSettingLength;
                } else {
                    currentLineLength = commandLines[cursorPositionY].length();
                }
                if (currentLineLength < getTerminalWidth()) {
                    cursorPositionX++;
                } else {
                    cursorPositionY++;
                    commandLines.push_back("");
                    cursorPositionX = 0;
                }
                reprintCommandLines(commandLines, terminalSetting);
                placeCursor(cursorPositionX, cursorPositionY);
                
                tabCompletionIndex = 0;
                currentCompletions.clear();
                originalInput = "";
                
                showInlineSuggestion(commandLines[cursorPositionY]);
            }
        }
        disableRawMode();
        std::string finalCommand;
        for (const auto& line : commandLines) {
            finalCommand += line;
        }
        notifyPluginsTriggerMainProcess("command_processed" , finalCommand);
        commandParser(finalCommand);
        notifyPluginsTriggerMainProcess("end", processId);
        if (exitFlag) {
            break;
        }
    }
}

void clearLines(const std::vector<std::string>& commandLines){
    std::cout << "\033[2K\r";
    if(commandLines.size() > 1){
        for (int i = 0; i < commandLines.size(); i++) {
            if (i > 0) {
                std::cout << "\033[A";
            }
            std::cout << "\033[2K\r";
        }
    }
}

void reprintCommandLines(const std::vector<std::string>& commandLines, const std::string& terminalSetting) {
    for (int i = 0; i < commandLines.size(); i++) {
        if (i == 0) {
            std::cout << terminalSetting << commandLines[i];
        } else {
            std::cout << commandLines[i];
        }
        if (i < commandLines.size() - 1) {
            std::cout << std::endl;
        }
    }
}

void placeCursor(size_t& cursorPositionX, size_t& cursorPositionY){
    int columnsBehind = commandLines[cursorPositionY].length() - cursorPositionX;
    int rowsBehind = commandLines.size() - cursorPositionY - 1;
    if (columnsBehind > 0) {
        while (columnsBehind > 0) {
            std::cout << "\033[D";
            columnsBehind--;
        }
    }
    if(rowsBehind > 0){
        while (rowsBehind > 0) {
            std::cout << "\033[A";
            rowsBehind--;
        }
    }
}

std::string handleArrowKey(char arrow, size_t& cursorPositionX, size_t& cursorPositionY, std::vector<std::string>& commandLines, std::string& command, const std::string& terminalTag) {
    if (hasSuggestion) {
        std::cout << "\033[K";
        hasSuggestion = false;
        currentSuggestion = "";
    }
    
    switch (arrow) {
        case 'A':
            return terminal.getPreviousCommand();
        case 'B':
            return terminal.getNextCommand();
        case 'C':
            if (cursorPositionX < command.length()) {
                cursorPositionX++;
                std::cout << "\033[C";
            } else if (cursorPositionY < commandLines.size() - 1) {
                cursorPositionY++;
                cursorPositionX = 0;
                std::cout << "\033[B";
            }
            return "";
        case 'D':
            if (cursorPositionX > 0) {
                cursorPositionX--;
                std::cout << "\033[D";
            } else if (cursorPositionY > 0) {
                cursorPositionY--;
                cursorPositionX = commandLines[cursorPositionY].length();
                std::cout << "\033[A";
            }
            return "";
    }
    return "";
}

std::string getClipboardContent() {
    std::string result;
    usleep(10000);
    
    FILE* pipe = popen("pbpaste", "r");
    if (!pipe) {
        std::cerr << "Failed to access clipboard" << std::endl;
        return "";
    }
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    int status = pclose(pipe);
    if (status != 0) {
        std::cerr << "Clipboard command failed with status: " << status << std::endl;
        return "";
    }

    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    return result;
}

void setRawMode(bool enable) {
    std::lock_guard<std::mutex> lock(rawModeMutex);
    if (rawModeEnabled == enable) {
        return;
    }
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
    rawModeEnabled = enable;
}

void enableRawMode() {
    setRawMode(true);
}

void disableRawMode() {
    setRawMode(false);
}

bool isRawModeEnabled() {
    std::lock_guard<std::mutex> lock(rawModeMutex);
    return rawModeEnabled;
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
        userData["Command_Prefix"] = commandPrefix;
        userData["Shortcuts_Prefix"] = shortcutsPrefix;
        userData["Multi_Script_Shortcuts"] = multiScriptShortcuts;
        userData["Last_Updated"] = lastUpdated;
        userData["Current_Theme"] = currentTheme;
        userData["Auto_Update_Check"] = checkForUpdates;
        userData["Aliases"] = aliases;
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
    if (!runningStartup) {
        addUserInputToHistory(command);
    }
    if (command.rfind(commandPrefix, 0) == 0) {
        commandProcesser(command.substr(1));
        terminal.addCommandToHistory(command);
        return;
    }
    if (command.rfind(shortcutsPrefix, 0) == 0) {
        multiScriptShortcutProcesser(command);
        return;
    }
    if (defaultTextEntryOnAI) {
        chatProcess(command);
        terminal.addCommandToHistory(command);
    } else {
        sendTerminalCommand(command);
    }
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
                commandParser(commandPrefix + cmd);
            }
        } else {
            std::cerr << "No command for given shortcut: " << strippedCommand << std::endl;
        }
    } else {
        std::cerr << "No shortcuts have been created." << std::endl;
    }
}

void commandProcesser(const std::string& command) {
    commandsQueue = std::queue<std::string>();
    auto commands = commandSplicer(command);
    if(aliasesEnabled){
        for (const auto& cmd : commands) {
            if (aliases.find(cmd) != aliases.end()) {
                std::string aliasCommand = aliases[cmd];
                std::vector<std::string> aliasCommands = commandSplicer(aliasCommand);
                for (const auto& aliasCmd : aliasCommands) {
                    commandsQueue.push(aliasCmd);
                }
            } else {
                commandsQueue.push(cmd);
            }
        }
    } else {
        for (const auto& cmd : commands) {
            commandsQueue.push(cmd);
        }
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
    } else if (lastCommandParsed == "clear") {
        sendTerminalCommand("clear");
    } else if (lastCommandParsed == "exit" || lastCommandParsed == "quit") {
        exitFlag = true;
        return;
    } else if (lastCommandParsed == "ai") {
        aiSettingsCommands();
    } else if (lastCommandParsed == "user") {
        userSettingsCommands();
        writeUserData();
    } else if (lastCommandParsed == "aihelp"){
        if (!defaultTextEntryOnAI && !c_assistant.getAPIKey().empty() ){
            std::string message = ("I am encountering these errors in the " + terminal.getTerminalName() + " and would like some help solving these issues. I entered: " + terminal.returnMostRecentUserInput() + " and got this " + terminal.returnMostRecentTerminalOutput());
            if (TESTING) {
                std::cout << message << std::endl;
            }
            std::cout << c_assistant.forceDirectChatGPT(message, false) << std::endl;
            return;
        }
    } else if(lastCommandParsed == "version") {
        std::cout << "CJ's Shell v" + currentVersion << std::endl;
    } else if (lastCommandParsed == "terminal") {
        try {
            std::string remainingCommands;
            while (!commandsQueue.empty()) {
                std::string command = commandsQueue.front();
                commandsQueue.pop();
                if (remainingCommands.empty()) {
                    remainingCommands = command;
                } else {
                    remainingCommands += " " + command;
                }
            }
            if (remainingCommands.empty()) {
                defaultTextEntryOnAI = false;
                return;
            }
            sendTerminalCommand(remainingCommands);
        } catch (std::out_of_range& e) {
            defaultTextEntryOnAI = false;
            return;
        }
    } else if(lastCommandParsed == "plugin") {
        pluginCommands();
    } else if (lastCommandParsed == "theme") {
        themeCommands();
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
    } else if (lastCommandParsed == "refresh-commands" || lastCommandParsed == "refresh-executables") {
        std::cout << "Refreshing executable commands cache..." << std::endl;
        refreshExecutablesCache();
        std::cout << "Discovered " << executablesCache.size() << " executable commands." << std::endl;
        return;
    } else {
        std::queue<std::string> tempQueue;
        tempQueue.push(lastCommandParsed);
        while (!commandsQueue.empty()) {
            tempQueue.push(commandsQueue.front());
            commandsQueue.pop();
        }
        std::vector<std::string> enabledPlugins = pluginManager->getEnabledPlugins();
        for(const auto& plugin : enabledPlugins){
            std::vector<std::string> pluginCommands = pluginManager->getPluginCommands(plugin);
            if(std::find(pluginCommands.begin(), pluginCommands.end(), lastCommandParsed) != pluginCommands.end()){
                pluginManager->handlePluginCommand(plugin, tempQueue);
                return;
            }
        }
        std::cerr << "Unknown command. Please try again." << std::endl;
    }
}

void printHelp() {
    std::cout << " commandprefix: Change the command prefix (" << commandPrefix << ")" << std::endl;
    std::cout << " ai: Access AI command settings and chat" << std::endl;
    std::cout << " approot: Switch to the application directory" << std::endl;
    std::cout << " terminal [ARGS]: Run terminal commands" << std::endl;
    std::cout << " user: Access user settings" << std::endl;
    std::cout << " aihelp: Get AI troubleshooting help" << std::endl;
    std::cout << " theme: Manage themes (load/save)" << std::endl;
    std::cout << " version: Display application version" << std::endl;
    std::cout << " plugin: Manage plugins" << std::endl;
    std::cout << " env: Manage environment variables" << std::endl;
    std::cout << " uninstall: Uninstall the application" << std::endl;
    std::cout << " refresh-commands: Refresh the executable commands cache" << std::endl;
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
    terminal.setAliases(aliases);
    std::thread commandThread = terminal.executeCommand(command);
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
    if (lastCommandParsed == "alias") {
        aliasCommands();
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
        std::cout << " text: Configure text settings (commandprefix, displayfullpath, defaultentry)" << std::endl;
        std::cout << " shortcut: Manage shortcuts (add, remove, list)" << std::endl;
        std::cout << " alias: Manage aliases (add, remove, list)" << std::endl;
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

void aliasCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cout << "Aliases are currently " << (aliasesEnabled ? "enabled." : "disabled.") << std::endl;
        return;
    }
    if (lastCommandParsed == "enable") {
        aliasesEnabled = true;
        std::cout << "Aliases enabled." << std::endl;
        writeUserData();
        return;
    }
    if (lastCommandParsed == "disable") {
        aliasesEnabled = false;
        std::cout << "Aliases disabled." << std::endl;
        writeUserData();
        return;
    }
    if (lastCommandParsed == "add") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        std::string aliasName = lastCommandParsed;
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        std::string aliasValue = lastCommandParsed;
        aliases[aliasName] = aliasValue;
        writeUserData();
        std::cout << "Alias added: " << aliasName << " -> " << aliasValue << std::endl;
        return;
    }
    if (lastCommandParsed == "remove") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        if (aliases.empty()) {
            std::cerr << "No aliases found." << std::endl;
            return;
        }
        if (aliases.find(lastCommandParsed) == aliases.end()) {
            std::cerr << "Alias not found: " << lastCommandParsed << std::endl;
            return;
        }
        if(lastCommandParsed == "all") {
            aliases.clear();
            writeUserData();
            std::cout << "All aliases removed." << std::endl;
            return;
        }
        std::string aliasName = lastCommandParsed;
        aliases.erase(aliasName);
        writeUserData();
        std::cout << "Alias removed: " << aliasName << std::endl;
        return;
    }
    if (lastCommandParsed == "list") {
        if (!aliases.empty()) {
            for (const auto& [key, value] : aliases) {
                std::cout << key << ": " << value << std::endl;
            }
        } else {
            std::cerr << "No aliases found." << std::endl;
        }
        return;
    }
    if (lastCommandParsed == "help") {
        std::cout << "Alias commands:" << std::endl;
        std::cout << " add [NAME] [VALUE]: Add a new alias" << std::endl;
        std::cout << " remove [NAME]: Remove an existing alias" << std::endl;
        std::cout << " list: List all aliases" << std::endl;
        return;
    }
    if (lastCommandParsed == "help") {
        std::cout << "Alias commands:" << std::endl;
        std::cout << " add [NAME] [VALUE]: Add a new alias" << std::endl;
        std::cout << " remove [NAME]: Remove an existing alias" << std::endl;
        std::cout << " list: List all aliases" << std::endl;
        return;
    }
    std::cerr << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
                commandParser(commandPrefix + command);
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

    if (lastCommandParsed == "commandprefix") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Command prefix is currently " + commandPrefix << std::endl;
            return;
        }
        
        if (validatePrefix(lastCommandParsed)) {
            commandPrefix = lastCommandParsed;
            std::cout << "Command prefix set to " + commandPrefix << std::endl;
        }
        return;
    }
    
    if (lastCommandParsed == "shortcutprefix") {
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
        std::cout << " commandprefix [CHAR]: Set the command prefix" << std::endl;
        std::cout << " shortcutprefix [CHAR]: Set the shortcut prefix" << std::endl;
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

bool downloadLatestRelease(){
    if(std::filesystem::exists(UPDATE_SCRIPT_PATH)){
        sendTerminalCommand((UPDATE_SCRIPT_PATH).string());
        std::cout << "Update script executed." << std::endl;
        exitFlag = true;
        return true;
    } else {
        std::cerr << "Error: Update script not found." << std::endl;
        return false;
    }
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
}

void loadTheme(const std::string& themeName) {
    if (themeManager->loadTheme(themeName)) {
        currentTheme = themeName;
        applyColorToStrings();
    }
}

void saveTheme(const std::string& themeName) {
    std::map<std::string, std::string> colors = {
        {"GREEN_COLOR_BOLD", GREEN_COLOR_BOLD},
        {"RESET_COLOR", RESET_COLOR},
        {"RED_COLOR_BOLD", RED_COLOR_BOLD},
        {"PURPLE_COLOR_BOLD", PURPLE_COLOR_BOLD},
        {"BLUE_COLOR_BOLD", BLUE_COLOR_BOLD},
        {"YELLOW_COLOR_BOLD", YELLOW_COLOR_BOLD},
        {"CYAN_COLOR_BOLD", CYAN_COLOR_BOLD},
        {"SHELL_COLOR", terminal.getShellColor()},
        {"DIRECTORY_COLOR", terminal.getDirectoryColor()},
        {"BRANCH_COLOR", terminal.getBranchColor()},
        {"GIT_COLOR", terminal.getGitColor()}

    };
    if (themeManager->saveTheme(themeName, colors)) {
        std::cout << "Theme saved: " << themeName << std::endl;
    } else {
        std::cerr << "Failed to save theme: " << themeName << std::endl;
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
    } else if (lastCommandParsed == "save") {
        getNextCommand();
        if (!lastCommandParsed.empty()) {
            saveTheme(lastCommandParsed);
        } else {
            std::cerr << "No theme name provided to save." << std::endl;
        }
    } else {
        std::cerr << "Unknown theme command. Use 'load' or 'save'." << std::endl;
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

std::vector<std::string> getTabCompletions(const std::string& input) {
    std::vector<std::string> completions;
    
    if (input.find(' ') != std::string::npos) {
        size_t spacePos = input.find_first_of(' ');
        std::string command = input.substr(0, spacePos);
        std::string argument = input.substr(spacePos + 1);
        
        if (command == "cd" || command == "ls" || command == "mkdir" || command == "rmdir" ||
            command == "cat" || command == "cp" || command == "mv" || command == "rm" ||
            command == "touch" || command == "less" || command == "nano" || command == "vim") {
            if (argument.find('/') != std::string::npos) {
                std::string completedPath = completeFilePath(input);
                if (!completedPath.empty()) {
                    completions.push_back(completedPath);
                }
                return completions;
            }
            
            std::string dir = terminal.getCurrentFilePath();
            try {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    std::string entryName = entry.path().filename().string();
                    if (startsWithCaseInsensitive(entryName, argument)) {
                        if ((command == "cd" || command == "rmdir") && !entry.is_directory()) {
                            continue;
                        }
                        completions.push_back(command + " " + entryName);
                    }
                }
            } catch (const std::filesystem::filesystem_error& e) {
            }
            
            return completions;
        }
        
        if (command == commandPrefix + "user") {
            std::vector<std::string> userCommands = {
                "startup", "text", "shortcut", "alias", "testing", "data", 
                "saveloop", "saveonexit", "checkforupdates", "updatepath", "help"
            };
            
            for (const auto& cmd : userCommands) {
                if (startsWith(cmd, argument)) {
                    completions.push_back(command + " " + cmd);
                }
            }
            
            if (argument.find(' ') != std::string::npos) {
                size_t subCmdPos = argument.find_first_of(' ');
                std::string subCommand = argument.substr(0, subCmdPos);
                std::string subArg = argument.substr(subCmdPos + 1);
                
                std::vector<std::pair<std::string, std::vector<std::string>>> userSubCommands = {
                    {"startup", {"add", "remove", "clear", "enable", "disable", "list", "runall", "help"}},
                    {"text", {"commandprefix", "shortcutprefix", "displayfullpath", "defaultentry", "help"}},
                    {"shortcut", {"enable", "disable", "list", "add", "remove", "help"}},
                    {"alias", {"enable", "disable", "add", "remove", "list", "help"}},
                    {"testing", {"enable", "disable"}},
                    {"saveloop", {"enable", "disable"}},
                    {"saveonexit", {"enable", "disable"}},
                    {"checkforupdates", {"enable", "disable"}},
                    {"updatepath", {"github", "cadenfinley"}},
                    {"data", {"get", "clear", "help"}},
                    {"update", {"check", "interval", "help"}}
                };
                
                for (const auto& [cmd, subCmds] : userSubCommands) {
                    if (subCommand == cmd) {
                        for (const auto& subcmd : subCmds) {
                            if (subArg.empty() || startsWith(subcmd, subArg)) {
                                completions.push_back(command + " " + subCommand + " " + subcmd);
                            }
                        }
                    }
                }
                
                if (subCommand == "data" && subArg.find(' ') != std::string::npos) {
                    size_t thirdCmdPos = subArg.find_first_of(' ');
                    std::string thirdCmd = subArg.substr(0, thirdCmdPos);
                    std::string thirdArg = subArg.substr(thirdCmdPos + 1);
                    
                    if (thirdCmd == "get") {
                        std::vector<std::string> getOpts = {"userdata", "userhistory", "all"};
                        for (const auto& opt : getOpts) {
                            if (thirdArg.empty() || startsWith(opt, thirdArg)) {
                                completions.push_back(command + " " + subCommand + " " + thirdCmd + " " + opt);
                            }
                        }
                    }
                }
                
                if (subCommand == "update" && subArg.find(' ') != std::string::npos) {
                    size_t thirdCmdPos = subArg.find_first_of(' ');
                    std::string thirdCmd = subArg.substr(0, thirdCmdPos);
                    std::string thirdArg = subArg.substr(thirdCmdPos + 1);
                    
                    if (thirdCmd == "interval") {
                        if (thirdArg.empty()) {
                            completions.push_back(command + " " + subCommand + " " + thirdCmd + " [HOURS]");
                        }
                    }
                }
            }
            
            return completions;
        }
        
        if (command == commandPrefix + "ai") {
            std::vector<std::string> aiCommands = {
                "log", "apikey", "chat", "get", "dump", "mode", "file",
                "directory", "model", "rejectchanges", "timeoutflag", "help"
            };
            
            for (const auto& cmd : aiCommands) {
                if (startsWith(cmd, argument)) {
                    completions.push_back(command + " " + cmd);
                }
            }
            
            if (argument.find(' ') != std::string::npos) {
                size_t subCmdPos = argument.find_first_of(' ');
                std::string subCommand = argument.substr(0, subCmdPos);
                std::string subArg = argument.substr(subCmdPos + 1);
                
                std::vector<std::pair<std::string, std::vector<std::string>>> aiSubCommands = {
                    {"apikey", {"set", "get"}},
                    {"chat", {"history", "cache", "help"}},
                    {"file", {"add", "remove", "active", "available", "refresh", "clear"}},
                    {"directory", {"set", "clear"}},
                    {"mode", {"chat", "code", "document", "creative", "assistant"}}
                };
                
                for (const auto& [cmd, subCmds] : aiSubCommands) {
                    if (subCommand == cmd) {
                        for (const auto& subcmd : subCmds) {
                            if (subArg.empty() || startsWith(subcmd, subArg)) {
                                completions.push_back(command + " " + subCommand + " " + subcmd);
                            }
                        }
                    }
                }
                
                if (subCommand == "chat" && subArg.find(' ') != std::string::npos) {
                    size_t thirdCmdPos = subArg.find_first_of(' ');
                    std::string thirdCmd = subArg.substr(0, thirdCmdPos);
                    std::string thirdArg = subArg.substr(thirdCmdPos + 1);
                    
                    if (thirdCmd == "cache") {
                        std::vector<std::string> cacheOpts = {"enable", "disable", "clear"};
                        for (const auto& opt : cacheOpts) {
                            if (thirdArg.empty() || startsWith(opt, thirdArg)) {
                                completions.push_back(command + " " + subCommand + " " + thirdCmd + " " + opt);
                            }
                        }
                    }
                    
                    if (thirdCmd == "history") {
                        std::vector<std::string> historyOpts = {"clear"};
                        for (const auto& opt : historyOpts) {
                            if (thirdArg.empty() || startsWith(opt, thirdArg)) {
                                completions.push_back(command + " " + subCommand + " " + thirdCmd + " " + opt);
                            }
                        }
                    }
                }
                
                if (subCommand == "file" && subArg.find(' ') != std::string::npos) {
                    size_t thirdCmdPos = subArg.find_first_of(' ');
                    std::string thirdCmd = subArg.substr(0, thirdCmdPos);
                    std::string thirdArg = subArg.substr(thirdCmdPos + 1);
                    
                    if (thirdCmd == "add" || thirdCmd == "remove") {
                        if (thirdArg.empty() || thirdArg == "a") {
                            completions.push_back(command + " " + subCommand + " " + thirdCmd + " all");
                        }
                        
                        std::string dir = terminal.getCurrentFilePath();
                        try {
                            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                                std::string entryName = entry.path().filename().string();
                                if (thirdArg.empty() || startsWithCaseInsensitive(entryName, thirdArg)) {
                                    completions.push_back(command + " " + subCommand + " " + thirdCmd + " " + entryName);
                                }
                            }
                        } catch (const std::filesystem::filesystem_error& e) {
                        }
                    }
                }
            }
            
            return completions;
        }
        
        if (command == commandPrefix + "plugin") {
            std::vector<std::string> pluginCommands = {
                "help", "available", "enabled", "enable", "disable", "info", "commands", "settings",
                "enableall", "disableall", "install", "uninstall"
            };
            
            for (const auto& cmd : pluginCommands) {
                if (startsWith(cmd, argument)) {
                    completions.push_back(command + " " + cmd);
                }
            }
            
            if (pluginManager && (argument == "enable" || argument == "disable" || argument == "info" || argument == "commands" || argument == "settings" || argument == "uninstall")) {
                std::vector<std::string> availPlugins = pluginManager->getAvailablePlugins();
                std::vector<std::string> enabledPlugins = pluginManager->getEnabledPlugins();
                
                std::vector<std::string> pluginList;
                if (argument == "enable") {
                    for (const auto& plugin : availPlugins) {
                        if (std::find(enabledPlugins.begin(), enabledPlugins.end(), plugin) == enabledPlugins.end()) {
                            pluginList.push_back(plugin);
                        }
                    }
                } else if (argument == "disable" || argument == "info" || argument == "commands" || argument == "settings") {
                    pluginList = enabledPlugins;
                } else if (argument == "uninstall") {
                    pluginList = availPlugins;
                }
                
                for (const auto& plugin : pluginList) {
                    completions.push_back(command + " " + argument + " " + plugin);
                }
            }
            
            if (argument.find(' ') != std::string::npos) {
                size_t subCmdPos = argument.find_first_of(' ');
                std::string subCommand = argument.substr(0, subCmdPos);
                std::string subArg = argument.substr(subCmdPos + 1);
                
                if (subCommand == "settings" && pluginManager) {
                    if (subArg.find(' ') != std::string::npos) {
                        size_t pluginPos = subArg.find_first_of(' ');
                        std::string pluginName = subArg.substr(0, pluginPos);
                        std::string settingArg = subArg.substr(pluginPos + 1);
                        
                        if (startsWith(settingArg, "s")) {
                            completions.push_back(command + " " + subCommand + " " + pluginName + " set");
                        }
                        
                        if (settingArg == "set") {
                            completions.push_back(command + " " + subCommand + " " + pluginName + " set [SETTING]");
                        }
                    } else {
                        for (const auto& plugin : pluginManager->getEnabledPlugins()) {
                            if (subArg.empty() || startsWithCaseInsensitive(plugin, subArg)) {
                                completions.push_back(command + " " + subCommand + " " + plugin);
                            }
                        }
                    }
                }
            }
            
            return completions;
        }
        
        if (command == commandPrefix + "theme") {
            std::vector<std::string> themeCommands = {"load", "save"};
            
            for (const auto& cmd : themeCommands) {
                if (startsWith(cmd, argument)) {
                    completions.push_back(command + " " + cmd);
                }
            }
            
            if (themeManager && argument == "load") {
                std::vector<std::string> availableThemes;
                for (const auto& theme : themeManager->getAvailableThemes()) {
                    availableThemes.push_back(theme.first);
                }
                for (const auto& theme : availableThemes) {
                    completions.push_back(command + " load " + theme);
                }
            }
            
            return completions;
        }
        
        if (command == commandPrefix + "terminal") {
            std::string completedPath = completeFilePath(input);
            if (!completedPath.empty()) {
                completions.push_back(completedPath);
            }
            return completions;
        }
    }
    
    if (input.find('/') != std::string::npos || (input.size() >= 2 && input.substr(0, 2) == "./") || (input.size() >= 1 && input[0] == '/')) {
        std::string completedPath = completeFilePath(input);
        if (!completedPath.empty()) {
            completions.push_back(completedPath);
        }
        return completions;
    }
    
    std::vector<std::string> commonCommands = {
        "cd", "ls", "mkdir", "rmdir", "touch", "cp", "mv", "rm", "cat", "grep", "find",
        "git", "python", "python3", "node", "npm", "yarn", "docker", "make", "gcc",
        "clear", "echo", "exit", "pwd", "less", "nano", "vim", "ssh", "scp", "tar",
        "curl", "wget", "ping", "ifconfig", "netstat", "top", "htop", "ps"
    };
    
    for (const auto& cmd : commonCommands) {
        if (startsWith(cmd, input)) {
            completions.push_back(cmd);
        }
    }

    if (!executablesCacheInitialized) {
        refreshExecutablesCache();
    }
    
    for (const auto& exec : executablesCache) {
        if (startsWith(exec, input) && 
            std::find(completions.begin(), completions.end(), exec) == completions.end()) {
            completions.push_back(exec);
        }
    }
    
    std::vector<std::string> recentCommands = terminal.getCommandHistory(10);
    for (const auto& cmd : recentCommands) {
        if (startsWith(cmd, input) && 
            std::find(completions.begin(), completions.end(), cmd) == completions.end()) {
            completions.insert(completions.begin(), cmd);
        }
    }
    
    if (input.size() >= commandPrefix.size() && 
        input.substr(0, commandPrefix.size()) == commandPrefix) {
        std::string cmdInput = input.substr(commandPrefix.length());
        
        std::vector<std::string> builtInCommands = {
            "ai", "approot", "clear", "terminal", "user", "aihelp", 
            "version", "plugin", "theme", "help", "uninstall", "env"
        };
        
        for (const auto& cmd : builtInCommands) {
            if (startsWith(cmd, cmdInput)) {
                completions.push_back(commandPrefix + cmd);
            }
        }
        
        if (pluginManager) {
            for (const auto& pluginName : pluginManager->getEnabledPlugins()) {
                std::vector<std::string> pluginCommands = pluginManager->getPluginCommands(pluginName);
                for (const auto& pluginCmd : pluginCommands) {
                    if (startsWith(pluginCmd, cmdInput)) {
                        completions.push_back(commandPrefix + pluginCmd);
                    }
                }
            }
        }
        
        return completions;
    }
    
    if (input.size() >= shortcutsPrefix.size() && 
        input.substr(0, shortcutsPrefix.size()) == shortcutsPrefix) {
        std::string shortcutInput = input.substr(shortcutsPrefix.length());
        
        for (const auto& [key, _] : multiScriptShortcuts) {
            if (startsWith(key, shortcutInput)) {
                completions.push_back(shortcutsPrefix + key);
            }
        }
        
        return completions;
    }
    
    std::string completedPath = completeFilePath(input);
    if (!completedPath.empty()) {
        completions.push_back(completedPath);
    }
    
    return completions;
}

std::string completeFilePath(const std::string& input) {
    std::string prefix = "";
    std::string pathToComplete = input;
    
    if (input.find(' ') != std::string::npos) {
        size_t spacePos = input.find_first_of(' ');
        prefix = input.substr(0, spacePos + 1);
        pathToComplete = input.substr(spacePos + 1);
    }
    
    std::filesystem::path basePath = terminal.getCurrentFilePath();
    
    std::string directoryPart;
    std::string filenamePart;
    
    if (pathToComplete.find('/') != std::string::npos) {
        size_t lastSlash = pathToComplete.find_last_of('/');
        directoryPart = pathToComplete.substr(0, lastSlash + 1);
        filenamePart = pathToComplete.substr(lastSlash + 1);
    } else {
        directoryPart = "";
        filenamePart = pathToComplete;
    }
    
    std::filesystem::path searchPath;
    
    if (directoryPart.empty()) {
        searchPath = basePath;
    } else if (directoryPart[0] == '/') {
        searchPath = directoryPart;
    } else if (directoryPart[0] == '~' && directoryPart.length() > 1 && directoryPart[1] == '/') {
        std::string homeDir = std::getenv("HOME") ? std::getenv("HOME") : "";
        if (!homeDir.empty()) {
            directoryPart.replace(0, 1, homeDir);
            searchPath = directoryPart;
        } else {
            searchPath = basePath / directoryPart;
        }
    } else {
        searchPath = basePath / directoryPart;
    }
    
    std::vector<std::string> matches;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(searchPath)) {
            std::string entryName = entry.path().filename().string();
            if (startsWithCaseInsensitive(entryName, filenamePart)) {
                std::string completion = directoryPart + entryName;
                if (entry.is_directory()) {
                    completion += "/";
                }
                matches.push_back(prefix + completion);
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
    }
    
    if (matches.size() == 1) {
        return matches[0];
    } else if (matches.size() > 1) {
        std::vector<std::string> strippedMatches;
        for (const auto& match : matches) {
            strippedMatches.push_back(match.substr(prefix.length()));
        }
        
        std::string commonPrefix = getCommonPrefix(strippedMatches);
        return prefix + commonPrefix;
    }
    
    return "";
}

std::string getCommonPrefix(const std::vector<std::string>& strings) {
    if (strings.empty()) {
        return "";
    }
    
    if (strings.size() == 1) {
        return strings[0];
    }
    
    std::string prefix = strings[0];
    for (size_t i = 1; i < strings.size(); ++i) {
        size_t j = 0;
        while (j < prefix.length() && j < strings[i].length() && prefix[j] == strings[i][j]) {
            j++;
        }
        prefix = prefix.substr(0, j);
    }
    
    return prefix;
}

void displayCompletionOptions(const std::vector<std::string>& completions) {
    if (completions.empty()) {
        return;
    }
    
    const int termWidth = getTerminalWidth();
    int maxLength = 0;
    
    for (const auto& comp : completions) {
        maxLength = std::max(maxLength, static_cast<int>(comp.length()));
    }
    
    maxLength += 2;
    
    int numColumns = std::max(1, termWidth / maxLength);
    int numRows = (completions.size() + numColumns - 1) / numColumns;
    
    for (int row = 0; row < numRows; ++row) {
        for (int col = 0; col < numColumns; ++col) {
            int index = col * numRows + row;
            if (index < completions.size()) {
                std::cout << completions[index];
                
                int padding = maxLength - completions[index].length();
                for (int i = 0; i < padding; ++i) {
                    std::cout << " ";
                }
            }
        }
        std::cout << std::endl;
    }
}

void applyCompletion(size_t index, std::string& command, size_t& cursorPositionX, size_t& cursorPositionY) {
    if (index < currentCompletions.size()) {
        command = currentCompletions[index];
        cursorPositionX = command.length();
    }
}

void showInlineSuggestion(const std::string& input) {
    if (hasSuggestion) {
        std::cout << "\033[K";
        hasSuggestion = false;
        currentSuggestion = "";
    }
    
    if (input.empty()) {
        return;
    }
    
    std::vector<std::string> completions = getTabCompletions(input);
    
    if (!completions.empty()) {
        std::string completion = completions[0];
        
        if (completion.length() > input.length()) {
            if (startsWith(completion, input)) {
                currentSuggestion = completion.substr(input.length());
                
                if (!currentSuggestion.empty()) {
                    std::cout << "\033[97m" << currentSuggestion[0] << "\033[0m";
                    if (currentSuggestion.length() > 1) {
                        std::cout << "\033[38;5;249m" << currentSuggestion.substr(1) << "\033[0m";
                    }
                    
                    std::cout << "\033[" << currentSuggestion.length() << "D";
                    
                    hasSuggestion = true;
                }
            }
        }
    }
}

void refreshExecutablesCache() {
    if (executablesCacheInitialized && !hasPathChanged()) {
        return;
    }
    
    executablesCache.clear();
    
    char* pathEnv = getenv("PATH");
    if (!pathEnv) {
        executablesCacheInitialized = true;
        return;
    }
    
    std::string path(pathEnv);
    std::stringstream pathStream(path);
    std::string directory;
    
    std::vector<std::thread> scanThreads;
    std::mutex cacheMutex;
    std::atomic<int> discoveredCount(0);

    while (std::getline(pathStream, directory, ':')) {
        if (directory.empty()) continue;

        scanThreads.emplace_back([directory, &cacheMutex, &discoveredCount]() {
            try {
                if (!std::filesystem::exists(directory)) {
                    return;
                }
                
                std::vector<std::string> localCache;
                for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                    if (entry.is_regular_file() || entry.is_symlink()) {
                        try {
                            std::error_code ec;
                            auto perms = std::filesystem::status(entry.path(), ec).permissions();
                            
                            if ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none ||
                                (perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none ||
                                (perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none) {
                                
                                std::string execName = entry.path().filename().string();
                                if (!execName.empty() && execName[0] != '.') {
                                    localCache.push_back(execName);
                                }
                            }
                        } catch (const std::exception& e) {
                            continue;
                        }
                    }
                }
                
                std::lock_guard<std::mutex> lock(cacheMutex);
                for (const auto& name : localCache) {
                    if (std::find(executablesCache.begin(), executablesCache.end(), name) == executablesCache.end()) {
                        executablesCache.push_back(name);
                        discoveredCount++;
                    }
                }
            } catch (const std::filesystem::filesystem_error& e) {
            } catch (const std::exception& e) {
            }
        });
    }

    for (auto& thread : scanThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    std::sort(executablesCache.begin(), executablesCache.end());
    
    executablesCacheInitialized = true;
    
    if (TESTING) {
        std::cout << "Discovered " << discoveredCount << " executable commands." << std::endl;
    }
    
    saveExecutableCacheToDisk();
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

bool executeUpdateIfAvailable(bool updateAvailable) {
    if (!updateAvailable) return false;
    
    std::cout << "\nAn update is available. Would you like to download it? (Y/N)" << std::endl;
    char response;
    std::cin >> response;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    
    if (response != 'Y' && response != 'y') return false;
    
    saveUpdateCache(false, cachedLatestVersion);
    
    if (!downloadLatestRelease()) {
        std::cout << "Failed to download the update. Please try again later." << std::endl;
        saveUpdateCache(true, cachedLatestVersion);
        return false;
    }
    return true;
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
                if(userData.contains("Text_Entry")) {
                    defaultTextEntryOnAI = userData["Text_Entry"].get<bool>();
                }
                if(userData.contains("Command_Prefix")) {
                    commandPrefix = userData["Command_Prefix"].get<std::string>();
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
                if(userData.contains("Aliases")) {
                    aliases = userData["Aliases"].get<std::map<std::string, std::string>>();
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

bool hasPathChanged() {
    static std::string lastPathValue;
    std::string currentPath = getenv("PATH") ? getenv("PATH") : "";
    
    if (lastPathValue.empty()) {
        lastPathValue = currentPath;
        return true;
    }
    
    if (lastPathValue != currentPath) {
        lastPathValue = currentPath;
        return true;
    }
    
    return false;
}

void loadExecutableCacheFromDisk() {
    std::ifstream cacheFile(DATA_DIRECTORY / "executables_cache.json");
    if (!cacheFile.is_open()) return;
    
    try {
        json cacheData;
        cacheFile >> cacheData;
        cacheFile.close();
        
        std::string cachedPathValue = cacheData["path_value"].get<std::string>();
        std::string currentPath = getenv("PATH") ? getenv("PATH") : "";
        
        if (cachedPathValue == currentPath) {
            executablesCache = cacheData["executables"].get<std::vector<std::string>>();
            executablesCacheInitialized = true;
        } else {
            refreshExecutablesCache();
            saveExecutableCacheToDisk();
        }
    } catch (const std::exception& e) {
        refreshExecutablesCache();
        saveExecutableCacheToDisk();
    }
}

void saveExecutableCacheToDisk() {
    json cacheData;
    cacheData["executables"] = executablesCache;
    cacheData["path_value"] = getenv("PATH") ? getenv("PATH") : "";
    
    std::ofstream cacheFile(DATA_DIRECTORY / "executables_cache.json");
    if (cacheFile.is_open()) {
        cacheFile << cacheData.dump();
        cacheFile.close();
    }
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
        setenv("SHELL", (DATA_DIRECTORY / "cjsh").c_str(), 1);
        
        if (getenv("PATH") == nullptr) {
            setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1);
        }
        
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
    
    for (const auto& job : terminal.getActiveJobs()) {
        kill(-job.pid, SIGTERM);
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