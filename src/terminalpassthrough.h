#ifndef TERMINALPASSTHROUGH_H
#define TERMINALPASSTHROUGH_H

#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <regex>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <array>

namespace fs = std::filesystem;

class TerminalPassthrough {
public:
    TerminalPassthrough() : displayWholePath(false) {
        currentDirectory = fs::current_path().string();
        terminalCacheUserInput = std::vector<std::string>();
        terminalCacheTerminalOutput = std::vector<std::string>();
    }

    /**
     * @brief Get the name of the terminal based on the operating system.
     * @return Terminal name as a string.
     */
    std::string getTerminalName(){
        #ifdef _WIN32
            return "cmd";
        #elif __linux__
            return "bash";
        #else
            return "sh";
        #endif
    }

    /**
     * @brief Set whether to display the whole path or just the current directory name.
     * @param displayWholePath Boolean flag to set the display mode.
     */
    void setDisplayWholePath(bool displayWholePath){
        this->displayWholePath = displayWholePath;
    }

    /**
     * @brief Print the current terminal position to the console.
     */
    void printCurrentTerminalPosition(){
        std::cout << returnCurrentTerminalPosition();
    }

    int getTerminalCurrentPositionRawLength(){
        return terminalCurrentPositionRawLength;
    }

    /**
     * @brief Return the current terminal position as a string.
     * @return Current terminal position.
     */
    std::string returnCurrentTerminalPosition(){
        std::string gitInfo;
        fs::path currentPath = fs::path(getCurrentFilePath());
        fs::path gitHeadPath;
        while (!isRootPath(currentPath)) {
            gitHeadPath = currentPath / ".git" / "HEAD";
            if (fs::exists(gitHeadPath)) {
                break;
            }
            currentPath = currentPath.parent_path();
        }
        bool gitRepo = fs::exists(gitHeadPath);
        if (gitRepo) {
            try {
                std::ifstream headFile(gitHeadPath);
                std::string line;
                std::regex headPattern("ref: refs/heads/(.*)");
                std::smatch match;
                std::string branchName;
                while (std::getline(headFile, line)) {
                    if (std::regex_search(line, match, headPattern)) {
                        branchName = match[1];
                        break;
                    }
                }
                std::string repoName = displayWholePath ? getCurrentFilePath() : getCurrentFileName();
                gitInfo = "\033[1;32m" + repoName + RESET_COLOR+BLUE_COLOR_BOLD+" git:("+RESET_COLOR+YELLOW_COLOR_BOLD + branchName +RESET_COLOR+BLUE_COLOR_BOLD+ ")"+RESET_COLOR;
            } catch (const std::exception& e) {
                std::cerr << "Error reading git HEAD file: " << e.what() << std::endl;
            }
            terminalCurrentPositionRawLength = getTerminalName().length() + 2 + gitInfo.length();
            return RED_COLOR_BOLD+getTerminalName()+RESET_COLOR + ": " + gitInfo + " ";
        }
        if (displayWholePath) {
            terminalCurrentPositionRawLength = getCurrentFilePath().length() + getTerminalName().length() + 2;
            return RED_COLOR_BOLD+getTerminalName()+RESET_COLOR + ": \033[1;34m" + getCurrentFilePath() + "\033[0m" + " ";
        } else {
            terminalCurrentPositionRawLength = getCurrentFileName().length() + getTerminalName().length() + 2;
            return RED_COLOR_BOLD+getTerminalName()+RESET_COLOR + ": \033[1;34m" + getCurrentFileName() + "\033[0m" + " ";
        }
    }

    /**
     * @brief Execute a command in the terminal.
     * @param command Command to execute.
     * @return A thread running the command.
     */
    std::thread executeCommand(std::string command){
        terminalCacheUserInput.push_back(command);
        return std::thread([this, command]() {
            try {
                std::string result;
                if (command.rfind("cd ", 0) == 0) {
                    std::string newDir = command.substr(3);
                    if (newDir == "/") {
                        currentDirectory = "/";
                    } else if (newDir == "..") {
                        fs::path dir = fs::path(currentDirectory).parent_path();
                        if (fs::exists(dir) && fs::is_directory(dir)) {
                            currentDirectory = dir.string();
                        } else {
                            result = "No such file or directory: " + dir.string();
                            throw std::runtime_error("No such file or directory");
                        }
                    } else {
                        fs::path dir = fs::path(currentDirectory) / newDir;
                        if (fs::exists(dir) && fs::is_directory(dir)) {
                            currentDirectory = fs::canonical(dir).string();
                        } else {
                            result = "No such file or directory: " + dir.string();
                            throw std::runtime_error("No such file or directory");
                        }
                    }
                    result = "Changed directory to: " + currentDirectory;
                } else {
                    std::array<char, 128> buffer;
                    std::string fullCommand;
                    if (getTerminalName() == "cmd") {
                        fullCommand = "cd " + currentDirectory + " && " + command + " 2>&1";
                    } else {
                        fullCommand = getTerminalName() + " -c \"cd " + currentDirectory + " && " + command + " 2>&1\"";
                    }
                    int terminalExecCode = std::system(fullCommand.c_str());
                        if(terminalExecCode != 0){
                            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(fullCommand.c_str(), "r"), pclose);
                            if (!pipe) {
                                throw std::runtime_error("popen() failed!");
                            }
                            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                                result += buffer.data();
                            }
                        } else {
                            result = "Terminal Output result: " + std::to_string(terminalExecCode);
                        }
                }
                terminalCacheTerminalOutput.push_back(result);
            } catch (const std::exception& e) {
                std::cerr << "Error executing command: '" << command << "' " << e.what() << std::endl;
            }
        });
    }
    
    /**
     * @brief Toggle the display mode between whole path and current directory name.
     */
    void toggleDisplayWholePath(){
        setDisplayWholePath(!displayWholePath);
    }

    /**
     * @brief Check if the display mode is set to show the whole path.
     * @return True if displaying the whole path, false otherwise.
     */
    bool isDisplayWholePath(){
        return displayWholePath;
    }

    /**
     * @brief Get the user input cache.
     * @return Vector of user input strings.
     */
    std::vector<std::string> getTerminalCacheUserInput(){
        return terminalCacheUserInput;
    }

    /**
     * @brief Get the terminal output cache.
     * @return Vector of terminal output strings.
     */
    std::vector<std::string> getTerminalCacheTerminalOutput(){
        return terminalCacheTerminalOutput;
    }

    /**
     * @brief Clear the terminal cache for both user input and terminal output.
     */
    void clearTerminalCache(){
        terminalCacheUserInput.clear();
        terminalCacheTerminalOutput.clear();
    }

    /**
     * @brief Return the most recent user input from the cache.
     * @return Most recent user input string.
     */
    std::string returnMostRecentUserInput(){
        return terminalCacheUserInput.back();
    }

    /**
     * @brief Return the most recent terminal output from the cache.
     * @return Most recent terminal output string.
     */
    std::string returnMostRecentTerminalOutput(){
        return terminalCacheTerminalOutput.back();
    }

    /**
     * @brief Get the previous command from the history.
     * @return Previous command string.
     */
    std::string getPreviousCommand();

    /**
     * @brief Get the next command from the history.
     * @return Next command string.
     */
    std::string getNextCommand();

private:
    std::string currentDirectory;
    bool displayWholePath;
    std::vector<std::string> terminalCacheUserInput;
    std::vector<std::string> terminalCacheTerminalOutput;
    std::string RED_COLOR_BOLD = "\033[1;31m";
    std::string RESET_COLOR = "\033[0m";
    std::string BLUE_COLOR_BOLD = "\033[1;34m";
    std::string YELLOW_COLOR_BOLD = "\033[1;33m";
    int commandHistoryIndex = -1;
    int terminalCurrentPositionRawLength = 0;

    /**
     * @brief Get the current file path.
     * @return Current file path as a string.
     */
    std::string getCurrentFilePath(){
        if (currentDirectory.empty()) {
            return fs::current_path().string();
        }
        return currentDirectory;
    }

    /**
     * @brief Get the current file name.
     * @return Current file name as a string.
     */
    std::string getCurrentFileName(){
        std::string currentDirectory = getCurrentFilePath();
        std::string currentFileName = fs::path(currentDirectory).filename().string();
        if (currentFileName.empty()) {
            return "/";
        }
        return currentFileName;
    }

    /**
     * @brief Check if the given path is the root path.
     * @param path Filesystem path to check.
     * @return True if the path is the root path, false otherwise.
     */
    bool isRootPath(const fs::path& path){
        return path == path.root_path();
    }
};

std::string TerminalPassthrough::getPreviousCommand() {
    if (terminalCacheUserInput.empty()) {
        return "";
    }
    if (commandHistoryIndex > 0) {
        commandHistoryIndex--;
    } else {
        commandHistoryIndex = terminalCacheUserInput.size() - 1;
    }
    return terminalCacheUserInput[commandHistoryIndex];
}

std::string TerminalPassthrough::getNextCommand() {
    if (terminalCacheUserInput.empty()) {
        return "";
    }
    if (commandHistoryIndex < terminalCacheUserInput.size() - 1) {
        commandHistoryIndex++;
    } else {
        commandHistoryIndex = 0;
    }
    return terminalCacheUserInput[commandHistoryIndex];
}

#endif // TERMINALPASSTHROUGH_H
