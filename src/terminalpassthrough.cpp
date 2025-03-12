#include "terminalpassthrough.h"

TerminalPassthrough::TerminalPassthrough() : displayWholePath(false) {
    currentDirectory = fs::current_path().string();
    terminalCacheUserInput = std::vector<std::string>();
    terminalCacheTerminalOutput = std::vector<std::string>();
}

std::string TerminalPassthrough::getTerminalName(){
    #ifdef _WIN32
        return "cmd";
    #elif __linux__
        return "bash";
    #else
        return "sh";
    #endif
}

std::vector<std::string> TerminalPassthrough::getFilesAtCurrentPath(){
    std::vector<std::string> files;
    for (const auto& entry : fs::directory_iterator(getCurrentFilePath())) {
        files.push_back(entry.path().string());
    }
    return files;
}

void TerminalPassthrough::setDisplayWholePath(bool displayWholePath){
    this->displayWholePath = displayWholePath;
}

std::string TerminalPassthrough::getFullPathOfFile(const std::string& file){
    if (fs::exists (fs::path(getCurrentFilePath()) / file)) {
        return (fs::path(getCurrentFilePath()) / file).string();
    }
    return "";
}

void TerminalPassthrough::printCurrentTerminalPosition(){
    std::cout << returnCurrentTerminalPosition();
}

int TerminalPassthrough::getTerminalCurrentPositionRawLength(){
    return terminalCurrentPositionRawLength;
}

std::string TerminalPassthrough::returnCurrentTerminalPosition(){
    int gitInfoLength = 0;
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
            gitInfoLength = repoName.length() + branchName.length() + 9;
            gitInfo = "\033[1;32m" + repoName + RESET_COLOR+BLUE_COLOR_BOLD+" git:("+RESET_COLOR+YELLOW_COLOR_BOLD + branchName +RESET_COLOR+BLUE_COLOR_BOLD+ ")"+RESET_COLOR;
        } catch (const std::exception& e) {
            std::cerr << "Error reading git HEAD file: " << e.what() << std::endl;
        }
        terminalCurrentPositionRawLength = getTerminalName().length() + 2 + gitInfoLength;
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

std::thread TerminalPassthrough::executeCommand(std::string command){
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

void TerminalPassthrough::toggleDisplayWholePath(){
    setDisplayWholePath(!displayWholePath);
}

bool TerminalPassthrough::isDisplayWholePath(){
    return displayWholePath;
}

std::vector<std::string> TerminalPassthrough::getTerminalCacheUserInput(){
    return terminalCacheUserInput;
}

std::vector<std::string> TerminalPassthrough::getTerminalCacheTerminalOutput(){
    return terminalCacheTerminalOutput;
}

void TerminalPassthrough::clearTerminalCache(){
    terminalCacheUserInput.clear();
    terminalCacheTerminalOutput.clear();
}

std::string TerminalPassthrough::returnMostRecentUserInput(){
    return terminalCacheUserInput.back();
}

std::string TerminalPassthrough::returnMostRecentTerminalOutput(){
    return terminalCacheTerminalOutput.back();
}

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

std::string TerminalPassthrough::getCurrentFilePath(){
    if (currentDirectory.empty()) {
        return fs::current_path().string();
    }
    return currentDirectory;
}

std::string TerminalPassthrough::getCurrentFileName(){
    std::string currentDirectory = getCurrentFilePath();
    std::string currentFileName = fs::path(currentDirectory).filename().string();
    if (currentFileName.empty()) {
        return "/";
    }
    return currentFileName;
}

bool TerminalPassthrough::isRootPath(const fs::path& path){
    return path == path.root_path();
}

void TerminalPassthrough::addCommandToHistory(const std::string& command) {
    if (command.empty()) {
        return;
    }
    if (std::find(terminalCacheUserInput.begin(), terminalCacheUserInput.end(), command) != terminalCacheUserInput.end()) {
        return;
    }
    terminalCacheUserInput.push_back(command);
}
