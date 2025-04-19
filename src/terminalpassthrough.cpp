#include "include/terminalpassthrough.h"

TerminalPassthrough::TerminalPassthrough() : displayWholePath(false) {
    currentDirectory = std::filesystem::current_path().string();
    terminalCacheUserInput = std::vector<std::string>();
    terminalCacheTerminalOutput = std::vector<std::string>();
    lastGitStatusCheck = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    isGitStatusCheckRunning = false;
    //     std::string terminalID= "dtt";
    //     char buffer[256];
    //     std::string command = "ps -p " + std::to_string(getppid()) + " -o comm=";
    //     FILE* pipe = popen(command.c_str(), "r");
    //     if (pipe) {
    //         if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    //             terminalID = std::string(buffer);
    //             terminalID.erase(std::remove(terminalID.begin(), terminalID.end(), '\n'), terminalID.end());
    //             terminalID = removeSpecialCharacters(terminalID);
    //         }
    //         pclose(pipe);
    //     }
    // terminalName = terminalID;
    terminalName = "dtt";
    
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
}

std::string TerminalPassthrough::removeSpecialCharacters(const std::string& input) {
    std::string result;
    for (char c : input) {
        if (isalnum(c)) {
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
                    std::string gitStatusCmd = "sh -c \"cd " + gitDir + 
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
            
            parseAndExecuteCommand(processedCommand, result);
            terminalCacheTerminalOutput.push_back(result);
            
        } catch (const std::exception& e) {
            std::string errorMsg = "Error executing command: '" + command + "' " + e.what();
            std::cerr << errorMsg << std::endl;
            terminalCacheTerminalOutput.push_back(std::move(errorMsg));
        }
    });
}

void TerminalPassthrough::parseAndExecuteCommand(const std::string& command, std::string& result) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "cd") {
        std::string dir;
        std::getline(iss >> std::ws, dir);
        changeDirectory(dir, result);
    } 
    else if (cmd == "jobs") {
        std::stringstream jobOutput;
        updateJobStatus();
        
        if (jobs.empty()) {
            jobOutput << "No active jobs";
        } else {
            for (size_t i = 0; i < jobs.size(); i++) {
                const auto& job = jobs[i];
                jobOutput << "[" << (i+1) << "] " 
                          << (job.foreground ? "Running " : "Stopped ")
                          << job.command << " (PID: " << job.pid << ")\n";
            }
        }
        result = jobOutput.str();
    }
    else if (cmd == "fg") {
        int jobId = 0;
        iss >> jobId;
        if (jobId <= 0) jobId = 1;
        
        if (bringJobToForeground(jobId)) {
            result = "Job brought to foreground";
        } else {
            result = "No such job";
        }
    }
    else if (cmd == "bg") {
        int jobId = 0;
        iss >> jobId;
        if (jobId <= 0) jobId = 1;
        
        if (sendJobToBackground(jobId)) {
            result = "Job sent to background";
        } else {
            result = "No such job";
        }
    }
    else if (cmd == "kill") {
        int jobId = 0;
        iss >> jobId;
        
        if (killJob(jobId)) {
            result = "Job killed";
        } else {
            result = "No such job";
        }
    }
    else {
        bool background = false;
        
        std::string fullCommand = command;
        if (!fullCommand.empty() && fullCommand.back() == '&') {
            background = true;
            fullCommand.pop_back();

            size_t lastNonSpace = fullCommand.find_last_not_of(" \t");
            if (lastNonSpace != std::string::npos) {
                fullCommand = fullCommand.substr(0, lastNonSpace + 1);
            }
        }
        
        if (background) {
            pid_t pid = executeChildProcess(fullCommand, false);
            jobs.push_back(Job(pid, fullCommand, false));
            result = "Started background process [" + std::to_string(jobs.size()) + "] (PID: " + std::to_string(pid) + ")";
        } else {
            pid_t pid = executeChildProcess(fullCommand, true);
            
            updateJobStatus();
            result = "Command completed";
        }
    }
}

pid_t TerminalPassthrough::executeChildProcess(const std::string& command, bool foreground) {
    pid_t pid = fork();
    
    if (pid == -1) {
        throw std::runtime_error("Failed to fork process");
    } 
    else if (pid == 0) {
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        
        pid_t childPid = getpid();
        setpgid(childPid, childPid);

        if (foreground) {
            tcsetpgrp(STDIN_FILENO, childPid);
        }

        if (!foreground) {
            setsid();
        }

        chdir(currentDirectory.c_str());
        
        extern char **environ;
        
        execle("/bin/sh", "sh", "-c", command.c_str(), nullptr, environ);
        
        std::cerr << "Failed to execute: " << command << std::endl;
        exit(EXIT_FAILURE);
    }
    
    setpgid(pid, pid);
    
    if (foreground) {
        tcsetpgrp(STDIN_FILENO, pid);
        
        waitForForegroundJob(pid);
        
        tcsetpgrp(STDIN_FILENO, getpgid(0));
    }
    
    return pid;
}

std::string TerminalPassthrough::captureCommandOutput(const std::string& command) {
    std::array<int, 2> pipe_fd;
    if (pipe(pipe_fd.data()) == -1) {
        return "Error: Failed to create pipe";
    }
    
    struct termios term_settings;
    tcgetattr(STDIN_FILENO, &term_settings);
    
    pid_t pid = fork();
    
    if (pid == -1) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return "Error: Failed to fork process";
    } 
    else if (pid == 0) {
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_fd[1], STDERR_FILENO);
        close(pipe_fd[1]);
        
        chdir(currentDirectory.c_str());
        
        setpgid(0, 0);
        
        extern char **environ;
        
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_fd[1], STDERR_FILENO);
        
        setpgid(0, 0);
        
        execle("/bin/sh", "sh", "-c", command.c_str(), nullptr, environ);
        
        std::cerr << "Failed to execute: " << command << std::endl;
        exit(EXIT_FAILURE);
    }
    
    close(pipe_fd[1]);
    
    std::string output;
    char buffer[4096];
    ssize_t count;
    
    fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK);
    
    fd_set read_fds;
    struct timeval timeout;
    int ready;
    
    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(pipe_fd[0], &read_fds);
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        ready = select(pipe_fd[0] + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (ready == -1) {
            if (errno != EINTR) break;
        } else if (ready == 0) {
            int status;
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == pid) {
                break;
            }
        } else {
            count = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
            if (count <= 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) break;
            } else {
                buffer[count] = '\0';
                output += buffer;
            }
        }
    }
    
    close(pipe_fd[0]);
    
    int status;
    waitpid(pid, &status, 0);
    
    tcsetattr(STDIN_FILENO, TCSANOW, &term_settings);
    
    return output;
}

bool TerminalPassthrough::changeDirectory(const std::string& dir, std::string& result) {
    std::string targetDir = dir;
    
    if (targetDir.empty() || targetDir == "~") {
        const char* homeDir = getenv("HOME");
        if (homeDir) {
            targetDir = homeDir;
        } else {
            result = "Could not determine home directory";
            return false;
        }
    }
    
    if (targetDir == "/") {
        currentDirectory = "/";
    } else if (targetDir == "..") {
        std::filesystem::path dirPath = std::filesystem::path(currentDirectory).parent_path();
        if (std::filesystem::exists(dirPath) && std::filesystem::is_directory(dirPath)) {
            currentDirectory = dirPath.string();
        } else {
            result = "Cannot go up from root directory";
            return false;
        }
    } else {
        std::filesystem::path dirPath;
        
        if (targetDir[0] == '/') {
            dirPath = targetDir;
        } else {
            dirPath = std::filesystem::path(currentDirectory) / targetDir;
        }
        
        if (!std::filesystem::exists(dirPath)) {
            result = "cd: " + targetDir + ": No such file or directory";
            return false;
        }
        
        if (!std::filesystem::is_directory(dirPath)) {
            result = "cd: " + targetDir + ": Not a directory";
            return false;
        }
        
        try {
            currentDirectory = std::filesystem::canonical(dirPath).string();
        } catch (const std::filesystem::filesystem_error& e) {
            result = "cd: " + targetDir + ": " + e.what();
            return false;
        }
    }
    
    if (chdir(currentDirectory.c_str()) != 0) {
        std::string errorMsg = "cd: " + std::string(strerror(errno));
        result = errorMsg;
        return false;
    }
    
    result = "Changed directory to: " + currentDirectory;
    return true;
}

void TerminalPassthrough::waitForForegroundJob(pid_t pid) {
    struct termios term_settings;
    tcgetattr(STDIN_FILENO, &term_settings);
    
    int status;
    waitpid(pid, &status, WUNTRACED);
    
    if (WIFSTOPPED(status)) {
        for (auto& job : jobs) {
            if (job.pid == pid) {
                job.foreground = false;
                job.status = status;
                
                tcsetpgrp(STDIN_FILENO, getpgid(0));
                return;
            }
        }
        
        jobs.push_back(Job(pid, "Unknown command", false));
        jobs.back().status = status;
    }
    
    tcsetattr(STDIN_FILENO, TCSANOW, &term_settings);
}

void TerminalPassthrough::updateJobStatus() {
    for (auto it = jobs.begin(); it != jobs.end(); ) {
        int status;
        pid_t result = waitpid(it->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
        
        if (result == 0) {
            ++it;
        } else if (result == it->pid) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                it = jobs.erase(it);
            } else {
                it->status = status;
                it->foreground = false;
                ++it;
            }
        } else {
            it = jobs.erase(it);
        }
    }
}

void TerminalPassthrough::listJobs() {
    updateJobStatus();
    
    if (jobs.empty()) {
        std::cout << "No active jobs" << std::endl;
        return;
    }
    
    for (size_t i = 0; i < jobs.size(); i++) {
        const auto& job = jobs[i];
        std::cout << "[" << (i+1) << "] " 
                  << (WIFSTOPPED(job.status) ? "Stopped " : "Running ")
                  << job.command << " (PID: " << job.pid << ")" << std::endl;
    }
}

bool TerminalPassthrough::bringJobToForeground(int jobId) {
    updateJobStatus();
    
    if (jobId <= 0 || jobId > static_cast<int>(jobs.size())) {
        return false;
    }
    
    struct termios term_settings;
    tcgetattr(STDIN_FILENO, &term_settings);
    
    Job& job = jobs[jobId - 1];
    job.foreground = true;
    
    if (WIFSTOPPED(job.status)) {
        kill(-job.pid, SIGCONT);
    }
    
    tcsetpgrp(STDIN_FILENO, job.pid);
    
    waitForForegroundJob(job.pid);
    
    tcsetpgrp(STDIN_FILENO, getpgid(0));
    
    tcsetattr(STDIN_FILENO, TCSANOW, &term_settings);
    
    return true;
}

bool TerminalPassthrough::sendJobToBackground(int jobId) {
    updateJobStatus();
    
    if (jobId <= 0 || jobId > static_cast<int>(jobs.size())) {
        return false;
    }
    
    Job& job = jobs[jobId - 1];
    job.foreground = false;
    
    if (WIFSTOPPED(job.status)) {
        kill(-job.pid, SIGCONT);
    }
    
    return true;
}

bool TerminalPassthrough::killJob(int jobId) {
    updateJobStatus();
    
    if (jobId <= 0 || jobId > static_cast<int>(jobs.size())) {
        return false;
    }
    
    Job& job = jobs[jobId - 1];
    
    kill(-job.pid, SIGTERM);
    
    return true;
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

std::vector<std::string> TerminalPassthrough::getCommandHistory(size_t count) {
    std::vector<std::string> recentCommands;
    size_t historySize = terminalCacheUserInput.size();
    size_t numCommands = std::min(count, historySize);
    
    for (size_t i = 0; i < numCommands; i++) {
        size_t index = historySize - 1 - i;
        recentCommands.push_back(terminalCacheUserInput[index]);
    }
    
    return recentCommands;
}

