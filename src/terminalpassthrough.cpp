#include "terminalpassthrough.h"

TerminalPassthrough::TerminalPassthrough() : displayWholePath(false) {
    currentDirectory = std::filesystem::current_path().string();
    terminalCacheUserInput = std::vector<std::string>();
    terminalCacheTerminalOutput = std::vector<std::string>();
}

std::string TerminalPassthrough::getTerminalName(){
    #ifdef __linux__
        return "bash";
    #else
        return "sh";
    #endif
}

std::vector<std::string> TerminalPassthrough::getFilesAtCurrentPath(){
    std::vector<std::string> files;
    for (const auto& entry : std::filesystem::directory_iterator(getCurrentFilePath())) {
        files.push_back(entry.path().string());
    }
    return files;
}

void TerminalPassthrough::setDisplayWholePath(bool displayWholePath){
    this->displayWholePath = displayWholePath;
}

void TerminalPassthrough::setAliases(const std::map<std::string, std::string>& aliases){
    this->aliases = aliases;
}

std::string TerminalPassthrough::getFullPathOfFile(const std::string& file){
    if (std::filesystem::exists (std::filesystem::path(getCurrentFilePath()) / file)) {
        return (std::filesystem::path(getCurrentFilePath()) / file).string();
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
    std::filesystem::path currentPath = std::filesystem::path(getCurrentFilePath());
    std::filesystem::path gitHeadPath;
    while (!isRootPath(currentPath)) {
        gitHeadPath = currentPath / ".git" / "HEAD";
        if (std::filesystem::exists(gitHeadPath)) {
            break;
        }
        currentPath = currentPath.parent_path();
    }
    bool gitRepo = std::filesystem::exists(gitHeadPath);
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
            gitInfo = GIT_COLOR + repoName + RESET_COLOR+DIRECTORY_COLOR+" git:("+RESET_COLOR+BRANCH_COLOR + branchName +RESET_COLOR+DIRECTORY_COLOR+ ")"+RESET_COLOR;
        } catch (const std::exception& e) {
            std::cerr << "Error reading git HEAD file: " << e.what() << std::endl;
        }
        terminalCurrentPositionRawLength = getTerminalName().length() + 2 + gitInfoLength;
        return SHELL_COLOR+getTerminalName()+RESET_COLOR + " " + gitInfo + " ";
    }
    if (displayWholePath) {
        terminalCurrentPositionRawLength = getCurrentFilePath().length() + getTerminalName().length() + 2;
        return SHELL_COLOR+getTerminalName()+RESET_COLOR + " " + DIRECTORY_COLOR + getCurrentFilePath() + RESET_COLOR + " ";
    } else {
        terminalCurrentPositionRawLength = getCurrentFileName().length() + getTerminalName().length() + 2;
        return SHELL_COLOR+getTerminalName()+RESET_COLOR + " " + DIRECTORY_COLOR + getCurrentFileName() + RESET_COLOR + " ";
    }
}

std::thread TerminalPassthrough::executeCommand(std::string command) {
    addCommandToHistory(command);
    return std::thread([this, command = std::move(command)]() {
        try {
            std::string result;
            
            // Check if the command matches an alias and replace it
            std::string processedCommand = command;
            std::istringstream iss(command);
            std::string commandName;
            iss >> commandName;
            
            if (!commandName.empty() && aliases.find(commandName) != aliases.end()) {
                // Get the rest of the command (arguments)
                std::string args;
                std::getline(iss >> std::ws, args);
                
                // Replace the alias with its definition and keep the arguments
                processedCommand = aliases[commandName];
                if (!args.empty()) {
                    processedCommand += " " + args;
                }
            }
            
            if (processedCommand.compare(0, 3, "cd ") == 0) {
                std::string newDir = processedCommand.substr(3);
                if (newDir == "/") {
                    currentDirectory = "/";
                } else if (newDir == "..") {
                    std::filesystem::path dir = std::filesystem::path(currentDirectory).parent_path();
                    if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
                        currentDirectory = dir.string();
                    } else {
                        result = "Cannot go up from root directory";
                        std::cerr << result << std::endl;
                        terminalCacheTerminalOutput.push_back(result);
                        return;
                    }
                } else {
                    std::filesystem::path dir = std::filesystem::path(currentDirectory) / newDir;
                    if (!std::filesystem::exists(dir)) {
                        result = "cd: " + newDir + ": No such file or directory";
                        std::cerr << result << std::endl;
                        terminalCacheTerminalOutput.push_back(result);
                        return;
                    } else if (!std::filesystem::is_directory(dir)) {
                        result = "cd: " + newDir + ": Not a directory";
                        std::cerr << result << std::endl;
                        terminalCacheTerminalOutput.push_back(result);
                        return;
                    } else if ((std::filesystem::status(dir).permissions() & std::filesystem::perms::owner_exec) == std::filesystem::perms::none) {
                        result = "cd: " + newDir + ": Permission denied";
                        std::cerr << result << std::endl;
                        terminalCacheTerminalOutput.push_back(result);
                        return;
                    } else {
                        currentDirectory = std::filesystem::canonical(dir).string();
                    }
                }
                result = "Changed directory to: " + currentDirectory;
            } else {
                result = std::system((getTerminalName() + " -c \"cd " + currentDirectory + " && " + processedCommand + " 2>&1\"").c_str());
                if(result != "0"){
                    result = "Error executing command: '" + processedCommand + "'";
                }
                terminalCacheTerminalOutput.push_back(result);
            }
        } catch (const std::exception& e) {
            std::string errorMsg = "Error executing command: '" + command + "' " + e.what();
            std::cerr << errorMsg << std::endl;
            terminalCacheTerminalOutput.push_back(std::move(errorMsg));
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
    if (commandHistoryIndex < terminalCacheUserInput.size() - 1) {
        commandHistoryIndex++;
    } else {
        commandHistoryIndex = 0;
    }
    return terminalCacheUserInput[commandHistoryIndex];
}

std::string TerminalPassthrough::getNextCommand() {
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

std::string TerminalPassthrough::getCurrentFilePath(){
    if (currentDirectory.empty()) {
        return std::filesystem::current_path().string();
    }
    return currentDirectory;
}

std::string TerminalPassthrough::getCurrentFileName(){
    std::string currentDirectory = getCurrentFilePath();
    std::string currentFileName = std::filesystem::path(currentDirectory).filename().string();
    if (currentFileName.empty()) {
        return "/";
    }
    return currentFileName;
}

bool TerminalPassthrough::isRootPath(const std::filesystem::path& path){
    return path == path.root_path();
}

void TerminalPassthrough::addCommandToHistory(const std::string& command) {
    if (command.empty()) {
        return;
    }
    if (std::find(terminalCacheUserInput.begin(), terminalCacheUserInput.end(), command) != terminalCacheUserInput.end()) {
        return;
    }
    commandHistoryIndex = terminalCacheUserInput.size();
    terminalCacheUserInput.push_back(command);
}

void TerminalPassthrough::setShellColor(const std::string& color){
    this->SHELL_COLOR = color;
}

void TerminalPassthrough::setDirectoryColor(const std::string& color){
    this->DIRECTORY_COLOR = color;
}

void TerminalPassthrough::setBranchColor(const std::string& color){
    this->BRANCH_COLOR = color;
}

void TerminalPassthrough::setGitColor(const std::string& color){
    this->GIT_COLOR = color;
}

std::string TerminalPassthrough::getShellColor() const {
    return SHELL_COLOR;
}

std::string TerminalPassthrough::getDirectoryColor() const {
    return DIRECTORY_COLOR;
}

std::string TerminalPassthrough::getBranchColor() const {
    return BRANCH_COLOR;
}

std::string TerminalPassthrough::getGitColor() const {
    return GIT_COLOR;
}

