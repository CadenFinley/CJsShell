#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <regex>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

class TextEngine {
public:
    static std::string speedSetting;
    static const std::string yellowColor;
    static const std::string resetColor;
    static const std::string greenColor;
    static int MAX_LINE_WIDTH;
    static const std::vector<std::string> BREAK_COMMANDS;

    static std::string setWidth();
    static void printWithDelays(const std::string& data, bool inputBuffer, bool newLine);
    static void printNoDelay(const std::string& data, bool inputBuffer, bool newLine);
    static void clearScreen();
    static void enterToNext();
    static bool checkValidInput(const std::string& command);
    static std::string parseCommand(const std::string& command, const std::vector<std::string>& possibleCommands);
    static int getMatchLength(const std::string& command, const std::string& possibleCommand);
    static bool has(const std::vector<std::string>& possibleCommands, const std::string& matchedCommand);
    static void setSpeedSetting(const std::string& speed);
    static std::string getSpeedSetting();
};

// Definitions for static members
std::string TextEngine::speedSetting = "nodelay";
const std::string TextEngine::yellowColor = "\033[1;33m";
const std::string TextEngine::resetColor = "\033[0m";
const std::string TextEngine::greenColor = "\033[0;32m";
int TextEngine::MAX_LINE_WIDTH = 50;
const std::vector<std::string> TextEngine::BREAK_COMMANDS = {};

// Method implementations
std::string TextEngine::setWidth() {
    try {
        std::string os = "unknown";
        #ifdef _WIN32
            os = "windows";
        #elif __linux__
            os = "linux";
        #elif __APPLE__
            os = "mac";
        #endif

        std::string command = (os == "windows") ? "mode con" : "tput cols";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            throw std::runtime_error("popen() failed!");
        }
        char buffer[128];
        std::string result = "";
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);

        if (os == "windows") {
            std::regex re("Columns: (\\d+)");
            std::smatch match;
            if (std::regex_search(result, match, re) && match.size() > 1) {
                MAX_LINE_WIDTH = std::stoi(match.str(1)) + 20;
            }
        } else {
            MAX_LINE_WIDTH = std::stoi(result) + 20;
        }
        return "Terminal width: " + std::to_string(MAX_LINE_WIDTH);
    } catch (...) {
        MAX_LINE_WIDTH = 30;
        return "Terminal width: 50";
    }
}

void TextEngine::printWithDelays(const std::string& data, bool inputBuffer, bool newLine) {
    if (data.empty()) return;
    bool needToBreak = false;
    std::string text = data;
    if (inputBuffer) {
        text += yellowColor + " (press enter to type)" + resetColor;
    }
    int currentLineWidth = 0;
    std::vector<std::string> words;
    std::istringstream iss(text);
    for (std::string s; iss >> s; ) {
        words.push_back(s);
    }
    for (const auto& word : words) {
        if (word.find("\\") != std::string::npos) {
            needToBreak = true;
        }
        if (inputBuffer) {
            if ((currentLineWidth + word.length() >= MAX_LINE_WIDTH + 30) && currentLineWidth != 0) {
                std::cout << '\n';
                currentLineWidth = 0;
            }
        } else {
            if ((currentLineWidth + word.length() >= MAX_LINE_WIDTH) && currentLineWidth != 0) {
                std::cout << '\n';
                currentLineWidth = 0;
            }
        }
        for (const auto& ch : word) {
            if (ch == '\n') {
                std::cout << '\n';
                currentLineWidth = 0;
                needToBreak = false;
                continue;
            }
            if (std::isalnum(ch) && !std::isspace(ch)) {
                try {
                    if (speedSetting == "slow") {
                        std::this_thread::sleep_for(std::chrono::milliseconds(30));
                    } else if (speedSetting == "fast") {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    } else if (speedSetting == "normal") {
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    }
                } catch (const std::exception&) {
                    // Handle exception
                }
            }
            std::cout << ch;
            currentLineWidth++;
        }
        if (needToBreak) {
            std::cout << '\n';
            currentLineWidth = 0;
            needToBreak = false;
        }
        if (currentLineWidth > 0) {
            std::cout << ' ';
            currentLineWidth++;
        }
    }
    if (newLine) {
        std::cout << '\n';
    }
    if (inputBuffer) {
        std::cin.ignore();
        std::cout << greenColor << "> " << resetColor;
    }
}

void TextEngine::printNoDelay(const std::string& data, bool inputBuffer, bool newLine) {
    if (data.empty()) return;
    bool needToBreak = false;
    std::string text = data;
    if (inputBuffer) {
        text += yellowColor + " (press enter to type)" + resetColor;
    }
    int currentLineWidth = 0;
    std::vector<std::string> words;
    std::istringstream iss(text);
    for (std::string s; iss >> s; ) {
        words.push_back(s);
    }
    for (const auto& word : words) {
        if (word.find("\\") != std::string::npos) {
            needToBreak = true;
        }
        if (inputBuffer) {
            if ((currentLineWidth + word.length() >= MAX_LINE_WIDTH + 30) && currentLineWidth != 0) {
                std::cout << '\n';
                currentLineWidth = 0;
            }
        } else {
            if ((currentLineWidth + word.length() >= MAX_LINE_WIDTH) && currentLineWidth != 0) {
                std::cout << '\n';
                currentLineWidth = 0;
            }
        }
        for (const auto& ch : word) {
            if (ch == '\n') {
                std::cout << '\n';
                currentLineWidth = 0;
                needToBreak = false;
                continue;
            }
            std::cout << ch;
            currentLineWidth++;
        }
        if (needToBreak) {
            std::cout << '\n';
            currentLineWidth = 0;
            needToBreak = false;
        }
        if (currentLineWidth > 0) {
            std::cout << ' ';
            currentLineWidth++;
        }
    }
    if (newLine) {
        std::cout << '\n';
    }
    if (inputBuffer) {
        std::cin.ignore();
        std::cout << greenColor << "> " << resetColor;
    }
}

void TextEngine::clearScreen() {
    std::string os = "unknown";
    #ifdef _WIN32
        os = "windows";
    #elif __linux__
        os = "linux";
    #elif __APPLE__
        os = "mac";
    #endif

    if (os == "windows") {
        std::system("cls");
    } else {
        std::cout << "\033[H\033[2J";
        std::cout.flush();
    }
}

void TextEngine::enterToNext() {
    printNoDelay(yellowColor + "Press Enter to continue" + resetColor, false, false);
    std::cin.ignore();
}

bool TextEngine::checkValidInput(const std::string& command) {
    return !command.empty();
}

std::string TextEngine::parseCommand(const std::string& command, const std::vector<std::string>& possibleCommands) {
    std::string matchedCommand = command;
    int maxMatchLength = 0;
    for (const auto& illegalCommand : BREAK_COMMANDS) {
        if (command == illegalCommand) {
            return command;
        }
    }
    for (const auto& possibleCommand : possibleCommands) {
        if (command == possibleCommand) {
            return command;
        }
        int matchLength = getMatchLength(command, possibleCommand);
        if (matchLength > maxMatchLength) {
            maxMatchLength = matchLength;
            matchedCommand = possibleCommand;
        }
    }
    return (maxMatchLength > 0 && has(possibleCommands, matchedCommand)) ? matchedCommand : command;
}

int TextEngine::getMatchLength(const std::string& command, const std::string& possibleCommand) {
    if (command.empty() || possibleCommand.empty()) {
        return 0;
    }
    int length = std::min(command.length(), possibleCommand.length());
    int matchLength = 0;
    for (int i = 0; i < length; ++i) {
        if (command[i] == possibleCommand[i]) {
            matchLength++;
        } else {
            break;
        }
    }
    return matchLength;
}

bool TextEngine::has(const std::vector<std::string>& possibleCommands, const std::string& matchedCommand) {
    return std::find(possibleCommands.begin(), possibleCommands.end(), matchedCommand) != possibleCommands.end();
}

void TextEngine::setSpeedSetting(const std::string& speed) {
    speedSetting = speed;
}

std::string TextEngine::getSpeedSetting() {
    return speedSetting;
}
