#ifndef TERMINALPASSTHROUGH_H
#define TERMINALPASSTHROUGH_H

#include <string>
#include <thread>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

class TerminalPassthrough {
public:
    TerminalPassthrough(){
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

    /**
     * @brief Return the current terminal position as a string.
     * @return Current terminal position.
     */
    std::string returnCurrentTerminalPosition(){
        std::string gitInfo;
        fs::path currentPath = fs::path(getCurrentFilePath());
        fs::path gitHeadPath;
        while (!isRootPath(currentPath)) { // Use isRootPath method
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
                gitInfo = "\033[1;32m" + repoName + " git:(" + branchName + ")\033[0m";
            } catch (const std::exception& e) {
                std::cerr << "Error reading git HEAD file: " << e.what() << std::endl;
            }
            return getTerminalName() + ": " + gitInfo + " ";
        }
        if (displayWholePath) {
            return getTerminalName() + ": \033[1;34m" + getCurrentFilePath() + "\033[0m ";
        } else {
            return getTerminalName() + ": \033[1;34m" + getCurrentFileName() + "\033[0m ";
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
            if (command.rfind("cd ", 0) == 0) {
                std::string newDir = command.substr(3);
                if (newDir == "/") {
                    currentDirectory = "/";
                } else if (newDir == "..") {
                    fs::path dir = fs::path(currentDirectory).parent_path();
                    if (fs::exists(dir) && fs::is_directory(dir)) {
                        currentDirectory = dir.string();
                    } else {
                        throw std::runtime_error("No such file or directory");
                    }
                } else {
                    fs::path dir = fs::path(currentDirectory) / newDir;
                    if (fs::exists(dir) && fs::is_directory(dir)) {
                        currentDirectory = fs::canonical(dir).string();
                    } else {
                        throw std::runtime_error("No such file or directory");
                    }
                }
            } else {
                std::string fullCommand = "cd " + currentDirectory + " && " + command;
                if (getTerminalName() == "cmd") {
                    fullCommand = "cmd /c \"cd /d " + currentDirectory + " && " + command + "\"";
                } else {
                    fullCommand = getTerminalName() + " -c \"cd " + currentDirectory + " && " + command + "\"";
                }
                std::system(fullCommand.c_str());
            }
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

private:
    std::string currentDirectory;
    bool displayWholePath;
    std::vector<std::string> terminalCacheUserInput;
    std::vector<std::string> terminalCacheTerminalOutput;

    /**
     * @brief Get the current file path.
     * @return Current file path as a string.
     */
    std::string getCurrentFilePath(){
        return currentDirectory;
    }

    /**
     * @brief Get the current file name.
     * @return Current file name as a string.
     */
    std::string getCurrentFileName(){
        return fs::path(currentDirectory).filename().string();
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

#endif // TERMINALPASSTHROUGH_H
