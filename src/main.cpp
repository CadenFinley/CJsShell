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
#include <termios.h>
#include <unistd.h>
#include "terminalpassthrough.h"
#include "nlohmann/json.hpp"
#include "openaipromptengine.h"
#include <sys/ioctl.h>

using json = nlohmann::json;

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

std::filesystem::path DATA_DIRECTORY = ".DTT-Data";
std::filesystem::path USER_DATA = DATA_DIRECTORY / ".USER_DATA.json";
std::filesystem::path USER_COMMAND_HISTORY = DATA_DIRECTORY / ".USER_COMMAND_HISTORY.txt";

std::queue<std::string> commandsQueue;
std::vector<std::string> startupCommands;
std::map<std::string, std::string> shortcuts;
std::map<std::string, std::vector<std::string>> multiScriptShortcuts;
bool textBuffer = false;
bool defaultTextEntryOnAI = false;
bool incognitoChatMode = false;
bool usingChatCache = true;
bool saveLoop = false;
bool rawEnabled = false;

std::vector<std::string> savedChatCache;

OpenAIPromptEngine openAIPromptEngine;
TerminalPassthrough terminal;

std::vector<std::string> commandLines;


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
void startupCommandsHandler();
void shortcutCommands();
void textCommands();
void getNextCommand();
void exit();
void aiSettingsCommands();
void aiChatCommands();
void chatProcess(const std::string& message);
void showChatHistory();
void extractCodeSnippet(const std::string& logFile, const std::string& fileName);
std::string getFileExtensionForLanguage(const std::string& language);
void multiScriptShortcutCommands();
void userDataCommands();
void setRawMode(bool enable);
void handleArrowKey(char arrow, size_t& cursorPositionX, size_t& cursorPositionY, std::vector<std::string>& commandLines, std::string& command, const std::string& terminalTag);
void placeCursor(size_t& cursorPositionX, size_t& cursorPositionY);
void reprintCommandLines(const std::vector<std::string>& commandLines, const std::string& terminalSetting);
void clearLines(const std::vector<std::string>& commandLines);

int main() {
    std::cout << "Loading..." << std::endl;

    startupCommands = {};
    shortcuts = {};
    multiScriptShortcuts = {};
    terminal = TerminalPassthrough();
    openAIPromptEngine = OpenAIPromptEngine("", "chat", "You are an AI personal assistant within a terminal application.");

    applicationDirectory = std::filesystem::current_path().string();
    if (applicationDirectory.find(":") != std::string::npos) {
        applicationDirectory = applicationDirectory.substr(applicationDirectory.find(":") + 1);
    }

    if (!std::filesystem::exists(DATA_DIRECTORY)) {
        std::cout << DATA_DIRECTORY.string() << " not found in: " << applicationDirectory << std::endl;
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

    if (!startupCommands.empty() && startCommandsOn) {
        runningStartup = true;
        std::cout << "Running startup commands..." << std::endl;
        for (const auto& command : startupCommands) {
            commandParser(commandPrefix + command);
        }
        runningStartup = false;
    }
    
    std::cout << "DevToolsTerminal LITE - Caden Finley (c) 2025" << std::endl;
    std::cout << "Created 2025 @ " << PURPLE_COLOR_BOLD << "Abilene Chrsitian University" << RESET_COLOR << std::endl;
    mainProcessLoop();
    return 0;
}

int getTerminalWidth(){
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
}

/**
 * @brief Main process loop that continuously reads and processes user commands.
 */
void mainProcessLoop() {
    std::string terminalSetting;
    setRawMode(true);
    while (true) {
        if (saveLoop) {
            writeUserData();
        }
        if (TESTING) {
            std::cout << RED_COLOR_BOLD << "DEV MODE " << rawEnabled << RESET_COLOR << std::endl;
        }
        if (defaultTextEntryOnAI) {
            terminalSetting = GREEN_COLOR_BOLD + "AI Menu: " + RESET_COLOR;
        } else {
            terminalSetting = terminal.returnCurrentTerminalPosition();
        }
        std::cout << terminalSetting;
        char c;
        size_t cursorPositionX = 0;
        size_t cursorPositionY = 0;
        commandLines.clear();
        commandLines.push_back("");
        while (true) {
            std::cin.get(c);
            if (c == '\033') {
                std::cin.get(c);
                if (c == '[') {
                    std::cin.get(c);
                    handleArrowKey(c, cursorPositionX, cursorPositionY, commandLines, commandLines[cursorPositionY], terminalSetting);
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
                    currentLineLength = commandLines[cursorPositionY].length() + terminal.getTerminalCurrentPositionRawLength();
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
        std::string finalCommand;
        for (const auto& line : commandLines) {
            finalCommand += line;
        }
        setRawMode(false);
        commandParser(finalCommand);
        setRawMode(true);
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

/**
 * @brief Set the terminal to raw mode.
 * @param enable Enable or disable raw mode.
 */
void setRawMode(bool enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
    rawEnabled = enable;
}

/**
 * @brief Handle arrow key inputs.
 * @param arrow Arrow key character.
 * @param cursorPositionX Reference to the cursor position.
 * @param command Reference to the command string.
 * @param terminalTag Terminal tag string.
 */
void handleArrowKey(char arrow, size_t& cursorPositionX, size_t& cursorPositionY, std::vector<std::string>& commandLines, std::string& command, const std::string& terminalTag) {
    switch (arrow) {
        case 'A': // Up arrow
            clearLines(commandLines);
            commandLines = {""};
            command = terminal.getPreviousCommand();
            cursorPositionX = command.length();
            cursorPositionY = commandLines.size() - 1;
            std::cout << "\033[2K\r" << terminalTag << command;
            break;
        case 'B': // Down arrow
            clearLines(commandLines);
            commandLines = {""};
            command = terminal.getNextCommand();
            cursorPositionX = command.length();
            cursorPositionY = commandLines.size() - 1;
            std::cout << "\033[2K\r" << terminalTag << command;
            break;
        case 'C': // Right arrow
            if (cursorPositionX < command.length()) {
                cursorPositionX++;
                std::cout << "\033[C";
            } else if (cursorPositionY < commandLines.size() - 1) {
                cursorPositionY++;
                cursorPositionX = 0;
                std::cout << "\033[B";
            }
            break;
        case 'D': // Left arrow
            if (cursorPositionX > 0) {
                cursorPositionX--;
                std::cout << "\033[D";
            } else if (cursorPositionY > 0) {
                cursorPositionY--;
                cursorPositionX = commandLines[cursorPositionY].length();
                std::cout << "\033[A";
            }
            break;
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
        if(userData.contains("OpenAI_API_KEY")){openAIPromptEngine.setAPIKey(userData["OpenAI_API_KEY"].get<std::string>());}
        if(userData.contains("Chat_Cache")) {
            savedChatCache = userData["Chat_Cache"].get<std::vector<std::string>>();
            openAIPromptEngine.setChatCache(savedChatCache);
        }
        if(userData.contains("Startup_Commands")){startupCommands = userData["Startup_Commands"].get<std::vector<std::string>>();}
        if(userData.contains("Shortcuts_Enabled")){shotcutsEnabled = userData["Shortcuts_Enabled"].get<bool>();}
        if(userData.contains("Shortcuts")){shortcuts = userData["Shortcuts"].get<std::map<std::string, std::string>>();}
        if(userData.contains("Text_Buffer")){textBuffer = userData["Text_Buffer"].get<bool>();}
        if(userData.contains("Text_Entry")){defaultTextEntryOnAI = userData["Text_Entry"].get<bool>();}
        if(userData.contains("Command_Prefix")){commandPrefix = userData["Command_Prefix"].get<std::string>();}
        if (userData.contains("Multi_Script_Shortcuts")){multiScriptShortcuts = userData["Multi_Script_Shortcuts"].get<std::map<std::string, std::vector<std::string>>>();}
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
        userData["OpenAI_API_KEY"] = openAIPromptEngine.getAPIKey();
        userData["Chat_Cache"] = savedChatCache;
        userData["Startup_Commands"] = startupCommands;
        userData["Shortcuts_Enabled"] = shotcutsEnabled;
        userData["Shortcuts"] = shortcuts;
        userData["Text_Buffer"] = textBuffer;
        userData["Text_Entry"] = defaultTextEntryOnAI;
        userData["Command_Prefix"] = commandPrefix;
        userData["Multi_Script_Shortcuts"] = multiScriptShortcuts;
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
    commandProcesser("terminal cd " + applicationDirectory +"/"+ DATA_DIRECTORY.string());
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

/**
 * @brief Parse and process a user command.
 * @param command Command string to parse.
 */
void commandParser(const std::string& command) {
    if (command.empty()) {
        return;
    }
    if (!runningStartup) {
        addUserInputToHistory(command);
    }
    if (command.rfind(commandPrefix, 0) == 0) {
        commandProcesser(command.substr(1));
        return;
    }
    if (defaultTextEntryOnAI) {
        chatProcess(command);
    } else {
        sendTerminalCommand(command);
    }
    terminal.addCommandToHistory(command);
}

/**
 * @brief Add user input to the command history file.
 * @param input User input string to add.
 */
void addUserInputToHistory(const std::string& input) {
    std::ofstream file(USER_COMMAND_HISTORY, std::ios_base::app);
    if (file.is_open()) {
        file << std::to_string(time(nullptr)) << " " << input << "\n";
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
 * @brief Process a multi-script shortcut command.
 * @param command Multi-script shortcut command string to process.
 */
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
            for(const auto& command : multiScriptShortcuts[strippedCommand]){
                commandProcesser(command);
            }
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
    if (lastCommandParsed == "approot") {
        goToApplicationDirectory();
    } else if (lastCommandParsed == "clear") {
        std::cout << "Clearing screen and terminal cache..." << std::endl;
        std::cout << "\033[2J\033[1;1H";
        terminal.clearTerminalCache();
    } else if(lastCommandParsed == "ss"){
        shortcutProcesser(command);
    } else if(lastCommandParsed == "mm"){
        multiScriptShortcutCommands();
    } else if (lastCommandParsed == "ai") {
        aiSettingsCommands();
    } else if (lastCommandParsed == "user") {
        userSettingsCommands();
    } else if (lastCommandParsed == "aihelp"){
        if (!defaultTextEntryOnAI && !openAIPromptEngine.getAPIKey().empty() ){
            std::string message = ("I am encountering these errors in the " + terminal.getTerminalName() + " and would like some help solving these issues. User input " + terminal.returnMostRecentUserInput() + " Terminal output " + terminal.returnMostRecentTerminalOutput());
            if (TESTING) {
                std::cout << message << std::endl;
            }
            std::cout << openAIPromptEngine.forceDirectChatGPT(message, true) << std::endl;
            return;
        }
    } else if (lastCommandParsed == "terminal") {
        try {
            std::string terminalCommand = command.substr(9);
            sendTerminalCommand(terminalCommand);
        } catch (std::out_of_range& e) { // Changed exception type
            defaultTextEntryOnAI = false;
            return;
        }
    } else if (lastCommandParsed == "exit") {
        exit();
    } else if (lastCommandParsed == "help") {
        std::cout << "Commands:" << std::endl;
        std::cout << "Command Prefix: " + commandPrefix << std::endl;
        std::cout << "ai" << std::endl;
        std::cout << "approot" << std::endl;
        std::cout << "terminal o[ARGS]" << std::endl;
        std::cout << "user" << std::endl;
        std::cout << "exit" << std::endl;
        std::cout << "clear" << std::endl;
        std::cout << "help" << std::endl;
    } else {
        std::cout << "Unknown command. Please try again." << std::endl;
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

void userDataCommands(){
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
        if(lastCommandParsed == "saveloop"){
            getNextCommand();
            if (lastCommandParsed.empty()) {
                std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
                return;
            }
            if (lastCommandParsed == "enable") {
                saveLoop = true;
                std::cout << "Text buffer enabled." << std::endl;
                return;
            }
            if (lastCommandParsed == "disable") {
                saveLoop = false;
                std::cout << "Text buffer disabled." << std::endl;
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
    std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
}

/**
 * @brief Process multi-script shortcut commands.
 */
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
    if(lastCommandParsed == "displayfullpath"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
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
    if(!incognitoChatMode){
        savedChatCache = openAIPromptEngine.getChatCache();
    } else {
        savedChatCache.clear();
    }
    writeUserData();
    setRawMode(false);
    std::cout << "Exiting..." << std::endl;
    std::exit(0);
}

/**
 * @brief Process AI settings commands.
 */
void aiSettingsCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        defaultTextEntryOnAI = true;
        showChatHistory();
        return;
    }
    if (lastCommandParsed == "log") {
        std::string lastChatSent = openAIPromptEngine.getLastPromptUsed();
        std::string lastChatReceived = openAIPromptEngine.getLastResponseReceived();
        std::string fileName = (DATA_DIRECTORY / ("OpenAPI_Chat_" + std::to_string(time(nullptr)) + ".txt")).string();
        std::ofstream file(fileName);
        if (file.is_open()) {
            file << "Chat Sent: " << lastChatSent << "\n";
            file << "Chat Received: " << lastChatReceived << "\n";
            file.close();
            std::cout << "Chat log saved to " << fileName << std::endl;
        } else {
            std::cout << "An error occurred while creating the chat file." << std::endl;
            return;
        }
        getNextCommand();
        if (lastCommandParsed.empty()) {
            return;
        }
        if (lastCommandParsed == "extract") {
            getNextCommand();
            if (lastCommandParsed.empty()) {
                extractCodeSnippet(fileName, (DATA_DIRECTORY / "extracted_code").string());
                std::filesystem::remove(fileName);
                return;
            }
            std::string fileNameToSave = lastCommandParsed;
            getNextCommand();
            if (lastCommandParsed.empty()) {
                extractCodeSnippet(fileName, (DATA_DIRECTORY / fileNameToSave).string());
                std::filesystem::remove(fileName);
                return;
            }
            if(lastCommandParsed == "approot"){
                extractCodeSnippet(fileName, (DATA_DIRECTORY / fileNameToSave).string());
                std::filesystem::remove(fileName);
                return;
            }
            if(lastCommandParsed == "currentpath"){
                std::string currentPath = terminal.getCurrentFilePath();
                extractCodeSnippet(fileName, (std::filesystem::path(currentPath) / fileNameToSave).string());
                std::filesystem::remove(fileName);
                return;
            }
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    }
    if (lastCommandParsed == "apikey") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        if (lastCommandParsed == "set") {
            getNextCommand();
            if (lastCommandParsed.empty()) {
                std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
                return;
            }
            openAIPromptEngine.setAPIKey(lastCommandParsed);
            if (openAIPromptEngine.testAPIKey(openAIPromptEngine.getAPIKey())) {
                std::cout << "OpenAI API key set." << std::endl;
                return;
            } else {
                std::cout << "Invalid API key." << std::endl;
                return;
            }
        }
        if (lastCommandParsed == "get") {
            std::cout << openAIPromptEngine.getAPIKey() << std::endl;
            return;
        }
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    }
    if (lastCommandParsed == "chat") {
        aiChatCommands();
        return;
    }
    if (lastCommandParsed == "get") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        std::cout << openAIPromptEngine.getResponseData(lastCommandParsed) << std::endl;
        return;
    }
    if (lastCommandParsed == "dump") {
        std::cout << openAIPromptEngine.getResponseData("all") << std::endl;
        std::cout << openAIPromptEngine.getLastPromptUsed() << std::endl;
        return;
    }
    if (lastCommandParsed == "mode") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "The current assistant mode is " << openAIPromptEngine.getAssistantType() << std::endl;
            return;
        }
        openAIPromptEngine.setAssistantType(lastCommandParsed);
        std::cout << "Assistant mode set to " << lastCommandParsed << std::endl;
        return;
    }
    if (lastCommandParsed == "file") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        std::vector<std::string> filesAtPath = terminal.getFilesAtCurrentPath();
        if (lastCommandParsed == "add"){
            getNextCommand();
            if (lastCommandParsed.empty()) {
                std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
                return;
            }
            if (lastCommandParsed == "all"){
                std::cout << "Processed " << openAIPromptEngine.addFiles(filesAtPath) <<  " characters."  << std::endl;
                return;
            }
            std::string fileToAdd = terminal.getFullPathOfFile(lastCommandParsed);
            if(fileToAdd.empty()){
                std::cout << "File not found." << std::endl;
                return;
            }
            std::cout << "Processed "<<openAIPromptEngine.addFile(fileToAdd) << " characters." << std::endl;
            return;
        }
        if (lastCommandParsed == "remove"){
            getNextCommand();
            if (lastCommandParsed.empty()) {
                std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
                return;
            }
            if (lastCommandParsed == "all"){
                openAIPromptEngine.clearFiles();
                return;
            }
            std::string fileToRemove = terminal.getFullPathOfFile(lastCommandParsed);
            if(fileToRemove.empty()){
                std::cout << "File not found." << std::endl;
                return;
            }
            openAIPromptEngine.removeFile(fileToRemove);
            return;
        }
        if (lastCommandParsed == "active"){
            std::vector<std::string> activeFiles = openAIPromptEngine.getFiles();
            std::cout << "Active Files: " << std::endl;
            for(const auto& file : activeFiles){
                std::cout << file << std::endl;
            }
            std::cout << "Total characters processed: " << openAIPromptEngine.getFileContents().length() << std::endl;
            return;
        }
        if (lastCommandParsed == "available"){
            std::cout << "Files at current path: " << std::endl;
            for(const auto& file : filesAtPath){
                std::cout << file << std::endl;
            }
            return;
        }
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    }
    if(lastCommandParsed == "model"){
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "The current model is " << openAIPromptEngine.getModel() << std::endl;
            return;
        }
        openAIPromptEngine.setModel(lastCommandParsed);
        std::cout << "Model set to " << lastCommandParsed << std::endl;
        return;
    }
    if (lastCommandParsed == "help") {
        std::cout << "Commands: " << std::endl;
        std::cout << "log: extract o[ARGS] o[ARGS]" << std::endl;
        std::cout << "apikey: set [ARGS], get" << std::endl;
        std::cout << "chat: [ARGS]" << std::endl;
        std::cout << "get: [ARGS]" << std::endl;
        std::cout << "dump" << std::endl;
        std::cout << "mode: [ARGS]" << std::endl;
        std::cout << "file: add [ARGS], remove [ARGS], active, available" << std::endl;
        std::cout << "model: [ARGS]" << std::endl;
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

/**
 * @brief Process AI chat commands.
 */
void aiChatCommands() {
    getNextCommand();
    if (lastCommandParsed.empty()) {
        std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
        return;
    }
    if (lastCommandParsed == "history") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        if (lastCommandParsed == "clear") {
            openAIPromptEngine.clearChatCache();
            savedChatCache.clear();
            std::cout << "Chat history cleared." << std::endl;
            return;
        }
    }
    if (lastCommandParsed == "cache") {
        getNextCommand();
        if (lastCommandParsed.empty()) {
            std::cout << "Unknown command. No given ARGS. Try 'help'" << std::endl;
            return;
        }
        if (lastCommandParsed == "enable") {
            openAIPromptEngine.setCacheTokens(true);
            std::cout << "Cache tokens enabled." << std::endl;
            return;
        }
        if (lastCommandParsed == "disable") {
            openAIPromptEngine.setCacheTokens(false);
            std::cout << "Cache tokens disabled." << std::endl;
            return;
        }
        if (lastCommandParsed == "clear") {
            openAIPromptEngine.clearChatCache();
            savedChatCache.clear();
            std::cout << "Chat history cleared." << std::endl;
            return;
        }
    }
    if (lastCommandParsed == "help") {
        std::cout << "Commands: " << std::endl;
        std::cout << "history: clear" << std::endl;
        std::cout << "cache: enable, disable, clear" << std::endl;
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

/**
 * @brief Process a chat message.
 * @param message Chat message to process.
 */
void chatProcess(const std::string& message) {
    //this needs work
    if (message.empty()) {
        return;
    }
    if (openAIPromptEngine.getAPIKey().empty()) {
        std::cout << "There is no OpenAPI key set." << std::endl;
        return;
    }
    std::string response = openAIPromptEngine.chatGPT(message,true);
    std::cout << "ChatGPT: " << response << std::endl;
}

/**
 * @brief Show the chat history.
 */
void showChatHistory() {
    if (!openAIPromptEngine.getChatCache().empty()) {
        std::cout << "Chat history:" << std::endl;
        for (const auto& message : openAIPromptEngine.getChatCache()) {
            std::cout << message << std::endl;
        }
    }
}

/**
 * @brief Extract a code snippet from a log file.
 * @param logFile Log file to extract from.
 * @param fileName File name to save the extracted code snippet.
 */
void extractCodeSnippet(const std::string& logFile, const std::string& fileName) {
    std::ifstream file(logFile);
    if (file.is_open()) {
        std::string line;
        std::string codeSnippet;
        std::string fileExtension;
        bool inCodeBlock = false;
        while (std::getline(file, line)) {
            if (line.rfind("```", 0) == 0) {
                if (inCodeBlock) {
                    break;
                } else {
                    inCodeBlock = true;
                    std::string language = line.substr(3);
                    fileExtension = getFileExtensionForLanguage(language);
                }
            } else if (inCodeBlock) {
                codeSnippet += line + "\n";
            }
        }
        file.close();
        if (!fileExtension.empty() && !codeSnippet.empty()) {
            std::ofstream outputFile(fileName + "." + fileExtension);
            if (outputFile.is_open()) {
                outputFile << codeSnippet;
                outputFile.close();
                std::cout << "Code snippet extracted and saved to " << fileName + "." + fileExtension << std::endl;
            }
        } else {
            std::cout << "No code snippet found in the log file." << std::endl;
        }
    } else {
        std::cout << "An error occurred while extracting the code snippet." << std::endl;
    }
}

/**
 * @brief Get the file extension for a given programming language.
 * @param language Programming language.
 * @return File extension for the given language.
 */
std::string getFileExtensionForLanguage(const std::string& language) {
    if (language == "java") return "java";
    if (language == "python") return "py";
    if (language == "javascript") return "js";
    if (language == "typescript") return "ts";
    if (language == "csharp") return "cs";
    if (language == "cpp") return "cpp";
    if (language == "c") return "c";
    if (language == "html") return "html";
    if (language == "css") return "css";
    if (language == "json") return "json";
    if (language == "xml") return "xml";
    return "txt";
}



