#include "terminalpassthrough.h"

TerminalPassthrough::TerminalPassthrough() : displayWholePath(false) {
    currentDirectory = std::filesystem::current_path().string();
    terminalCacheUserInput = std::vector<std::string>();
    terminalCacheTerminalOutput = std::vector<std::string>();
    lastGitStatusCheck = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    isGitStatusCheckRunning = false;
        std::string terminalID= "dtt";
        char buffer[256];
        std::string command = "ps -p " + std::to_string(getppid()) + " -o comm=";
        FILE* pipe = popen(command.c_str(), "r");
        if (pipe) {
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                terminalID = std::string(buffer);
                terminalID.erase(std::remove(terminalID.begin(), terminalID.end(), '\n'), terminalID.end());
                // Remove any special characters from terminal name
                terminalID = removeSpecialCharacters(terminalID);
            }
            pclose(pipe);
        }
    terminalName = terminalID;
}

// Helper function to remove special characters
std::string TerminalPassthrough::removeSpecialCharacters(const std::string& input) {
    std::string result;
    for (char c : input) {
        // Keep only alphanumeric characters, underscore, and hyphen
        if (isalnum(c) || c == '_' || c == '-') {
            result += c;
        }
    }
    return result;
}

std::string TerminalPassthrough::getTerminalName(){
    return terminalName;
}

std::vector<std::string> TerminalPassthrough::getFilesAtCurrentPath(const bool& includeHidden, const bool& fullFilePath, const bool& includeDirectories){
    std::vector<std::string> files;
    for (const auto& entry : std::filesystem::directory_iterator(getCurrentFilePath())) {
        if (includeHidden || entry.path().filename().string()[0] != '.') {
            if (includeDirectories || !std::filesystem::is_directory(entry.path())) {
                std::string fileName = entry.path().filename().string();
                if (fullFilePath) {
                    fileName = entry.path().string();
                } else {
                    fileName = entry.path().filename().string();
                }
            }
        }
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

            std::string statusSymbols = "";
            std::string gitDir = currentPath.string();
            bool isCleanRepo = true;
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastGitStatusCheck).count();
            
            if ((elapsed > 10 || cachedGitDir != gitDir) && !isGitStatusCheckRunning) {
                isGitStatusCheckRunning = true;
                std::thread statusThread([this, gitDir]() {
                    std::string gitStatusCmd = getTerminalName() + 
                        " -c \"cd " + gitDir + 
                        " && git status --porcelain -z\"";
                    
                    FILE* statusPipe = popen(gitStatusCmd.c_str(), "r");
                    char statusBuffer[1024];
                    std::string statusOutput = "";
                    
                    if (statusPipe) {
                        while (fgets(statusBuffer, sizeof(statusBuffer), statusPipe) != nullptr) {
                            statusOutput += statusBuffer;
                        }
                        pclose(statusPipe);
                        
                        bool isClean = statusOutput.empty();
                        std::string symbols = "";
                        
                        if (!isClean) {
                            symbols = "*";
                        }
                        
                        std::lock_guard<std::mutex> lock(gitStatusMutex);
                        cachedGitDir = gitDir;
                        cachedStatusSymbols = symbols;
                        cachedIsCleanRepo = isClean;
                        lastGitStatusCheck = std::chrono::steady_clock::now();
                        isGitStatusCheckRunning = false;
                    } else {
                        std::lock_guard<std::mutex> lock(gitStatusMutex);
                        isGitStatusCheckRunning = false;
                    }
                });
                statusThread.detach();
                
                statusSymbols = cachedStatusSymbols;
                isCleanRepo = cachedIsCleanRepo;
            } else {
                std::lock_guard<std::mutex> lock(gitStatusMutex);
                statusSymbols = cachedStatusSymbols;
                isCleanRepo = cachedIsCleanRepo;
            }
            
            std::string repoName = displayWholePath ? getCurrentFilePath() : getCurrentFileName();
            std::string statusInfo;
            
            if (isCleanRepo) {
                statusInfo = " ✓";
            } else {
                statusInfo = " " + statusSymbols;
            }
            
            gitInfoLength = repoName.length() + branchName.length() + statusInfo.length() + 9;
            gitInfo = GIT_COLOR + repoName + RESET_COLOR + DIRECTORY_COLOR + " git:(" + RESET_COLOR + BRANCH_COLOR + branchName + RESET_COLOR;
            
            if (isCleanRepo) {
                gitInfo += DIRECTORY_COLOR + statusInfo + RESET_COLOR;
            } else if (!statusSymbols.empty()) {
                gitInfo += DIRECTORY_COLOR + statusInfo + RESET_COLOR;
            }
            
            gitInfo += DIRECTORY_COLOR + ")" + RESET_COLOR;
        } catch (const std::exception& e) {
            std::cerr << "Error reading git HEAD file: " << e.what() << std::endl;
        }
        terminalCurrentPositionRawLength = getTerminalName().length() + 2 + gitInfoLength;
        return SHELL_COLOR + getTerminalName() + RESET_COLOR + " " + gitInfo + " ";
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
            
            std::string processedCommand = command;
            std::istringstream iss(command);
            std::string commandName;
            iss >> commandName;
            
            if (commandName == "cd") {
                std::string dirArg;
                iss >> dirArg;
                
                if (!dirArg.empty() && aliases.find(dirArg) != aliases.end()) {
                    std::string aliasDef = aliases[dirArg];
                    std::filesystem::path potentialDir = std::filesystem::path(currentDirectory) / aliasDef;
                    
                    if (std::filesystem::exists(potentialDir) && std::filesystem::is_directory(potentialDir)) {
                        std::string remainingArgs;
                        std::getline(iss >> std::ws, remainingArgs);
                        
                        processedCommand = "cd " + aliasDef;
                        if (!remainingArgs.empty()) {
                            processedCommand += " " + remainingArgs;
                        }
                    }
                }
            }
            else if (!commandName.empty() && aliases.find(commandName) != aliases.end()) {
                std::string args;
                std::getline(iss >> std::ws, args);
                
                processedCommand = aliases[commandName];
                if (!args.empty()) {
                    processedCommand += " " + args;
                }
            }
            
            if (processedCommand.compare(0, 3, "cd ") == 0) {
                std::string cdCommand = processedCommand;
                std::string subsequentCommand;
                
                // Check for && and split the command
                size_t andPos = processedCommand.find("&&");
                if (andPos != std::string::npos) {
                    cdCommand = processedCommand.substr(0, andPos);
                    subsequentCommand = processedCommand.substr(andPos + 2);
                    // Trim whitespace
                    cdCommand = cdCommand.substr(0, cdCommand.find_last_not_of(" \t") + 1);
                    subsequentCommand = subsequentCommand.substr(subsequentCommand.find_first_not_of(" \t"));
                }
                
                std::string newDir = cdCommand.substr(3);
                bool cdSuccess = true;
                
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
                        cdSuccess = false;
                    }
                } else {
                    std::filesystem::path dir = std::filesystem::path(currentDirectory) / newDir;
                    if (!std::filesystem::exists(dir)) {
                        result = "cd: " + newDir + ": No such file or directory";
                        std::cerr << result << std::endl;
                        terminalCacheTerminalOutput.push_back(result);
                        cdSuccess = false;
                    } else if (!std::filesystem::is_directory(dir)) {
                        result = "cd: " + newDir + ": Not a directory";
                        std::cerr << result << std::endl;
                        terminalCacheTerminalOutput.push_back(result);
                        cdSuccess = false;
                    } else if ((std::filesystem::status(dir).permissions() & std::filesystem::perms::owner_exec) == std::filesystem::perms::none) {
                        result = "cd: " + newDir + ": Permission denied";
                        std::cerr << result << std::endl;
                        terminalCacheTerminalOutput.push_back(result);
                        cdSuccess = false;
                    } else {
                        currentDirectory = std::filesystem::canonical(dir).string();
                    }
                }
                
                if (cdSuccess) {
                    result = "Changed directory to: " + currentDirectory;
                    
                    // Execute subsequent command if it exists
                    if (!subsequentCommand.empty()) {
                        int cmdResult = std::system((getTerminalName() + " -c \"cd " + currentDirectory + " && " + subsequentCommand + " 2>&1\"").c_str());
                        if (cmdResult != 0) {
                            result += "\nError executing subsequent command: '" + subsequentCommand + "'";
                        }
                    }
                }
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

