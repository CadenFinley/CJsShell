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
#include <streambuf>
#include <ostream>

#include "nlohmann/json.hpp"

#include "terminalpassthrough.h"
#include "openaipromptengine.h"
#include "pluginmanager.h"
#include "thememanager.h"

using json = nlohmann::json;

bool TESTING = false;
bool runningStartup = false;
bool exitFlag = false;
bool defaultTextEntryOnAI = false;
bool displayWholePath = false;
bool rawModeEnabled = false;

bool saveLoop = false;
bool saveOnExit = false;
bool shotcutsEnabled = true;
bool startCommandsOn = true;
bool usingChatCache = true;
bool checkForUpdates = true;

std::string GREEN_COLOR_BOLD = "\033[1;32m";
std::string RESET_COLOR = "\033[0m";
std::string RED_COLOR_BOLD = "\033[1;31m";
std::string PURPLE_COLOR_BOLD = "\033[1;35m";
std::string BLUE_COLOR_BOLD = "\033[1;34m";
std::string YELLOW_COLOR_BOLD = "\033[1;33m";
std::string CYAN_COLOR_BOLD = "\033[1;36m";

std::string currentTheme = "default";
std::map<std::string, std::map<std::string, std::string>> availableThemes;

const std::string processId = std::to_string(getpid());
const std::string updateURL = "https://api.github.com/repos/cadenfinley/DevToolsTerminal/releases/latest";
const std::string githubRepoURL = "https://github.com/CadenFinley/DevToolsTerminal";
const std::string currentVersion = "1.8.0.1";

std::string commandPrefix = "!";
std::string lastCommandParsed;
std::string applicationDirectory;
std::string titleLine = "DevToolsTerminal v" + currentVersion + " - Caden Finley (c) 2025";
std::string createdLine = "Created 2025 @ " + PURPLE_COLOR_BOLD + "Abilene Christian University" + RESET_COLOR;
std::string lastUpdated = "N/A";
std::string lastLogin = "N/A";

std::filesystem::path DATA_DIRECTORY = ".DTT-Data";
std::filesystem::path USER_DATA = DATA_DIRECTORY / ".USER_DATA.json";
std::filesystem::path USER_COMMAND_HISTORY = DATA_DIRECTORY / ".USER_COMMAND_HISTORY.txt";
std::filesystem::path THEMES_DIRECTORY = DATA_DIRECTORY / "themes";
std::filesystem::path PLUGINS_DIRECTORY = DATA_DIRECTORY / "plugins";

std::queue<std::string> commandsQueue;
std::vector<std::string> startupCommands;
std::vector<std::string> savedChatCache;
std::vector<std::string> commandLines;
std::map<std::string, std::string> shortcuts;
std::map<std::string, std::vector<std::string>> multiScriptShortcuts;
std::map<std::string, std::string> envVars;

OpenAIPromptEngine c_assistant;
TerminalPassthrough terminal;
PluginManager* pluginManager = nullptr;
ThemeManager* themeManager = nullptr;

std::mutex rawModeMutex;

std::string readAndReturnUserDataFile();
std::vector<std::string> commandSplicer(const std::string& command);
void mainProcessLoop();
void setRawMode(bool enable);
void enableRawMode();
void disableRawMode();
bool isRawModeEnabled();
void createNewUSER_DATAFile();
void createNewUSER_HISTORYfile();
void loadUserData();
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
void multiScriptShortcutCommands();
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
void envVarCommands();
std::string generateUninstallScript();

int main() {

    startupCommands = {};
    shortcuts = {};
    multiScriptShortcuts = {};
    terminal = TerminalPassthrough();
    c_assistant = OpenAIPromptEngine("", "chat", "You are an AI personal assistant within a terminal application.", {}, ".DTT-Data");

    sendTerminalCommand("cd /");
    sendTerminalCommand("clear");

    applicationDirectory = std::filesystem::current_path().string();
    if (applicationDirectory.find(":") != std::string::npos) {
        applicationDirectory = applicationDirectory.substr(applicationDirectory.find(":") + 1);
    }

    if (!std::filesystem::exists(DATA_DIRECTORY)) {
        std::filesystem::create_directory(applicationDirectory / DATA_DIRECTORY);
    }

    if (!std::filesystem::exists(USER_DATA)) {
        createNewUSER_DATAFile();
    } else {
        loadUserData();
    }

    if (!std::filesystem::exists(USER_COMMAND_HISTORY)) {
        createNewUSER_HISTORYfile();
    }

    if(checkForUpdates){
        if (checkForUpdate()) {
            std::cout << "\nAn update is available. Would you like to download it? (Y/N)" << std::endl;
            char response;
            std::cin >> response;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (response == 'Y' || response == 'y') {
                if (!downloadLatestRelease()) {
                    std::cout << "Failed to download the update. Please try again later." << std::endl;
                }
            }
        } else {
            std::cout << " ->  You are up to date!" << std::endl;
        }
    }

    std::time_t now = std::time(nullptr);
    std::tm* now_tm = std::localtime(&now);
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now_tm);

    std::ifstream changelogFile(DATA_DIRECTORY / "CHANGELOG.txt");
    if (changelogFile.is_open()) {
        std::cout << "Thanks for downloading the latest version of DevToolsTerminal Version: " << currentVersion << std::endl;
        std::cout << "Check out the github repo for more information:\n" << githubRepoURL << std::endl;
        std::cout << "And check me out at CadenFinley.com" << std::endl;
        std::string changeLog((std::istreambuf_iterator<char>(changelogFile)), std::istreambuf_iterator<char>());
        changelogFile.close();
        displayChangeLog(changeLog);
        lastUpdated = buffer;
        std::cout << "Press enter to continue..." << std::endl;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::filesystem::remove(DATA_DIRECTORY / "CHANGELOG.txt");
    }

    pluginManager = new PluginManager(PLUGINS_DIRECTORY);
    pluginManager->discoverPlugins();

    themeManager = new ThemeManager(THEMES_DIRECTORY);
    if (themeManager->loadTheme(currentTheme)) {
        applyColorToStrings();
    }

    if (!startupCommands.empty() && startCommandsOn) {
        runningStartup = true;
        std::cout << "Running startup commands..." << std::endl;
        for (const auto& command : startupCommands) {
            commandParser(commandPrefix + command);
        }
        runningStartup = false;
    }

    std::cout << "Last Login: " << lastLogin << std::endl;
    lastLogin = buffer;

    std::cout << titleLine << std::endl;
    std::cout << createdLine << std::endl;

    //only breaks on the exitFlag being set to true
    mainProcessLoop();

    if(saveOnExit){
        savedChatCache = c_assistant.getChatCache();
        writeUserData();
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
            terminalSetting = GREEN_COLOR_BOLD + "AI Menu: " + RESET_COLOR;
            terminalSettingLength = 9;
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
        enableRawMode();
        while (true) {
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
            } else if (c == '\n') {
                std::cout << std::endl;
                break;
            } else if (c == 127) {
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
            } else {
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
    } else {
        std::cerr << "Error: Unable to create the user data file at " << USER_DATA << std::endl;
    }
}

void createNewUSER_HISTORYfile() {
    std::ofstream file(USER_COMMAND_HISTORY);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to create the user history file at " << USER_COMMAND_HISTORY << std::endl;
    }
}

void loadUserData() {
    std::ifstream file(USER_DATA);
    if (file.is_open()) {
        if (file.peek() == std::ifstream::traits_type::eof()) {
            std::cout << "User data file is empty. Creating new file..." << std::endl;
            file.close();
            createNewUSER_DATAFile();
            return;
        }
        try {
            json userData;
            file >> userData;
            if(userData.contains("OpenAI_API_KEY")){
                c_assistant.setAPIKey(userData["OpenAI_API_KEY"].get<std::string>());
            }
            if(userData.contains("Chat_Cache")) {
                savedChatCache = userData["Chat_Cache"].get<std::vector<std::string> >();
                c_assistant.setChatCache(savedChatCache);
            }
            if(userData.contains("Startup_Commands")){
                startupCommands = userData["Startup_Commands"].get<std::vector<std::string> >();
            }
            if(userData.contains("Shortcuts_Enabled")){
                shotcutsEnabled = userData["Shortcuts_Enabled"].get<bool>();
            }
            if(userData.contains("Shortcuts")){
                shortcuts = userData["Shortcuts"].get<std::map<std::string, std::string> >();
            }
            if(userData.contains("Text_Entry")){
                defaultTextEntryOnAI = userData["Text_Entry"].get<bool>();
            }
            if(userData.contains("Command_Prefix")){
                commandPrefix = userData["Command_Prefix"].get<std::string>();
            }
            if(userData.contains("Multi_Script_Shortcuts")){
                multiScriptShortcuts = userData["Multi_Script_Shortcuts"].get<std::map<std::string, std::vector<std::string>>>();
            }
            if(userData.contains("Last_Updated")){
                lastUpdated = userData["Last_Updated"].get<std::string>();
            }
            if(userData.contains("Last_Login")){
                lastLogin = userData["Last_Login"].get<std::string>();
            }
            if(userData.contains("EnvVars")){
                std::map<std::string, std::string> loadedEnvVars = userData["EnvVars"].get<std::map<std::string, std::string>>();
                for(const auto& [name, value] : loadedEnvVars) {
                    terminal.setEnvVar(name, value);
                }
            }
            if(userData.contains("Current_Theme")) {
                currentTheme = userData["Current_Theme"].get<std::string>();
            }
            file.close();
        }
        catch(const json::parse_error& e) {
            std::cerr << "Error parsing user data file: " << e.what() << "\nCreating new file." << std::endl;
            file.close();
            createNewUSER_DATAFile();
            return;
        }
    } else {
        std::cerr << "Error: Unable to read the user data file at " << USER_DATA << std::endl;
    }
}

void writeUserData() {
    std::ofstream file(USER_DATA);
    if (file.is_open()) {
        json userData;
        userData["OpenAI_API_KEY"] = c_assistant.getAPIKey();
        userData["Chat_Cache"] = savedChatCache;
        userData["Startup_Commands"] = startupCommands;
        userData["Shortcuts_Enabled"] = shotcutsEnabled;
        userData["Shortcuts"] = shortcuts;
        userData["Text_Buffer"] = false;
        userData["Text_Entry"] = defaultTextEntryOnAI;
        userData["Command_Prefix"] = commandPrefix;
        userData["Multi_Script_Shortcuts"] = multiScriptShortcuts;
        userData["Last_Updated"] = lastUpdated;
        userData["Last_Login"] = lastLogin;
        userData["EnvVars"] = terminal.getAllEnvVars();
        userData["Current_Theme"] = currentTheme;
        file << userData.dump(4);
        file.close();
    } else {
        std::cerr << "Error: Unable to write to the user data file at " << USER_DATA << std::endl;
    }
}

void goToApplicationDirectory() {
    commandProcesser("terminal cd /");
    commandProcesser("terminal cd " + applicationDirectory +"/"+ DATA_DIRECTORY.string());
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
    if (!runningStartup) {
        addUserInputToHistory(command);
    }
    if (command.rfind(commandPrefix, 0) == 0) {
        commandProcesser(command.substr(1));
        terminal.addCommandToHistory(command);
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

void shortcutProcesser(const std::string& command) {
    if (!shotcutsEnabled) {
        std::cout << "Shortcuts are disabled." << std::endl;
        return;
    }
    if (!shortcuts.empty()) {
        std::string strippedCommand = command.substr(2);
        trim(strippedCommand);
        if (strippedCommand.empty()) {
            std::cout << "No shortcut given." << std::endl;
            return;
        }

        std::istringstream iss(strippedCommand);
        std::string shortcutName;
        iss >> shortcutName;

        if (shortcuts.find(shortcutName) != shortcuts.end()) {
            std::string baseCommand = shortcuts[shortcutName];
            std::vector<std::string> dashArgs;
            std::string remainingArgs;
            std::string arg;
            while (iss >> arg) {
                if (arg.front() == '-') {
                    dashArgs.push_back(arg);
                } else {
                    remainingArgs += arg + " ";
                }
            }
            trim(remainingArgs);
            std::istringstream baseIss(baseCommand);
            std::string singleCommand;
            while (std::getline(baseIss, singleCommand, ';')) {
                trim(singleCommand);
                if (!singleCommand.empty()) {
                    std::string fullCommand = singleCommand;
                    if (!remainingArgs.empty()) {
                        fullCommand += remainingArgs;
                    }
                    commandParser(fullCommand);
                    for (const auto& dashArg : dashArgs) {
                        std::string processedArg = dashArg;
                        processedArg.erase(0, 1);
                        commandParser(processedArg);
                    }
                }
            }
        } else {
            std::cout << "No command for given shortcut: " << shortcutName << std::endl;
        }
    } else {
        std::cout << "No shortcuts have been created." << std::endl;
    }
}

void multiScriptShortcutProcesser(const std::string& command){
    if (!shotcutsEnabled) {
        std::cout << "Shortcuts are disabled." << std::endl;
        return;
    }
    if (!multiScriptShortcuts.empty()) {
        std::string strippedCommand = command.substr(2);
        trim(strippedCommand);
        if (strippedCommand.empty()) {
            std::cout << "No shortcut given." << std::endl;
            return;
        }
        if (multiScriptShortcuts.find(strippedCommand) != multiScriptShortcuts.end()) {
            std::istringstream iss(strippedCommand);
            std::string shortcutName;
            iss >> shortcutName;
            std::vector<std::string> dashArgs;
            std::string remainingArgs;
            std::string arg;
            while (iss >> arg) {
                if (arg.front() == '-') {
                    dashArgs.push_back(arg);
                } else {
                    remainingArgs += arg + " ";
                }
            }
            trim(remainingArgs);
            for(const auto& baseCommand : multiScriptShortcuts[shortcutName]) {
                std::string fullCommand = baseCommand;
                if (!remainingArgs.empty()) {
                    fullCommand += remainingArgs;
                }
                commandParser(fullCommand);
                for (const auto& dashArg : dashArgs) {
                    std::string processedArg = dashArg;
                    processedArg.erase(0, 1);
                    commandParser(processedArg);
                }
            }
        } else {
            std::cout << "No command for given shortcut: " << strippedCommand << std::endl;
        }
    } else {
        std::cout << "No smulti-script shortcuts have been created." << std::endl;
    }
}

void commandProcesser(const std::string& command) {
    commandsQueue = std::queue<std::string>();
    auto commands = commandSplicer(command);
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
        std::cout << "Unknown command. Please try again." << std::endl;
    }
    getNextCommand();
    if (lastCommandParsed == "approot") {
        goToApplicationDirectory();
    } else if (lastCommandParsed == "clear") {
        sendTerminalCommand("clear");
    } else if(lastCommandParsed == "ss"){
        shortcutProcesser(command);
    } else if(lastCommandParsed == "mm"){
        multiScriptShortcutProcesser(command);
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
        std::cout << "DevToolsTerminal v" + currentVersion << std::endl;
    } else if (lastCommandParsed == "terminal") {
        try {
            std::string terminalCommand = command.substr(9);
            sendTerminalCommand(terminalCommand);
        } catch (std::out_of_range& e) {
            defaultTextEntryOnAI = false;
            return;
        }
    } else if(lastCommandParsed == "plugin") {
        pluginCommands();
    } else if (lastCommandParsed == "theme") {
        themeCommands();
    } else if (lastCommandParsed == "env") {
        envVarCommands();
    } else if (lastCommandParsed == "help") {
        std::cout << "Commands:" << std::endl;
        std::cout << " commandprefix: Change the command prefix (" << commandPrefix << ")" << std::endl;
        std::cout << " ai: Access AI command settings and chat" << std::endl;
        std::cout << " approot: Switch to the application directory" << std::endl;
        std::cout << " terminal [ARGS]: Run terminal commands" << std::endl;
        std::cout << " user: Access user settings" << std::endl;
        std::cout << " ss: Execute single script shortcut" << std::endl;
        std::cout << " mm: Execute multi-script shortcut" << std::endl;
        std::cout << " aihelp: Get AI troubleshooting help" << std::endl;
        std::cout << " theme: Manage themes (load/save)" << std::endl;
        std::cout << " version: Display application version" << std::endl;
        std::cout << " plugin: Manage plugins" << std::endl;
        std::cout << " env: Manage environment variables" << std::endl;
        std::cout << " uninstall: Uninstall the application" << std::endl;
        return;
    } else if (lastCommandParsed == "uninstall") {
        std::cout << "Are you sure you want to uninstall DevToolsTerminal? (y/n): ";
        char confirmation;
        std::cin >> confirmation;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        if (confirmation == 'y' || confirmation == 'Y') {
            // First check if the script exists in the expected location
            std::string uninstallScriptPath = applicationDirectory + "/tool-scripts/uninstall.sh";
            bool scriptExists = std::filesystem::exists(uninstallScriptPath);
            
            // If not found, generate it
            if (!scriptExists) {
                std::cout << "Uninstall script not found. Generating one..." << std::endl;
                uninstallScriptPath = generateUninstallScript();
            }
            
            std::string uninstallCommand = "bash \"" + uninstallScriptPath + "\"";
            std::cout << "Running uninstall script..." << std::endl;
            system(uninstallCommand.c_str());
            exitFlag = true;
        } else {
            std::cout << "Uninstall cancelled." << std::endl;
        }
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

void pluginCommands(){
    getNextCommand();
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
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
        if(lastCommandParsed == "commands") {
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
                std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
                return;
            }
            if(lastCommandParsed == "set") {
                getNextCommand();
                if(lastCommandParsed.empty()) {
                    std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
                    return;
                }
                std::string settingToModify = lastCommandParsed;
                getNextCommand();
                if(lastCommandParsed.empty()) {
                    std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    } else {
        std::vector<std::string> availablePlugins = pluginManager->getAvailablePlugins();
        if(std::find(availablePlugins.begin(), availablePlugins.end(), pluginToModify) != availablePlugins.end()){
            getNextCommand();
            if(lastCommandParsed == "enable") {
                pluginManager->enablePlugin(pluginToModify);
                return;
            }
            std::cout << "Plugin: "<< pluginToModify << " is disabled." << std::endl;
            return;
        } else {
            std::cout << "Plugin " << pluginToModify << " does not exist." << std::endl;
            return;
        }
    }
}

void sendTerminalCommand(const std::string& command) {
    if (TESTING) {
        std::cout << "Sending Command: " << command << std::endl;
    }
    std::thread commandThread = terminal.executeCommand(command);
    if (commandThread.joinable()) {
        commandThread.join();
    }
}

void userSettingsCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
    if (lastCommandParsed == "help") {
        std::cout << "User settings commands:" << std::endl;
        std::cout << " startup: Manage startup commands (add, remove, clear, enable, disable, list, runall)" << std::endl;
        std::cout << " text: Configure text settings (commandprefix, displayfullpath, defaultentry)" << std::endl;
        std::cout << " shortcut: Manage shortcuts (add, remove, clear, list, mm)" << std::endl;
        std::cout << " testing: Toggle testing mode (enable/disable)" << std::endl;
        std::cout << " data: Manage user data (get userdata/userhistory/all, clear)" << std::endl;
        std::cout << " saveloop: Toggle auto-save loop (enable/disable)" << std::endl;
        std::cout << " saveonexit: Toggle save on exit (enable/disable)" << std::endl;
        std::cout << " checkforupdates: Toggle update checking (enable/disable)" << std::endl;
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
            std::cout << "No startup commands." << std::endl;
        }
        return;
    }
    if (lastCommandParsed == "add") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        startupCommands.push_back(lastCommandParsed);
        std::cout << "Command added to startup commands." << std::endl;
        return;
    }
    if (lastCommandParsed == "remove") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
            std::cout << "Command not found in startup commands." << std::endl;
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
            std::cout << "No startup commands." << std::endl;
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
            std::cout << "No startup commands." << std::endl;
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
    std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
}

void shortcutCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        if (!shortcuts.empty()) {
            std::cout << "Shortcuts:" << std::endl;
            for (const auto& [key, value] : shortcuts) {
                std::cout << key + " = " + value << std::endl;
            }
        } else {
            std::cout << "No shortcuts." << std::endl;
        }
        if(!multiScriptShortcuts.empty()){
            std::cout << "Multi-Script Shortcuts:" << std::endl;
            for (const auto& [key, value] : multiScriptShortcuts) {
                std::cout << key + " = ";
                for(const auto& command : value){
                    std::cout << "'"+command + "' ";
                }
                std::cout << std::endl;
            }
        } else {
            std::cout << "No multi-script shortcuts." << std::endl;
        }
        return;
    }
    if (lastCommandParsed == "clear") {
        shortcuts.clear();
        std::cout << "Shortcuts cleared." << std::endl;
        return;
    }
    if (lastCommandParsed == "enable") {
        shotcutsEnabled = true;
        std::cout << "Shortcuts enabled." << std::endl;
        return;
    }
    if (lastCommandParsed == "disable") {
        shotcutsEnabled = false;
        std::cout << "Shortcuts disabled." << std::endl;
        return;
    }
    if(lastCommandParsed == "mm"){
        multiScriptShortcutCommands();
        return;
    }
    if (lastCommandParsed == "add") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        std::string shortcut = lastCommandParsed;
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        std::string command = lastCommandParsed;
        shortcuts[shortcut] = command;
        std::cout << "Shortcut added." << std::endl;
        return;
    }
    if (lastCommandParsed == "remove") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        if(shortcuts.find(lastCommandParsed) == shortcuts.end()){
            std::cout << "Shortcut not found." << std::endl;
            return;
        }
        shortcuts.erase(lastCommandParsed);
        std::cout << "Shortcut removed." << std::endl;
        return;
    }
    if (lastCommandParsed == "list") {
        if (!shortcuts.empty()) {
            std::cout << "Shortcuts:" << std::endl;
            for (const auto& [key, value] : shortcuts) {
                std::cout << key + " = " + value << std::endl;
            }
        } else {
            std::cout << "No shortcuts." << std::endl;
        }
        if(!multiScriptShortcuts.empty()){
            std::cout << "Multi-Script Shortcuts:" << std::endl;
            for (const auto& [key, value] : multiScriptShortcuts) {
                std::cout << key + " = ";
                for(const auto& command : value){
                    std::cout << "'"+command + "' ";
                }
                std::cout << std::endl;
            }
        } else {
            std::cout << "No multi-script shortcuts." << std::endl;
        }
        return;
    }
    if (lastCommandParsed == "help") {
        std::cout << "Shortcut commands:" << std::endl;
        std::cout << " clear: Remove all shortcuts" << std::endl;
        std::cout << " enable: Enable shortcuts" << std::endl;
        std::cout << " disable: Disable shortcuts" << std::endl;
        std::cout << " mm: Access multi-script shortcut commands" << std::endl;
        std::cout << " add [NAME] [CMD]: Add a new shortcut" << std::endl;
        std::cout << " remove [NAME]: Remove a shortcut" << std::endl;
        std::cout << " list: List all shortcuts" << std::endl;
        return;
    }
    std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
}

void multiScriptShortcutCommands(){
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    }
    if(lastCommandParsed == "add"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        multiScriptShortcuts[shortcut] = commands;
        std::cout << "Multi-Script Shortcut added." << std::endl;
        return;
    }
    if(lastCommandParsed == "remove"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        if(multiScriptShortcuts.find(lastCommandParsed) == multiScriptShortcuts.end()){
            std::cout << "Multi-Script Shortcut not found." << std::endl;
            return;
        }
        multiScriptShortcuts.erase(lastCommandParsed);
        std::cout << "Multi-Script Shortcut removed." << std::endl;
        return;
    }
    if (lastCommandParsed == "help") {
        std::cout << "Multi-script shortcut commands:" << std::endl;
        std::cout << " add [NAME] [CMD1] [CMD2] ... : Add a multi-script shortcut" << std::endl;
        std::cout << " remove [NAME]: Remove a multi-script shortcut" << std::endl;
        return;
    }
    std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
}

void textCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    }
    if (lastCommandParsed == "commandprefix") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Command prefix is currently " + commandPrefix << std::endl;
            return;
        }
        if (lastCommandParsed.length() > 1) {
            std::cout << "Invalid command prefix. Must be a single character." << std::endl;
            return;
        } else if (lastCommandParsed == " ") {
            std::cout << "Invalid command prefix. Must not be a space." << std::endl;
            return;
        }
        commandPrefix = lastCommandParsed;
        std::cout << "Command prefix set to " + commandPrefix << std::endl;
        return;
    }
    if(lastCommandParsed == "displayfullpath"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Display whole path is currently " << (terminal.isDisplayWholePath() ? "enabled." : "disabled.") << std::endl;
            return;
        }
        if(lastCommandParsed == "enable"){
            terminal.setDisplayWholePath(true);
            std::cout << "Display whole path enabled." << std::endl;
            return;
        }
        if(lastCommandParsed == "disable"){
            terminal.setDisplayWholePath(false);
            std::cout << "Display whole path disabled." << std::endl;
            return;
        }
    }
    if(lastCommandParsed == "defaultentry"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Default text entry is currently " << (defaultTextEntryOnAI ? "AI." : "terminal.") << std::endl;
            return;
        }
        if(lastCommandParsed == "ai"){
            defaultTextEntryOnAI = true;
            std::cout << "Default text entry set to AI." << std::endl;
            return;
        }
        if(lastCommandParsed == "terminal"){
            defaultTextEntryOnAI = false;
            std::cout << "Default text entry set to terminal." << std::endl;
            return;
        }
    }
    if (lastCommandParsed == "help") {
        std::cout << "Text commands:" << std::endl;
        std::cout << " commandprefix [CHAR]: Set the command prefix" << std::endl;
        std::cout << " displayfullpath enable/disable: Toggle full path display" << std::endl;
        std::cout << " defaultentry ai/terminal: Set default text entry mode" << std::endl;
        return;
    }
    std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
        std::vector<std::string> filesAtPath = terminal.getFilesAtCurrentPath();
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
            c_assistant.setSaveDirectory(".DTT-Data");
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

bool checkForUpdate() {
    std::cout << "Checking for updates...";
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

    std::string command = "curl -s " + updateURL;
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

bool downloadLatestRelease(){
    std::string releaseJson;
    std::string curlCmd = "curl -s " + updateURL;
    FILE* pipe = popen(curlCmd.c_str(), "r");
    if(!pipe){
        std::cerr << "Error: Unable to fetch release information." << std::endl;
        return false;
    }
    char buffer[128];
    while(fgets(buffer, 128, pipe) != nullptr){
        releaseJson += buffer;
    }
    pclose(pipe);
    if(releaseJson.empty()){
        std::cerr << "Error: Empty response when fetching release info." << std::endl;
        return false;
    }
    try {
        json releaseData = json::parse(releaseJson);
        if(!releaseData.contains("assets") || !releaseData["assets"].is_array() || releaseData["assets"].empty()){
            std::cerr << "Error: No assets found in the latest release." << std::endl;
            return false;
        }
        std::string downloadUrl;
        std::string changeLog;
        if(releaseData.contains("body")) {
            changeLog = releaseData["body"].get<std::string>();
        }
        #ifdef _WIN32
        for (const auto& asset : releaseData["assets"]) {
            if (asset["browser_download_url"].get<std::string>().find(".exe") != std::string::npos) {
                downloadUrl = asset["browser_download_url"].get<std::string>();
                break;
            }
        }
        #else
        downloadUrl = releaseData["assets"][0]["browser_download_url"].get<std::string>();
        #endif
        size_t pos = downloadUrl.find_last_of('/');
        std::string filename = (pos != std::string::npos) ? downloadUrl.substr(pos+1) : "latest_release";
        std::string downloadPath = (std::filesystem::current_path() / filename).string();
        std::string downloadCmd = "curl -L -o " + downloadPath + " " + downloadUrl;
        int ret = system(downloadCmd.c_str());
        if(ret == 0){
            std::string chmodCmd = "chmod +x " + downloadPath;
            system(chmodCmd.c_str());
            std::cout << "Downloaded latest release asset to " << downloadPath << std::endl;
            std::cout << "Replacing current running program with updated version..." << std::endl;
            std::string exePath = (std::filesystem::current_path() / "DevToolsTerminal").string();
            #ifdef _WIN32
            exePath += ".exe";
            #endif
            if(std::rename(downloadPath.c_str(), exePath.c_str()) != 0){
                std::cerr << "Error: Failed to replace the current executable." << std::endl;
                return false;
            }
            std::ofstream changelogFile(DATA_DIRECTORY / "CHANGELOG.txt");
            if (changelogFile.is_open()) {
                changelogFile << changeLog;
                changelogFile.close();
            }
            execl(exePath.c_str(), exePath.c_str(), (char*)NULL);
            std::cerr << "Error: Failed to execute the updated program." << std::endl;
            return false;
        } else {
            std::cerr << "Error: Download command failed." << std::endl;
            return false;
        }
    } catch(std::exception &e) {
        std::cerr << "Error parsing release JSON: " << e.what() << std::endl;
        return false;
    }
}

void displayChangeLog(const std::string& changeLog) {
    std::string clickableChangeLog = changeLog;
    size_t pos = 0;
    while ((pos = clickableChangeLog.find("http", pos)) != std::string::npos) {
        size_t end = clickableChangeLog.find_first_of(" \n", pos);
        if (end == std::string::npos) end = clickableChangeLog.length();
        std::string url = clickableChangeLog.substr(pos, end - pos);
        std::string clickableUrl = "\033]8;;" + url + "\033\\" + url + "\033]8;;\033\\";
        clickableChangeLog.replace(pos, url.length(), clickableUrl);
        pos += clickableUrl.length();
    }
    std::cout << "Change Log:" << std::endl;
    std::cout << clickableChangeLog << std::endl;
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

void envVarCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::map<std::string, std::string> allVars = terminal.getAllEnvVars();
        if (allVars.empty()) {
            std::cout << "No environment variables defined." << std::endl;
        } else {
            std::cout << "Environment Variables:" << std::endl;
            for (const auto& [name, value] : allVars) {
                std::cout << name << "=" << value << std::endl;
            }
        }
        return;
    }
    
    if (lastCommandParsed == "set") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Error: No variable name provided." << std::endl;
            return;
        }
        std::string varName = lastCommandParsed;
        
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Error: No value provided for variable " << varName << "." << std::endl;
            return;
        }
        std::string varValue = lastCommandParsed;
        
        terminal.setEnvVar(varName, varValue);
        std::cout << "Environment variable set: " << varName << "=" << varValue << std::endl;
        return;
    }
    
    if (lastCommandParsed == "get") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Error: No variable name provided." << std::endl;
            return;
        }
        std::string varName = lastCommandParsed;
        
        if (terminal.hasEnvVar(varName)) {
            std::cout << varName << "=" << terminal.getEnvVar(varName) << std::endl;
        } else {
            std::cout << "Environment variable not defined: " << varName << std::endl;
        }
        return;
    }
    
    if (lastCommandParsed == "remove" || lastCommandParsed == "unset") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Error: No variable name provided." << std::endl;
            return;
        }
        std::string varName = lastCommandParsed;
        
        if (terminal.hasEnvVar(varName)) {
            terminal.removeEnvVar(varName);
            std::cout << "Environment variable removed: " << varName << std::endl;
        } else {
            std::cout << "Environment variable not defined: " << varName << std::endl;
        }
        return;
    }
    
    if (lastCommandParsed == "clear") {
        std::map<std::string, std::string> allVars = terminal.getAllEnvVars();
        for (const auto& [name, _] : allVars) {
            terminal.removeEnvVar(name);
        }
        std::cout << "All environment variables cleared." << std::endl;
        return;
    }
    
    if (lastCommandParsed == "expand") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cerr << "Error: No string provided for expansion." << std::endl;
            return;
        }
        
        std::string toExpand = lastCommandParsed;
        while (!commandsQueue.empty()) {
            toExpand += " " + commandsQueue.front();
            commandsQueue.pop();
        }
        
        std::cout << "Original: " << toExpand << std::endl;
        std::cout << "Expanded: " << terminal.expandEnvVars(toExpand) << std::endl;
        return;
    }
    
    if (lastCommandParsed == "help") {
        std::cout << "Environment variable commands:" << std::endl;
        std::cout << " env: List all environment variables" << std::endl;
        std::cout << " env set NAME VALUE: Set an environment variable" << std::endl;
        std::cout << " env get NAME: Get the value of an environment variable" << std::endl;
        std::cout << " env remove|unset NAME: Remove an environment variable" << std::endl;
        std::cout << " env clear: Remove all environment variables" << std::endl;
        std::cout << " env expand STRING: Show expansion of variables in a string" << std::endl;
        return;
    }
    
    std::cerr << "Unknown command. Try 'env help' for a list of commands." << std::endl;
}

// Add this function to generate the uninstall script on demand
std::string generateUninstallScript() {
    std::filesystem::path uninstallPath = DATA_DIRECTORY / "dtt-uninstall.sh";
    std::ofstream uninstallScript(uninstallPath);
    
    if (uninstallScript.is_open()) {
        uninstallScript << "#!/bin/bash\n\n";
        uninstallScript << "# Installation locations to check\n";
        uninstallScript << "HOME_DIR=\"$HOME\"\n";
        uninstallScript << "DATA_DIR=\"$HOME_DIR/.DTT-Data\"\n";
        uninstallScript << "APP_NAME=\"DevToolsTerminal\"\n";
        uninstallScript << "APP_PATH=\"$DATA_DIR/$APP_NAME\"\n";
        uninstallScript << "ZSHRC_PATH=\"$HOME/.zshrc\"\n";
        uninstallScript << "UNINSTALL_SCRIPT=\"$DATA_DIR/uninstall-$APP_NAME.sh\"\n\n";
        
        uninstallScript << "# Legacy installation locations to check for backward compatibility\n";
        uninstallScript << "SYSTEM_INSTALL_DIR=\"/usr/local/bin\"\n";
        uninstallScript << "USER_INSTALL_DIR=\"$HOME/.local/bin\"\n";
        uninstallScript << "SYSTEM_APP_PATH=\"$SYSTEM_INSTALL_DIR/$APP_NAME\"\n";
        uninstallScript << "USER_APP_PATH=\"$USER_INSTALL_DIR/$APP_NAME\"\n";
        uninstallScript << "LEGACY_UNINSTALL_SCRIPT=\"$SYSTEM_INSTALL_DIR/uninstall-$APP_NAME.sh\"\n";
        uninstallScript << "LEGACY_USER_UNINSTALL_SCRIPT=\"$USER_INSTALL_DIR/uninstall-$APP_NAME.sh\"\n\n";
        
        uninstallScript << "echo \"DevToolsTerminal Uninstaller\"\n";
        uninstallScript << "echo \"---------------------------\"\n";
        uninstallScript << "echo \"This will uninstall DevToolsTerminal and remove auto-launch from zsh.\"\n\n";
        
        uninstallScript << "# Function to remove auto-launch entries from .zshrc\n";
        uninstallScript << "remove_from_zshrc() {\n";
        uninstallScript << "    if [ -f \"$ZSHRC_PATH\" ]; then\n";
        uninstallScript << "        echo \"Removing auto-launch configuration from .zshrc...\"\n";
        uninstallScript << "        # Create a temporary file\n";
        uninstallScript << "        TEMP_FILE=$(mktemp)\n";
        uninstallScript << "        \n";
        uninstallScript << "        # Filter out the DevToolsTerminal auto-launch block\n";
        uninstallScript << "        sed '/# DevToolsTerminal Auto-Launch/,+3d' \"$ZSHRC_PATH\" > \"$TEMP_FILE\"\n";
        uninstallScript << "        \n";
        uninstallScript << "        # Also remove any PATH additions for legacy user installation\n";
        uninstallScript << "        if [ -d \"$USER_INSTALL_DIR\" ]; then\n";
        uninstallScript << "            sed \"/export PATH=\\\"\\$PATH:$USER_INSTALL_DIR\\\"/d\" \"$TEMP_FILE\" > \"${TEMP_FILE}.2\"\n";
        uninstallScript << "            mv \"${TEMP_FILE}.2\" \"$TEMP_FILE\"\n";
        uninstallScript << "        fi\n";
        uninstallScript << "        \n";
        uninstallScript << "        # Replace the original file\n";
        uninstallScript << "        cat \"$TEMP_FILE\" > \"$ZSHRC_PATH\"\n";
        uninstallScript << "        rm \"$TEMP_FILE\"\n";
        uninstallScript << "        \n";
        uninstallScript << "        echo \"Auto-launch configuration removed from .zshrc.\"\n";
        uninstallScript << "    else\n";
        uninstallScript << "        echo \"No .zshrc file found.\"\n";
        uninstallScript << "    fi\n";
        uninstallScript << "}\n\n";
        
        uninstallScript << "# Try to find the installed application\n";
        uninstallScript << "APP_FOUND=false\n\n";
        
        uninstallScript << "# First check the new location in .DTT-Data\n";
        uninstallScript << "if [ -f \"$APP_PATH\" ]; then\n";
        uninstallScript << "    echo \"Found DevToolsTerminal at $APP_PATH\"\n";
        uninstallScript << "    APP_FOUND=true\n";
        uninstallScript << "    \n";
        uninstallScript << "    echo \"Removing executable from $APP_PATH...\"\n";
        uninstallScript << "    rm \"$APP_PATH\"\n";
        uninstallScript << "    \n";
        uninstallScript << "    # Also remove the uninstall script from the data directory if it exists\n";
        uninstallScript << "    if [ -f \"$UNINSTALL_SCRIPT\" ] && [ \"$UNINSTALL_SCRIPT\" != \"$0\" ]; then\n";
        uninstallScript << "        rm \"$UNINSTALL_SCRIPT\"\n";
        uninstallScript << "    fi\n";
        uninstallScript << "fi\n\n";
        
        uninstallScript << "# Check for legacy installations and remove them if found\n";
        uninstallScript << "if [ -f \"$SYSTEM_APP_PATH\" ]; then\n";
        uninstallScript << "    echo \"Found legacy installation at $SYSTEM_APP_PATH\"\n";
        uninstallScript << "    APP_FOUND=true\n";
        uninstallScript << "    \n";
        uninstallScript << "    # Check if we have permission to remove it\n";
        uninstallScript << "    if [ -w \"$SYSTEM_INSTALL_DIR\" ]; then\n";
        uninstallScript << "        echo \"Removing legacy installation from $SYSTEM_APP_PATH...\"\n";
        uninstallScript << "        rm \"$SYSTEM_APP_PATH\"\n";
        uninstallScript << "        # Also remove the uninstall script if it exists\n";
        uninstallScript << "        if [ -f \"$LEGACY_UNINSTALL_SCRIPT\" ]; then\n";
        uninstallScript << "            rm \"$LEGACY_UNINSTALL_SCRIPT\"\n";
        uninstallScript << "        fi\n";
        uninstallScript << "    elif sudo -n true 2>/dev/null; then\n";
        uninstallScript << "        echo \"Removing legacy installation with sudo...\"\n";
        uninstallScript << "        sudo rm \"$SYSTEM_APP_PATH\"\n";
        uninstallScript << "        # Also remove the uninstall script if it exists\n";
        uninstallScript << "        if [ -f \"$LEGACY_UNINSTALL_SCRIPT\" ]; then\n";
        uninstallScript << "            sudo rm \"$LEGACY_UNINSTALL_SCRIPT\"\n";
        uninstallScript << "        fi\n";
        uninstallScript << "    else\n";
        uninstallScript << "        echo \"Warning: You need root privileges to remove legacy installation at $SYSTEM_APP_PATH\"\n";
        uninstallScript << "        echo \"Please run: sudo rm $SYSTEM_APP_PATH\"\n";
        uninstallScript << "        if [ -f \"$LEGACY_UNINSTALL_SCRIPT\" ]; then\n";
        uninstallScript << "            echo \"And also run: sudo rm $LEGACY_UNINSTALL_SCRIPT\"\n";
        uninstallScript << "        fi\n";
        uninstallScript << "    fi\n";
        uninstallScript << "fi\n\n";
        
        uninstallScript << "if [ -f \"$USER_APP_PATH\" ]; then\n";
        uninstallScript << "    echo \"Found legacy user installation at $USER_APP_PATH\"\n";
        uninstallScript << "    APP_FOUND=true\n";
        uninstallScript << "    \n";
        uninstallScript << "    echo \"Removing legacy user installation...\"\n";
        uninstallScript << "    rm \"$USER_APP_PATH\"\n";
        uninstallScript << "    \n";
        uninstallScript << "    # Also remove the user uninstall script if it exists\n";
        uninstallScript << "    if [ -f \"$LEGACY_USER_UNINSTALL_SCRIPT\" ]; then\n";
        uninstallScript << "        rm \"$LEGACY_USER_UNINSTALL_SCRIPT\"\n";
        uninstallScript << "    fi\n";
        uninstallScript << "    \n";
        uninstallScript << "    # Clean up the directory if it's empty\n";
        uninstallScript << "    if [ -d \"$USER_INSTALL_DIR\" ] && [ -z \"$(ls -A \"$USER_INSTALL_DIR\")\" ]; then\n";
        uninstallScript << "        echo \"Removing empty directory $USER_INSTALL_DIR...\"\n";
        uninstallScript << "        rmdir \"$USER_INSTALL_DIR\"\n";
        uninstallScript << "    fi\n";
        uninstallScript << "fi\n\n";
        
        uninstallScript << "if [ \"$APP_FOUND\" = false ]; then\n";
        uninstallScript << "    echo \"Error: DevToolsTerminal installation not found.\"\n";
        uninstallScript << "    echo \"Checked locations:\"\n";
        uninstallScript << "    echo \"  - $APP_PATH (current)\"\n";
        uninstallScript << "    echo \"  - $SYSTEM_APP_PATH (legacy)\"\n";
        uninstallScript << "    echo \"  - $USER_APP_PATH (legacy)\"\n";
        uninstallScript << "else\n";
        uninstallScript << "    # Remove from .zshrc regardless of which installation was found\n";
        uninstallScript << "    remove_from_zshrc\n";
        uninstallScript << "    \n";
        uninstallScript << "    echo \"Uninstallation complete!\"\n";
        uninstallScript << "    echo \"Note: Your personal data in $DATA_DIR has not been removed.\"\n";
        uninstallScript << "    read -p \"Would you like to remove all data? (y/n): \" remove_data\n";
        uninstallScript << "    if [[ \"$remove_data\" =~ ^[Yy]$ ]]; then\n";
        uninstallScript << "        echo \"Removing all data from $DATA_DIR...\"\n";
        uninstallScript << "        rm -rf \"$DATA_DIR\"\n";
        uninstallScript << "        echo \"All data removed.\"\n";
        uninstallScript << "    else\n";
        uninstallScript << "        echo \"Data directory preserved. To manually remove it later, run: rm -rf $DATA_DIR\"\n";
        uninstallScript << "    fi\n";
        uninstallScript << "fi\n\n";
        
        uninstallScript << "# Self-delete if this script was executed directly\n";
        uninstallScript << "SCRIPT_PATH=$(realpath \"$0\")\n";
        uninstallScript << "if [[ \"$SCRIPT_PATH\" != \"$UNINSTALL_SCRIPT\" && \"$SCRIPT_PATH\" != \"$LEGACY_UNINSTALL_SCRIPT\" && \"$SCRIPT_PATH\" != \"$LEGACY_USER_UNINSTALL_SCRIPT\" ]]; then\n";
        uninstallScript << "    rm \"$SCRIPT_PATH\"\n";
        uninstallScript << "fi\n";
        
        uninstallScript.close();
        
        // Make the script executable
        std::string chmodCmd = "chmod +x " + uninstallPath.string();
        system(chmodCmd.c_str());
        
        std::cout << "Generated uninstall script at " << uninstallPath.string() << std::endl;
    } else {
        std::cerr << "Error: Could not create uninstall script." << std::endl;
    }
    
    return uninstallPath.string();
}