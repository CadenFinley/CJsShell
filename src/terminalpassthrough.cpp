#include "terminalpassthrough.h"
#include <iostream>
#include <thread>
#include <filesystem>
#include <regex>
#include <fstream>

namespace fs = std::filesystem; 

TerminalPassthrough::TerminalPassthrough() : displayWholePath(false) {
    currentDirectory = fs::current_path().string();
    terminalCacheUserInput = std::vector<std::string>();
    terminalCacheTerminalOutput = std::vector<std::string>();
}

std::string TerminalPassthrough::getTerminalName() {
    #ifdef _WIN32
        return "cmd";
    #elif __linux__
        return "bash";
    #else
        return "sh";
    #endif
}

void TerminalPassthrough::setDisplayWholePath(bool displayWholePath) {
    this->displayWholePath = displayWholePath;
}

void TerminalPassthrough::printCurrentTerminalPosition() {
    std::cout << returnCurrentTerminalPosition();
}

std::string TerminalPassthrough::returnCurrentTerminalPosition() {
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

std::thread TerminalPassthrough::executeCommand(std::string command) {
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

void TerminalPassthrough::toggleDisplayWholePath() {
    setDisplayWholePath(!displayWholePath);
}

bool TerminalPassthrough::isDisplayWholePath() {
    return displayWholePath;
}

std::vector<std::string> TerminalPassthrough::getTerminalCacheUserInput() {
    return terminalCacheUserInput;
}

std::vector<std::string> TerminalPassthrough::getTerminalCacheTerminalOutput() {
    return terminalCacheTerminalOutput;
}

void TerminalPassthrough::clearTerminalCache() {
    terminalCacheUserInput.clear();
    terminalCacheTerminalOutput.clear();
}

std::string TerminalPassthrough::returnMostRecentUserInput() {
    if (!terminalCacheUserInput.empty()) {
        return terminalCacheUserInput.back();
    }
    return "";
}

std::string TerminalPassthrough::returnMostRecentTerminalOutput() {
    if (!terminalCacheTerminalOutput.empty()) {
        return terminalCacheTerminalOutput.back();
    }
    return "";
}

std::string TerminalPassthrough::getCurrentFilePath() {
    return currentDirectory;
}

std::string TerminalPassthrough::getCurrentFileName() {
    fs::path filePath = fs::path(getCurrentFilePath()).filename();
    return filePath.empty() ? "/" : filePath.string();
}

bool TerminalPassthrough::isRootPath(const fs::path& path) {
    return path == path.root_path();
}




