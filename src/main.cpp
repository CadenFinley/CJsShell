#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <fstream>
#include <map>
#include <queue>
#include <algorithm>
#include <cctype>
#include <locale>
#include "terminalpassthrough.h"
#include <nlohmann/json.hpp> // Include nlohmann/json library

using json = nlohmann::json;

std::filesystem::path USER_DATA = ".USER_DATA.json";
std::filesystem::path USER_COMMAND_HISTORY = ".USER_COMMAND_HISTORY.txt";

bool TESTING = false;
bool shotcutsEnabled = true;
bool startCommandsOn = true;
bool runningStartup = false;

std::string commandPrefix = "!";
std::string lastCommandParsed;
std::string applicationDirectory;
const std::string GREEN_COLOR_BOLD = "\033[1;32m";
const std::string RESET_COLOR = "\033[0m";
const std::string RED_COLOR_BOLD = "\033[1;31m";
const std::string PURPLE_COLOR_BOLD = "\033[1;35m";

std::queue<std::string> commandsQueue;
std::vector<std::string> startupCommands;
std::map<std::string, std::string> shortcuts;
bool textBuffer; // Declare textBuffer
bool defaultTextEntryOnAI; // Declare defaultTextEntryOnAI

TerminalPassthrough terminal;

/**
 * @brief Main process loop that continuously reads and processes user commands.
 */
void mainProcessLoop();
void createNewUSER_DATAFile();
void createNewUSER_HISTORYfile();
void loadUserData();
void writeUserData();
void goToApplicationDirectory();
std::string readAndReturnUserDataFile();
std::vector<std::string> commandSplicer(const std::string& command);
void commandParser(const std::string& command);
void addUserInputToHistory(const std::string& input);
void shortcutProcesser(const std::string& command);
void commandProcesser(const std::string& command);
void sendTerminalCommand(const std::string& command);
void userSettingsCommands();
void startupCommandsHandler(); // Rename to avoid conflict
void shortcutCommands();
void textCommands();
void getNextCommand();
void exit();



int main() {
    std::cout << "DevToolsTerminal LITE - Caden Finley (c) 2025" << std::endl;
    std::cout << "Created 2025 @ " << PURPLE_COLOR_BOLD << "Abilene Chrsitian University" << RESET_COLOR << std::endl;
    std::cout << "Loading..." << std::endl;
    applicationDirectory = std::filesystem::current_path().string();
    if (applicationDirectory.find(":") != std::string::npos) {
        applicationDirectory = applicationDirectory.substr(applicationDirectory.find(":") + 1);
    }
    startupCommands = {};
    shortcuts = {};
    terminal = TerminalPassthrough();
    if (!std::filesystem::exists(USER_DATA)) {
        createNewUSER_DATAFile();
    } else {
        loadUserData();
    }
    if (!std::filesystem::exists(USER_COMMAND_HISTORY)) {
        createNewUSER_HISTORYfile();
    }
    if (!startupCommands.empty() && startCommandsOn) {
        runningStartup = true;
        std::cout << "Running startup commands..." << std::endl;
        for (const auto& command : startupCommands) {
            commandParser(commandPrefix + command);
        }
        runningStartup = false;
    }
    mainProcessLoop();
    return 0;
}

/**
 * @brief Main process loop that continuously reads and processes user commands.
 */
void mainProcessLoop() {
    while (true) {
        if (TESTING) {
            std::cout << RED_COLOR_BOLD << "DEV MODE" << RESET_COLOR << std::endl;
        }
        std::cout << terminal.returnCurrentTerminalPosition();
        std::string command;
        std::getline(std::cin, command);
        commandParser(command);
    }
}

/**
 * @brief Create a new user data file with default settings.
 */
void createNewUSER_DATAFile() {
    std::cout << "User data file not found. Creating new file..." << std::endl;
    std::ofstream file(USER_DATA);
    if (file.is_open()) {
        startupCommands.push_back("terminal cd /");
        writeUserData();
        file.close();
    } else {
        std::cout << "An error occurred while creating the user data file." << std::endl;
    }
}

/**
 * @brief Create a new user command history file.
 */
void createNewUSER_HISTORYfile() {
    std::cout << "User history file not found. Creating new file..." << std::endl;
    std::ofstream file(USER_COMMAND_HISTORY);
    if (!file.is_open()) {
        std::cout << "An error occurred while creating the user history file." << std::endl;
    }
}

/**
 * @brief Load user data from the user data file.
 */
void loadUserData() {
    std::ifstream file(USER_DATA);
    if (file.is_open()) {
        json userData;
        file >> userData;
        startupCommands = userData["Startup_Commands"].get<std::vector<std::string>>();
        shotcutsEnabled = userData["Shortcuts_Enabled"].get<bool>();
        shortcuts = userData["Shortcuts"].get<std::map<std::string, std::string>>();
        textBuffer = userData["Text_Buffer"].get<bool>();
        commandPrefix = userData["Command_Prefix"].get<std::string>();
        file.close();
    } else {
        std::cout << "An error occurred while reading the user data file." << std::endl;
    }
}

/**
 * @brief Write user data to the user data file.
 */
void writeUserData() {
    std::ofstream file(USER_DATA);
    if (file.is_open()) {
        json userData;
        userData["Startup_Commands"] = startupCommands;
        userData["Shortcuts_Enabled"] = shotcutsEnabled;
        userData["Shortcuts"] = shortcuts;
        userData["Text_Buffer"] = false;
        userData["Text_Entry"] = "terminal";
        userData["Command_Prefix"] = commandPrefix;
        file << userData.dump(4);
        file.close();
    } else {
        std::cout << "An error occurred while writing to the user data file." << std::endl;
    }
}

/**
 * @brief Change the current directory to the application directory.
 */
void goToApplicationDirectory() {
    commandProcesser("terminal cd /");
    commandProcesser("terminal cd " + applicationDirectory);
}

/**
 * @brief Read and return the contents of the user data file.
 * @return Contents of the user data file as a string.
 */
std::string readAndReturnUserDataFile() {
    std::ifstream file(USER_DATA);
    if (file.is_open()) {
        std::string userData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        return userData.empty() ? "No data found." : userData;
    } else {
        std::cout << "An error occurred while reading the user data file." << std::endl;
        return "";
    }
}

/**
 * @brief Split a command string into individual commands.
 * @param command Command string to split.
 * @return Vector of command strings.
 */
std::vector<std::string> commandSplicer(const std::string& command) {
    std::vector<std::string> commands;
    std::istringstream iss(command);
    std::string word;
    while (iss >> word) {
        commands.push_back(word);
    }
    return commands;
}

/**
 * @brief Parse and process a user command.
 * @param command Command string to parse.
 */
void commandParser(const std::string& command) {
    if (command.empty()) {
        std::cout << "Invalid input. Please try again." << std::endl;
        return;
    }
    if (!runningStartup) {
        addUserInputToHistory(command);
    }
    if (command.rfind(commandPrefix, 0) == 0) { // Use rfind to check prefix
        commandProcesser(command.substr(1));
        return;
    }
    sendTerminalCommand(command);
}

/**
 * @brief Add user input to the command history file.
 * @param input User input string to add.
 */
void addUserInputToHistory(const std::string& input) {
    std::ofstream file(USER_COMMAND_HISTORY, std::ios_base::app);
    if (file.is_open()) {
        file << "timestamp_placeholder" << " " << input << "\n"; // Placeholder for TimeEngine::timeStamp()
        file.close();
    } else {
        std::cout << "An error occurred while writing to the user input history file." << std::endl;
    }
}

// trim from start (in place)
inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
inline void trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}

/**
 * @brief Process a shortcut command.
 * @param command Shortcut command string to process.
 */
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
        if (shortcuts.find(strippedCommand) != shortcuts.end()) {
            commandProcesser(shortcuts[strippedCommand]);
        } else {
            std::cout << "No command for given shortcut: " << strippedCommand << std::endl;
        }
    } else {
        std::cout << "No shortcuts." << std::endl;
    }
}

/**
 * @brief Process a user command.
 * @param command Command string to process.
 */
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
    if (lastCommandParsed == "ss") {
        shortcutProcesser(command);
    } else if (lastCommandParsed == "approot") {
        goToApplicationDirectory();
    } else if (lastCommandParsed == "clear") {
        std::cout << "Clearing screen and terminal cache..." << std::endl;
        std::cout << "\033[2J\033[1;1H";
        terminal.clearTerminalCache();
    } else if (lastCommandParsed == "ai") {
        std::cout << "This build does not support AI." << std::endl;
    } else if (lastCommandParsed == "user") {
        userSettingsCommands();
    } else if (lastCommandParsed == "terminal") {
        std::string strippedCommand = command.substr(9);
        sendTerminalCommand(strippedCommand);
    } else if (lastCommandParsed == "exit") {
        exit();
    } else if (lastCommandParsed == "help") {
        std::cout << "Commands:" << std::endl;
        std::cout << "Command Prefix: " + commandPrefix << std::endl;
        std::cout << "ss [ARGS]" << std::endl;
        std::cout << "approot" << std::endl;
        std::cout << "terminal o[ARGS]" << std::endl;
        std::cout << "user" << std::endl;
        std::cout << "exit" << std::endl;
        std::cout << commandPrefix+"clear or clear" << std::endl;
        std::cout << "help" << std::endl;
    } else {
        std::cout << "Unknown command. Please try again. Type 'help' or '.help' if you need help" << std::endl;
    }
}

/**
 * @brief Send a command to the terminal for execution.
 * @param command Command string to send.
 */
void sendTerminalCommand(const std::string& command) {
    if (TESTING) {
        std::cout << "Sending Command: " << command << std::endl;
    }
    std::thread commandThread = terminal.executeCommand(command);
    commandThread.join();
    if (TESTING) {
        std::cout << "Command Thread Joined." << std::endl;
    }
}

/**
 * @brief Process user settings commands.
 */
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
            getNextCommand();
            if (lastCommandParsed.empty()) {
                return;
            }
        }
        if (lastCommandParsed == "disable") {
            TESTING = false;
            std::cout << "Testing mode disabled." << std::endl;
            getNextCommand();
            if (lastCommandParsed.empty()) {
                return;
            }
        }
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    }
    if (lastCommandParsed == "data") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        if (lastCommandParsed == "get") {
            getNextCommand();
            if (lastCommandParsed.empty()) {
                std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
                    std::cout << "An error occurred while reading the user history file." << std::endl;
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
                    std::cout << "An error occurred while reading the user history file." << std::endl;
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
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    }
    if (lastCommandParsed == "help") {
        std::cout << "Commands: " << std::endl;
        std::cout << "startup: add [ARGS], remove [ARGS], clear, enable, disable, list, runall" << std::endl;
        std::cout << "text: commandprefix [ARGS]" << std::endl;
        std::cout << "shortcut: clear, enable, disable, add [ARGS], remove [ARGS], list" << std::endl;
        std::cout << "testing [ARGS]" << std::endl;
        std::cout << "data: get [ARGS], clear" << std::endl;
        return;
    }
    std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
}

/**
 * @brief Handle startup commands.
 */
void startupCommandsHandler() { // Renamed function
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
        startupCommands.erase(std::remove(startupCommands.begin(), startupCommands.end(), lastCommandParsed), startupCommands.end());
        std::cout << "Command removed from startup commands." << std::endl;
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
    std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
}

/**
 * @brief Process shortcut commands.
 */
void shortcutCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
        return;
    }
    std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
}

/**
 * @brief Process text commands.
 */
void textCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    }
    if (lastCommandParsed == "commandprefix") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        if (lastCommandParsed.length() > 1 || lastCommandParsed.empty()) {
            std::cout << "Invalid command prefix. Must be a single character." << std::endl;
            return;
        }
        commandPrefix = lastCommandParsed;
        std::cout << "Command prefix set to " + commandPrefix << std::endl;
        return;
    }
    std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
}

/**
 * @brief Get the next command from the command queue.
 */
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

/**
 * @brief Exit the application, saving user data.
 */
void exit() {
    writeUserData();
    std::cout << "Exiting..." << std::endl;
    std::exit(0);
}