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
            gitInfo = "\033[1;32m" + repoName + RESET_COLOR+DIRECTORY_COLOR+" git:("+RESET_COLOR+BRANCH_COLOR + branchName +RESET_COLOR+DIRECTORY_COLOR+ ")"+RESET_COLOR;
        } catch (const std::exception& e) {
            std::cerr << "Error reading git HEAD file: " << e.what() << std::endl;
        }
        terminalCurrentPositionRawLength = getTerminalName().length() + 2 + gitInfoLength;
        return SHELL_COLOR+getTerminalName()+RESET_COLOR + ": " + gitInfo + " ";
    }
    if (displayWholePath) {
        terminalCurrentPositionRawLength = getCurrentFilePath().length() + getTerminalName().length() + 2;
        return SHELL_COLOR+getTerminalName()+RESET_COLOR + ": \033[1;34m" + getCurrentFilePath() + "\033[0m" + " ";
    } else {
        terminalCurrentPositionRawLength = getCurrentFileName().length() + getTerminalName().length() + 2;
        return SHELL_COLOR+getTerminalName()+RESET_COLOR + ": \033[1;34m" + getCurrentFileName() + "\033[0m" + " ";
    }
}

std::thread TerminalPassthrough::executeCommand(std::string command) {
    addCommandToHistory(command);
    return std::thread([this, command]() {
        try {
            std::string expandedCommand = expandEnvVars(command);
            std::string result;
            if (expandedCommand.rfind("cd ", 0) == 0) {
                std::string newDir = expandedCommand.substr(3);
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
                
                std::string envVarSetup;
                for (const auto& [name, value] : envVars) {
                    if (getTerminalName() == "cmd") {
                        envVarSetup += "set \"" + name + "=" + value + "\" && ";
                    } else {
                        envVarSetup += "export " + name + "=\"" + value + "\"; ";
                    }
                }
                
                if (getTerminalName() == "cmd") {
                    fullCommand = envVarSetup + "cd " + currentDirectory + " && " + expandedCommand + " 2>&1";
                } else {
                    fullCommand = getTerminalName() + " -c \"" + envVarSetup + "cd " + currentDirectory + " && " + expandedCommand + " 2>&1\"";
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
    commandHistoryIndex = terminalCacheUserInput.size();
    terminalCacheUserInput.push_back(command);
}

void TerminalPassthrough::setEnvVar(const std::string& name, const std::string& value) {
    envVars[name] = value;
}

std::string TerminalPassthrough::getEnvVar(const std::string& name) const {
    auto it = envVars.find(name);
    if (it != envVars.end()) {
        return it->second;
    }
    return "";
}

bool TerminalPassthrough::hasEnvVar(const std::string& name) const {
    return envVars.find(name) != envVars.end();
}

void TerminalPassthrough::removeEnvVar(const std::string& name) {
    envVars.erase(name);
}

std::map<std::string, std::string> TerminalPassthrough::getAllEnvVars() const {
    return envVars;
}

std::string TerminalPassthrough::expandEnvVars(const std::string& command) const {
    std::string result = command;
    size_t pos = 0;
    
    // Find all occurrences of $VAR or ${VAR}
    while ((pos = result.find('$', pos)) != std::string::npos) {
        // Skip if it's escaped with \\$
        if (pos > 0 && result[pos-1] == '\\') {
            pos++;
            continue;
        }
        
        size_t varStart = pos + 1;
        size_t varEnd;
        bool hasBraces = false;
        
        // Check if it's ${VAR} format
        if (varStart < result.size() && result[varStart] == '{') {
            hasBraces = true;
            varStart++;
            varEnd = result.find('}', varStart);
            if (varEnd == std::string::npos) {
                // Unmatched brace, just continue
                pos++;
                continue;
            }
        } else {
            // It's $VAR format
            varEnd = varStart;
            while (varEnd < result.size() && (isalnum(result[varEnd]) || result[varEnd] == '_')) {
                varEnd++;
            }
        }
        
        if (varStart == varEnd) {
            pos++;
            continue;
        }
        
        std::string varName = result.substr(varStart, varEnd - varStart);
        std::string replacement = getEnvVar(varName);
        
        size_t replaceStart = pos;
        size_t replaceLength = (hasBraces ? varEnd + 1 : varEnd) - pos;
        
        result.replace(replaceStart, replaceLength, replacement);
        pos = replaceStart + replacement.length();
    }
    
    return result;
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

std::string TerminalPassthrough::getShellColor() const {
    return SHELL_COLOR;
}

std::string TerminalPassthrough::getDirectoryColor() const {
    return DIRECTORY_COLOR;
}

std::string TerminalPassthrough::getBranchColor() const {
    return BRANCH_COLOR;
}

