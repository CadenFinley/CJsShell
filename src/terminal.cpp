#include "include/terminal.h"

Terminal::Terminal() : displayWholePath(false) {
    currentDirectory = std::filesystem::current_path().string();
    terminalCacheUserInput = std::vector<std::string>();
    terminalCacheTerminalOutput = std::vector<std::string>();
    lastGitStatusCheck = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    isGitStatusCheckRunning = false;
    shouldTerminate = false;
    terminalName = "cjsh";
    
    
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    
    
    sigset_t block_mask;
    sigfillset(&block_mask);
    
    
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sa.sa_mask = block_mask;
    sigaction(SIGTTOU, &sa, nullptr);
    
    
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sa.sa_mask = block_mask;
    sigaction(SIGTTIN, &sa, nullptr);
    
    
    sa.sa_sigaction = &Terminal::signalHandlerWrapper;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sa.sa_mask = block_mask;
    sigaction(SIGCHLD, &sa, nullptr);
    
    
    tcgetattr(STDIN_FILENO, &original_termios);
    terminal_state_saved = true;
}

Terminal::~Terminal() {
    shouldTerminate = true;
    
    
    terminateAllChildProcesses();
    
    
    if (terminal_state_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}


void Terminal::signalHandlerWrapper(int signum, siginfo_t* info, void* context) {
    
    if (signum == SIGCHLD) {
        pid_t child_pid;
        int status;
        
        
        while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
            
            
            
        }
    }
}


void Terminal::saveTerminalState() {
    if (isatty(STDIN_FILENO)) {
        if (tcgetattr(STDIN_FILENO, &original_termios) == 0) {
            terminal_state_saved = true;
        }
    }
}


void Terminal::restoreTerminalState() {
    if (terminal_state_saved) {
        
        tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
    }
}

std::string Terminal::getTerminalName(){
    return terminalName;
}

std::vector<std::string> Terminal::getFilesAtCurrentPath(const bool& includeHidden, const bool& fullFilePath, const bool& includeDirectories){
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
                files.push_back(fileName);
            }
        }
    }
    return files;
}

void Terminal::setDisplayWholePath(bool displayWholePath){
    this->displayWholePath = displayWholePath;
}

void Terminal::setAliases(const std::map<std::string, std::string>& aliases){
    this->aliases = aliases;
}

std::string Terminal::getFullPathOfFile(const std::string& file){
    if (std::filesystem::exists (std::filesystem::path(getCurrentFilePath()) / file)) {
        return (std::filesystem::path(getCurrentFilePath()) / file).string();
    }
    return "";
}

void Terminal::printCurrentTerminalPosition(){
    std::cout << returnCurrentTerminalPosition();
}

int Terminal::getTerminalCurrentPositionRawLength(){
    return terminalCurrentPositionRawLength;
}

std::string Terminal::returnCurrentTerminalPosition(){
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
            
            if ((elapsed > 30 || cachedGitDir != gitDir) && !isGitStatusCheckRunning) {
                isGitStatusCheckRunning = true;
                if (cachedGitDir != gitDir) {
                    std::thread statusThread([this, gitDir]() {
                        if (shouldTerminate) {
                            std::lock_guard<std::mutex> lock(gitStatusMutex);
                            isGitStatusCheckRunning = false;
                            return;
                        }
                        
                        std::string gitStatusCmd = "cjsh -c \"cd " + gitDir + 
                            " && git status --porcelain | head -1\"";
                        
                        FILE* statusPipe = popen(gitStatusCmd.c_str(), "r");
                        char statusBuffer[1024];
                        std::string statusOutput = "";
                        
                        if (statusPipe) {
                            while (fgets(statusBuffer, sizeof(statusBuffer), statusPipe) != nullptr) {
                                statusOutput += statusBuffer;
                                if (shouldTerminate) {
                                    pclose(statusPipe);
                                    std::lock_guard<std::mutex> lock(gitStatusMutex);
                                    isGitStatusCheckRunning = false;
                                    return;
                                }
                            }
                            pclose(statusPipe);
                            
                            if (shouldTerminate) {
                                std::lock_guard<std::mutex> lock(gitStatusMutex);
                                isGitStatusCheckRunning = false;
                                return;
                            }
                            
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
                } else {
                    std::thread statusThread([this, gitDir]() {
                        if (shouldTerminate) {
                            std::lock_guard<std::mutex> lock(gitStatusMutex);
                            isGitStatusCheckRunning = false;
                            return;
                        }
                        
                        std::string gitStatusCmd = "cjsh -c \"cd " + gitDir + 
                            " && git status --porcelain | head -1\"";
                        
                        FILE* statusPipe = popen(gitStatusCmd.c_str(), "r");
                        char statusBuffer[1024];
                        std::string statusOutput = "";
                        
                        if (statusPipe) {
                            while (fgets(statusBuffer, sizeof(statusBuffer), statusPipe) != nullptr) {
                                statusOutput += statusBuffer;
                                if (shouldTerminate) {
                                    pclose(statusPipe);
                                    std::lock_guard<std::mutex> lock(gitStatusMutex);
                                    isGitStatusCheckRunning = false;
                                    return;
                                }
                            }
                            pclose(statusPipe);
                            
                            if (shouldTerminate) {
                                std::lock_guard<std::mutex> lock(gitStatusMutex);
                                isGitStatusCheckRunning = false;
                                return;
                            }
                            
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
                }
                
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
        
        return SHELL_COLOR + getTerminalName() + RESET_COLOR + " " + gitInfo;
    }

    
    if (!PROMPT_FORMAT.empty()) {
        return expandPromptFormat(PROMPT_FORMAT);
    }
    
    
    if (displayWholePath) {
        return SHELL_COLOR+getTerminalName()+RESET_COLOR + " " + DIRECTORY_COLOR + getCurrentFilePath() + RESET_COLOR;
    } else {
        return SHELL_COLOR+getTerminalName()+RESET_COLOR + " " + DIRECTORY_COLOR + getCurrentFileName() + RESET_COLOR;
    }
}

std::string Terminal::expandPromptFormat(const std::string& format) {
    std::string result = format;
    
    
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    const char* username = getenv("USER");
    if (!username) username = getenv("LOGNAME");
    if (!username) username = "user";
    
    
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    char timeBuffer[20];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", std::localtime(&currentTime));
    
    
    result = std::regex_replace(result, std::regex("\\\\W"), DIRECTORY_COLOR + getCurrentFilePath() + RESET_COLOR);
    result = std::regex_replace(result, std::regex("\\\\w"), DIRECTORY_COLOR + getCurrentFileName() + RESET_COLOR);
    result = std::regex_replace(result, std::regex("\\\\u"), SHELL_COLOR + std::string(username) + RESET_COLOR);
    result = std::regex_replace(result, std::regex("\\\\h"), BRANCH_COLOR + std::string(hostname) + RESET_COLOR);
    result = std::regex_replace(result, std::regex("\\\\t"), GIT_COLOR + std::string(timeBuffer) + RESET_COLOR);
    result = std::regex_replace(result, std::regex("\\\\\\$"), "$");
    
    
    result = std::regex_replace(result, std::regex("\\$\\{SHELL_COLOR\\}"), SHELL_COLOR);
    result = std::regex_replace(result, std::regex("\\$\\{DIRECTORY_COLOR\\}"), DIRECTORY_COLOR);
    result = std::regex_replace(result, std::regex("\\$\\{BRANCH_COLOR\\}"), BRANCH_COLOR);
    result = std::regex_replace(result, std::regex("\\$\\{GIT_COLOR\\}"), GIT_COLOR);
    result = std::regex_replace(result, std::regex("\\$\\{RESET_COLOR\\}"), RESET_COLOR);
    
    
    result = std::regex_replace(result, std::regex("cjsh"), SHELL_COLOR + getTerminalName() + RESET_COLOR);
    
    return result;
}

std::string Terminal::expandAliases(const std::string& command) {
    std::istringstream iss(command);
    std::string commandName;
    iss >> commandName;
    
    if (commandName.empty()) {
        return command;
    }
    
    auto aliasIt = aliases.find(commandName);
    if (aliasIt != aliases.end()) {
        std::string remaining;
        std::getline(iss >> std::ws, remaining);
        
        std::string aliasValue = aliasIt->second;
        
        if (aliasValue.find("$") != std::string::npos) {
            std::vector<std::string> args;
            std::istringstream argStream(remaining);
            std::string arg;
            while (argStream >> arg) {
                args.push_back(arg);
            }
            
            size_t pos = 0;
            while ((pos = aliasValue.find('$', pos)) != std::string::npos) {
                if (pos + 1 < aliasValue.size() && isdigit(aliasValue[pos + 1])) {
                    int argNum = aliasValue[pos + 1] - '0';
                    if (argNum >= 0 && argNum < static_cast<int>(args.size())) {
                        aliasValue.replace(pos, 2, args[argNum]);
                    } else {
                        aliasValue.replace(pos, 2, "");
                    }
                } else {
                    pos++;
                }
            }
            
            return aliasValue;
        }
        
        return aliasValue + (remaining.empty() ? "" : " " + remaining);
    }
    
    return command;
}

std::string Terminal::processCommandSubstitution(const std::string& command) {
    std::string result = command;
    std::string::size_type pos = 0;
    
    while ((pos = result.find("$(", pos)) != std::string::npos) {
        int depth = 1;
        std::string::size_type end = pos + 2;
        
        while (end < result.length() && depth > 0) {
            if (result[end] == '(') depth++;
            else if (result[end] == ')') depth--;
            end++;
        }
        
        if (depth == 0) {
            std::string subCommand = result.substr(pos + 2, end - pos - 3);
            
            FILE* pipe = popen(subCommand.c_str(), "r");
            if (!pipe) {
                std::cerr << "Error executing command substitution" << std::endl;
                pos = end;
                continue;
            }
            
            std::string output;
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
            }
            pclose(pipe);
            
            while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
                output.pop_back();
            }
            
            result.replace(pos, end - pos, output);
            pos += output.length();
        } else {
            pos = end;
        }
    }
    
    pos = 0;
    while ((pos = result.find('`', pos)) != std::string::npos) {
        std::string::size_type end = result.find('`', pos + 1);
        if (end != std::string::npos) {
            std::string subCommand = result.substr(pos + 1, end - pos - 1);
            
            FILE* pipe = popen(subCommand.c_str(), "r");
            if (!pipe) {
                std::cerr << "Error executing command substitution" << std::endl;
                pos = end + 1;
                continue;
            }
            
            std::string output;
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                output += buffer;
            }
            pclose(pipe);
            
            while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
                output.pop_back();
            }
            
            result.replace(pos, end - pos + 1, output);
            pos += output.length();
        } else {
            break;
        }
    }
    
    return result;
}

std::thread Terminal::executeCommand(std::string command) {
    addCommandToHistory(command);
    return std::thread([this, command = std::move(command)]() {
        try {
            std::string result;
            
            std::string processedCommand = expandAliases(command);
            
            parseAndExecuteCommand(processedCommand, result);
            terminalCacheTerminalOutput.push_back(result);
            
        } catch (const std::exception& e) {
            std::string errorMsg = "Error executing command: '" + command + "' " + e.what();
            std::cerr << errorMsg << std::endl;
            terminalCacheTerminalOutput.push_back(std::move(errorMsg));
        }
    });
}

bool Terminal::executeCommandSync(const std::string& command) {
    addCommandToHistory(command);
    std::string result;
    bool success = true;
    
    try {
        std::string processedCommand = expandAliases(command);
        
        success = parseAndExecuteCommand(processedCommand, result);
        terminalCacheTerminalOutput.push_back(result);
        
    } catch (const std::exception& e) {
        std::string errorMsg = "Error executing command: '" + command + "' " + e.what();
        std::cerr << errorMsg << std::endl;
        terminalCacheTerminalOutput.push_back(std::move(errorMsg));
        return false;
    }
    
    return success;
}

bool Terminal::parseAndExecuteCommand(const std::string& command, std::string& result) {
    std::string processedCmd = processCommandSubstitution(command);
    
    std::vector<std::string> semicolonCommands;
    std::string tempCmd = processedCmd;
    
    bool inQuotes = false;
    char quoteChar = 0;
    std::string currentCommand;
    
    for (size_t i = 0; i < tempCmd.length(); i++) {
        char c = tempCmd[i];
        
        if ((c == '"' || c == '\'') && (i == 0 || tempCmd[i-1] != '\\')) {
            if (!inQuotes) {
                inQuotes = true;
                quoteChar = c;
                currentCommand += c;
            } else if (c == quoteChar) {
                inQuotes = false;
                quoteChar = 0;
                currentCommand += c;
            } else {
                currentCommand += c;
            }
        }
        else if (c == ';' && !inQuotes) {
            if (!currentCommand.empty()) {
                size_t lastNonSpace = currentCommand.find_last_not_of(" \t");
                if (lastNonSpace != std::string::npos) {
                    currentCommand = currentCommand.substr(0, lastNonSpace + 1);
                }
                semicolonCommands.push_back(currentCommand);
                currentCommand.clear();
            }
        } else {
            currentCommand += c;
        }
    }

    if (!currentCommand.empty()) {
        size_t lastNonSpace = currentCommand.find_last_not_of(" \t");
        if (lastNonSpace != std::string::npos) {
            currentCommand = currentCommand.substr(0, lastNonSpace + 1);
        }
        semicolonCommands.push_back(currentCommand);
    }

    if (semicolonCommands.empty()) {
        semicolonCommands.push_back(tempCmd);
    }
    
    std::string commandResults;
    bool overall_success = true;
    
    std::map<std::string, std::string> globalEnvVars;
    
    for (const auto& semicolonCmd : semicolonCommands) {
        std::string remainingCommand = semicolonCmd;
        bool success = true;
        std::string partialResults;
        
        std::map<std::string, std::string> localEnvVars;
        std::vector<std::string> cmdArgs = parseCommandIntoArgs(remainingCommand);
        size_t envVarEnd = 0;
        
        while (envVarEnd < cmdArgs.size() && 
               cmdArgs[envVarEnd].find('=') != std::string::npos && 
               isalpha(cmdArgs[envVarEnd][0])) {
            size_t eqPos = cmdArgs[envVarEnd].find('=');
            localEnvVars[cmdArgs[envVarEnd].substr(0, eqPos)] = cmdArgs[envVarEnd].substr(eqPos + 1);
            envVarEnd++;
        }
        
        if (envVarEnd > 0) {
            remainingCommand.clear();
            for (size_t i = envVarEnd; i < cmdArgs.size(); i++) {
                remainingCommand += cmdArgs[i];
                if (i + 1 < cmdArgs.size()) {
                    remainingCommand += " ";
                }
            }
        }
        
        for (const auto& env : localEnvVars) {
            setenv(env.first.c_str(), env.second.c_str(), 1);
            globalEnvVars[env.first] = env.second;
        }
        
        while (!remainingCommand.empty() && success) {
            size_t andPos = remainingCommand.find("&&");
            std::string currentCmd;
            
            if (andPos != std::string::npos) {
                currentCmd = remainingCommand.substr(0, andPos);
                size_t lastNonSpace = currentCmd.find_last_not_of(" \t");
                if (lastNonSpace != std::string::npos) {
                    currentCmd = currentCmd.substr(0, lastNonSpace + 1);
                }
                remainingCommand = remainingCommand.substr(andPos + 2);
                size_t firstNonSpace = remainingCommand.find_first_not_of(" \t");
                if (firstNonSpace != std::string::npos) {
                    remainingCommand = remainingCommand.substr(firstNonSpace);
                } else {
                    remainingCommand.clear();
                }
            } else {
                currentCmd = remainingCommand;
                remainingCommand.clear();
            }
            
            if (currentCmd.find('|') != std::string::npos) {
                std::vector<std::string> pipeCommands = splitByPipes(currentCmd);
                if (!pipeCommands.empty()) {
                    std::string pipeResult;
                    success = executeCommandWithPipes(pipeCommands, pipeResult);
                    
                    if (!partialResults.empty()) {
                        partialResults += "\n";
                    }
                    partialResults += pipeResult;
                    
                    if (!success) {
                        std::cerr << "cjsh: " << pipeResult << std::endl;
                    }
                } else {
                    success = executeIndividualCommand(currentCmd, partialResults);
                    if (!success) {
                        std::cerr << "cjsh: " << partialResults << std::endl;
                    }
                }
            } else {
                std::string singleCmdResult;
                success = executeIndividualCommand(currentCmd, singleCmdResult);
                
                if (!partialResults.empty()) {
                    partialResults += "\n";
                }
                partialResults += singleCmdResult;
                
                if (!success) {
                    std::cerr << "cjsh: " << singleCmdResult << std::endl;
                }
            }
            
            if (!success) {
                overall_success = false;
                break;
            }
        }
        
        if (!commandResults.empty()) {
            commandResults += "\n";
        }
        commandResults += partialResults;
    }
    for (const auto& env : globalEnvVars) {
        if (getenv(env.first.c_str()) != nullptr) {
            // nothing
        }
    }
    
    result = commandResults;
    return overall_success;
}

std::vector<std::string> Terminal::splitByPipes(const std::string& command) {
    std::vector<std::string> result;
    std::string currentCommand;
    bool inQuotes = false;
    char quoteChar = 0;
    
    for (size_t i = 0; i < command.length(); i++) {
        char c = command[i];
        
        if ((c == '"' || c == '\'') && (i == 0 || command[i-1] != '\\')) {
            if (!inQuotes) {
                inQuotes = true;
                quoteChar = c;
                currentCommand += c;
            } else if (c == quoteChar) {
                inQuotes = false;
                quoteChar = 0;
                currentCommand += c;
            } else {
                currentCommand += c;
            }
        } else if (c == '|' && !inQuotes) {
            if (!currentCommand.empty()) {
                size_t lastNonSpace = currentCommand.find_last_not_of(" \t");
                if (lastNonSpace != std::string::npos) {
                    currentCommand = currentCommand.substr(0, lastNonSpace + 1);
                }
                result.push_back(currentCommand);
                currentCommand.clear();
            }
        } else {
            currentCommand += c;
        }
    }
    
    if (!currentCommand.empty()) {
        size_t lastNonSpace = currentCommand.find_last_not_of(" \t");
        if (lastNonSpace != std::string::npos) {
            currentCommand = currentCommand.substr(0, lastNonSpace + 1);
        }
        result.push_back(currentCommand);
    }
    
    return result;
}

bool Terminal::executeCommandWithPipes(const std::vector<std::string>& commands, std::string& result) {
    if (commands.empty()) {
        result = "Error: No commands to pipe";
        return false;
    }
    
    if (commands.size() == 1) {
        return executeIndividualCommand(commands[0], result);
    }
    
    int numCommands = commands.size();
    std::vector<int> pipefds(2 * (numCommands - 1));
    
    for (int i = 0; i < numCommands - 1; i++) {
        if (pipe(pipefds.data() + i * 2) < 0) {
            result = "Error creating pipe: " + std::string(strerror(errno));
            return false;
        }
    }
    
    std::vector<pid_t> pids(numCommands);
    
    for (int i = 0; i < numCommands; i++) {
        pids[i] = fork();
        
        if (pids[i] == -1) {
            result = "Error forking process for pipe command: " + std::string(strerror(errno));
            return false;
        }
        
        if (pids[i] == 0) {
            setpgid(0, 0);
            
            if (i > 0) {
                dup2(pipefds[(i-1)*2], STDIN_FILENO);
            }
            
            if (i < numCommands - 1) {
                dup2(pipefds[i*2+1], STDOUT_FILENO);
            }
            
            for (int j = 0; j < 2 * (numCommands - 1); j++) {
                close(pipefds[j]);
            }
            
            std::vector<std::string> args = parseCommandIntoArgs(commands[i]);
            std::vector<RedirectionInfo> redirections;
            
            if (handleRedirection(commands[i], args, redirections)) {
                std::vector<int> savedFds;
                if (setupRedirection(redirections, savedFds)) {
                    std::string executable = findExecutableInPath(args[0]);
                    
                    if (!executable.empty()) {
                        std::vector<std::string> expandedArgs = expandWildcardsInArgs(args);
                        
                        std::vector<char*> argv;
                        for (auto& arg : expandedArgs) {
                            argv.push_back(arg.data());
                        }
                        argv.push_back(nullptr);
                        
                        execvp(executable.c_str(), argv.data());
                    }
                    
                    std::cerr << "cjsh: command not found: " << args[0] << std::endl;
                    exit(EXIT_FAILURE);
                }
            }
            
            exit(EXIT_FAILURE);
        }
    }
    
    for (int j = 0; j < 2 * (numCommands - 1); j++) {
        close(pipefds[j]);
    }
    
    int status;
    bool success = true;
    std::string output;
    std::string errorMsg;
    
    for (int i = 0; i < numCommands; i++) {
        waitpid(pids[i], &status, 0);
        
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            success = false;
            if (WIFEXITED(status)) {
                errorMsg += "Command '" + commands[i] + "' failed with exit code " + std::to_string(WEXITSTATUS(status)) + ". ";
            } else if (WIFSIGNALED(status)) {
                errorMsg += "Command '" + commands[i] + "' terminated by signal " + std::to_string(WTERMSIG(status)) + ". ";
            }
        }
    }
    
    if (success) {
        result = "Piped commands completed successfully";
    } else {
        result = "Pipe execution failed: " + errorMsg;
    }
    
    return success;
}

bool Terminal::handleRedirection(const std::string& command, std::vector<std::string>& args, 
                                           std::vector<RedirectionInfo>& redirections) {
    std::vector<std::string> cleanArgs;
    
    for (size_t i = 0; i < args.size(); i++) {
        const std::string& arg = args[i];
        
        if (arg == ">") {
            if (i + 1 < args.size()) {
                RedirectionInfo redir;
                redir.type = 1;
                redir.file = args[i + 1];
                redirections.push_back(redir);
                i++;
            } else {
                std::cerr << "Error: Missing filename after > redirection" << std::endl;
                return false;
            }
        } else if (arg == ">>") {
            if (i + 1 < args.size()) {
                RedirectionInfo redir;
                redir.type = 2;
                redir.file = args[i + 1];
                redirections.push_back(redir);
                i++;
            } else {
                std::cerr << "Error: Missing filename after >> redirection" << std::endl;
                return false;
            }
        } else if (arg == "<") {
            if (i + 1 < args.size()) {
                RedirectionInfo redir;
                redir.type = 3;
                redir.file = args[i + 1];
                redirections.push_back(redir);
                i++;
            } else {
                std::cerr << "Error: Missing filename after < redirection" << std::endl;
                return false;
            }
        } else if (arg == "2>") {
            if (i + 1 < args.size()) {
                RedirectionInfo redir;
                redir.type = 4;
                redir.file = args[i + 1];
                redirections.push_back(redir);
                i++;
            } else {
                std::cerr << "Error: Missing filename after 2> redirection" << std::endl;
                return false;
            }
        } else {
            cleanArgs.push_back(arg);
        }
    }
    
    args = cleanArgs;
    return true;
}

bool Terminal::setupRedirection(const std::vector<RedirectionInfo>& redirections, 
                                         std::vector<int>& savedFds) {
    for (const auto& redir : redirections) {
        int flags, fd;
        
        switch (redir.type) {
            case 1:
                savedFds.push_back(dup(STDOUT_FILENO));
                flags = O_WRONLY | O_CREAT | O_TRUNC;
                fd = open(redir.file.c_str(), flags, 0666);
                if (fd == -1) {
                    std::cerr << "Error opening file for output redirection: " << redir.file << ": " << strerror(errno) << std::endl;
                    return false;
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                break;
                
            case 2:
                savedFds.push_back(dup(STDOUT_FILENO));
                flags = O_WRONLY | O_CREAT | O_APPEND;
                fd = open(redir.file.c_str(), flags, 0666);
                if (fd == -1) {
                    std::cerr << "Error opening file for append redirection: " << redir.file << ": " << strerror(errno) << std::endl;
                    return false;
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                break;
                
            case 3:
                savedFds.push_back(dup(STDIN_FILENO));
                flags = O_RDONLY;
                fd = open(redir.file.c_str(), flags);
                if (fd == -1) {
                    std::cerr << "Error opening file for input redirection: " << redir.file << ": " << strerror(errno) << std::endl;
                    return false;
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
                break;
                
            case 4:
                savedFds.push_back(dup(STDERR_FILENO));
                flags = O_WRONLY | O_CREAT | O_TRUNC;
                fd = open(redir.file.c_str(), flags, 0666);
                if (fd == -1) {
                    std::cerr << "Error opening file for error redirection: " << redir.file << ": " << strerror(errno) << std::endl;
                    return false;
                }
                dup2(fd, STDERR_FILENO);
                close(fd);
                break;
        }
    }
    
    return true;
}

void Terminal::restoreRedirection(const std::vector<int>& savedFds) {
    for (int fd : savedFds) {
        if (fd >= 0) {
            close(fd);
        }
    }
}

bool Terminal::executeIndividualCommand(const std::string& command, std::string& result) {
    std::map<std::string, std::string> cmdEnvs;
    std::vector<std::string> splitArgs = parseCommandIntoArgs(command);
    size_t idx = 0;
    while (idx < splitArgs.size()
           && splitArgs[idx].find('=') != std::string::npos
           && std::isalpha(splitArgs[idx][0])) {
        auto pos = splitArgs[idx].find('=');
        cmdEnvs[splitArgs[idx].substr(0, pos)] = splitArgs[idx].substr(pos + 1);
        idx++;
    }
    if (idx > 0 && idx == splitArgs.size()) {
        for (auto& kv : cmdEnvs) {
            setenv(kv.first.c_str(), kv.second.c_str(), 1);
        }
        result.clear();
        return true;
    }
    std::string fullCommand = command;
    if (idx > 0) {
        fullCommand.clear();
        for (size_t i = idx; i < splitArgs.size(); i++) {
            fullCommand += splitArgs[i];
            if (i + 1 < splitArgs.size()) fullCommand += " ";
        }
    }

    std::istringstream iss(fullCommand);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "cd") {
        std::string dir;
        std::getline(iss >> std::ws, dir);
        return changeDirectory(dir, result);
    } 
    else if (cmd == "export") {
        std::string envLine;
        std::getline(iss >> std::ws, envLine);
        processExportCommand(envLine, result);
        return true;
    }
    else if (cmd == "env" || cmd == "printenv") {
        std::string envVar;
        iss >> envVar;
        if (envVar.empty()) {
            extern char **environ;
            std::stringstream ss;
            for (char **env = environ; *env != nullptr; env++) {
                ss << *env << std::endl;
            }
            result = ss.str();
        } else {
            const char* value = getenv(envVar.c_str());
            result = value ? value : "Error: Environment variable '" + envVar + "' is not set";
        }
        return true;
    }
    else if (cmd == "jobs") {
        std::stringstream jobOutput;
        updateJobStatus();
        
        std::lock_guard<std::mutex> lock(jobsMutex);
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
        std::cout << result << std::endl;
        return true;
    }
    else if (cmd == "fg") {
        int jobId = 0;
        iss >> jobId;
        if (jobId <= 0) jobId = 1;
        
        if (bringJobToForeground(jobId)) {
            result = "Job brought to foreground";
            std::cout << result << std::endl;
            return true;
        } else {
            result = "Error: No job with ID " + std::to_string(jobId) + " found";
            std::cout << result << std::endl;
            return false;
        }
    }
    else if (cmd == "bg") {
        int jobId = 0;
        iss >> jobId;
        if (jobId <= 0) jobId = 1;
        
        if (sendJobToBackground(jobId)) {
            result = "Job sent to background";
            std::cout << result << std::endl;
            return true;
        } else {
            result = "Error: No job with ID " + std::to_string(jobId) + " found";
            std::cout << result << std::endl;
            return false;
        }
    }
    else if (cmd == "kill") {
        int jobId = 0;
        iss >> jobId;
        
        if (killJob(jobId)) {
            result = "Job killed";
            std::cout << result << std::endl;
            return true;
        } else {
            result = "Error: No job with ID " + std::to_string(jobId) + " found";
            std::cout << result << std::endl;
            return false;
        }
    }
    else if (cmd == "alias") {
        std::string aliasLine;
        std::getline(iss >> std::ws, aliasLine);
        
        if (aliasLine.empty()) {
            
            if (aliases.empty()) {
                result = "No aliases defined";
            } else {
                std::stringstream ss;
                for (const auto& [name, value] : aliases) {
                    ss << "alias " << name << "='" << value << "'\n";
                }
                result = ss.str();
            }
            std::cout << result << std::endl;
            return true;
        }
        
        
        size_t eqPos = aliasLine.find('=');
        if (eqPos != std::string::npos) {
            std::string name = aliasLine.substr(0, eqPos);
            std::string value = aliasLine.substr(eqPos + 1);
            
            
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            
            if (value.size() >= 2 && 
                ((value.front() == '"' && value.back() == '"') || 
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.size() - 2);
            }
            
            aliases[name] = value;
            
            result = "Alias '" + name + "' defined";
            
            
            
            setenv("CJSH_SAVE_ALIAS_NAME", name.c_str(), 1);
            setenv("CJSH_SAVE_ALIAS_VALUE", value.c_str(), 1);
            setenv("CJSH_SAVE_ALIAS", "1", 1);
        } else {
            
            std::string name = aliasLine;
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            
            auto it = aliases.find(name);
            if (it != aliases.end()) {
                result = "alias " + name + "='" + it->second + "'";
            } else {
                result = "No alias named '" + name + "' defined";
            }
            std::cout << result << std::endl;
        }
        return true;
    }
    else if (cmd == "sudo" || cmd == "ssh" || cmd == "su" || cmd == "login" || cmd == "passwd") {
        if (cmd == "sudo") {
            if (command.find("-S") == std::string::npos) {
                std::string sudoCommand = "sudo -S " + command.substr(5);
                return executeInteractiveCommand(sudoCommand, result);
            } else {
            }
        }
        return executeInteractiveCommand(command, result);
    } else {
        bool background = false;
        // handle trailing &
        if (!fullCommand.empty() && fullCommand.back() == '&') {
            background = true;
            fullCommand.pop_back();
            size_t lastNonSpace = fullCommand.find_last_not_of(" \t");
            if (lastNonSpace != std::string::npos)
                fullCommand = fullCommand.substr(0, lastNonSpace + 1);
        }
        auto args = parseCommandIntoArgs(fullCommand);
        
        // Add check for empty args
        if (args.empty()) {
            result = "Error: Empty command";
            return false;
        }
        
        // Check if command exists before proceeding
        std::string executable = findExecutableInPath(args[0]);
        if (executable.empty() || executable == args[0]) {
            // If findExecutableInPath returns empty or the original command,
            // it means the command was not found
            if (access(args[0].c_str(), F_OK) != 0) {
                result = "Error: command not found: " + args[0];
                return false;
            }
        }
        
        std::vector<RedirectionInfo> redirections;
        if (handleRedirection(fullCommand, args, redirections)) {
            if (background) {
                pid_t pid = fork();
                if (pid == 0) {
                    setpgid(0, 0);
                    // set per-command env
                    for (auto& kv : cmdEnvs) {
                        setenv(kv.first.c_str(), kv.second.c_str(), 1);
                    }
                    std::vector<int> savedFds;
                    if (setupRedirection(redirections, savedFds)) {
                        auto expandedArgs = expandWildcardsInArgs(args);
                        std::vector<char*> argv;
                        for (auto& a : expandedArgs) argv.push_back(a.data());
                        argv.push_back(nullptr);
                        std::string exe = findExecutableInPath(expandedArgs[0]);
                        if (!exe.empty()) execvp(exe.c_str(), argv.data());
                    }
                    exit(EXIT_FAILURE);
                }
                {
                    std::lock_guard<std::mutex> lock(jobsMutex);
                    jobs.push_back(Job(pid, fullCommand, false));
                    result = "Started background process [" + std::to_string(jobs.size()) + "] (PID: " + std::to_string(pid) + ")";
                }
                return true;
            } else {
                pid_t pid = fork();
                if (pid == 0) {
                    setpgid(0, 0);
                    tcsetpgrp(STDIN_FILENO, getpid());
                    std::vector<int> savedFds;
                    if (setupRedirection(redirections, savedFds)) {
                        // apply per-command environment
                        for (auto& kv : cmdEnvs) {
                            setenv(kv.first.c_str(), kv.second.c_str(), 1);
                        }
                        auto expandedArgs = expandWildcardsInArgs(args);
                        std::vector<char*> argv;
                        for (auto& a : expandedArgs) argv.push_back(a.data());
                        argv.push_back(nullptr);
                        std::string exe = findExecutableInPath(expandedArgs[0]);
                        if (!exe.empty()) execvp(exe.c_str(), argv.data());
                    }
                    exit(EXIT_FAILURE);
                }
                
                tcsetpgrp(STDIN_FILENO, pid);
                
                int status;
                waitpid(pid, &status, WUNTRACED);
                
                tcsetpgrp(STDIN_FILENO, getpgid(0));
                
                updateJobStatus();
                
                if (WIFEXITED(status)) {
                    int exitStatus = WEXITSTATUS(status);
                    if (exitStatus != 0) {
                        result = "Command failed with exit code " + std::to_string(exitStatus);
                        return false;
                    } else {
                        result = "Command completed successfully";
                        return true;
                    }
                } else if (WIFSIGNALED(status)) {
                    result = "Command terminated by signal " + std::to_string(WTERMSIG(status));
                    return false;
                } else {
                    result = "Command completed";
                    return true;
                }
            }
        } else {
            result = "Error in command syntax or redirection";
            return false;
        }
    }
    
    result = "Error: Failed to execute command";
    return false;
}

void Terminal::processExportCommand(const std::string& exportLine, std::string& result) {
    std::istringstream iss(exportLine);
    std::string assignment;
    bool success = false;
    std::vector<std::string> failures;
    
    while (iss >> assignment) {
        size_t eqPos = assignment.find('=');
        if (eqPos != std::string::npos) {
            std::string name = assignment.substr(0, eqPos);
            std::string value = assignment.substr(eqPos + 1);
            
            if (value.size() >= 2 && 
                ((value.front() == '"' && value.back() == '"') || 
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.size() - 2);
            }
            
            value = expandEnvironmentVariables(value);
            
            if (setenv(name.c_str(), value.c_str(), 1) == 0) {
                success = true;
                
                
                setenv("CJSH_SAVE_ENV_NAME", name.c_str(), 1);
                setenv("CJSH_SAVE_ENV_VALUE", value.c_str(), 1);
                setenv("CJSH_SAVE_ENV", "1", 1);
            } else {
                failures.push_back(name + "=" + value + " (" + strerror(errno) + ")");
            }
        } else {
            failures.push_back(assignment + " (missing '=' operator)");
        }
    }
    
    if (success && failures.empty()) {
        result = "Environment variable(s) exported successfully";
    } else if (success) {
        result = "Some environment variables were exported, but the following failed:\n";
        for (const auto& fail : failures) {
            result += "- " + fail + "\n";
        }
    } else {
        result = "Failed to export environment variable(s):\n";
        for (const auto& fail : failures) {
            result += "- " + fail + "\n";
        }
    }
}

std::string Terminal::expandEnvironmentVariables(const std::string& input) {
    std::string result = input;
    size_t pos = 0;
    
    while ((pos = result.find('$', pos)) != std::string::npos) {
        if (pos + 1 < result.size() && result[pos + 1] == '{') {
            size_t closeBrace = result.find('}', pos + 2);
            if (closeBrace != std::string::npos) {
                std::string varName = result.substr(pos + 2, closeBrace - pos - 2);
                const char* envValue = getenv(varName.c_str());
                result.replace(pos, closeBrace - pos + 1, envValue ? envValue : "");
            } else {
                pos++;
            }
        }
        else if (pos + 1 < result.size() && (isalpha(result[pos + 1]) || result[pos + 1] == '_')) {
            size_t endPos = pos + 1;
            while (endPos < result.size() && (isalnum(result[endPos]) || result[endPos] == '_')) {
                endPos++;
            }
            
            std::string varName = result.substr(pos + 1, endPos - pos - 1);
            const char* envValue = getenv(varName.c_str());
            result.replace(pos, endPos - pos, envValue ? envValue : "");
        } else {
            pos++;
        }
    }
    
    return result;
}

bool Terminal::hasWildcard(const std::string& arg) {
    return arg.find('*') != std::string::npos || 
           arg.find('?') != std::string::npos || 
           (arg.find('[') != std::string::npos && arg.find(']') != std::string::npos);
}

bool Terminal::matchPattern(const std::string& pattern, const std::string& str) {
    size_t patIdx = 0;
    size_t strIdx = 0;
    size_t patLen = pattern.length();
    size_t strLen = str.length();
    
    while (patIdx < patLen && strIdx < strLen) {
        if (pattern[patIdx] == '?') {
            patIdx++;
            strIdx++;
        } else if (pattern[patIdx] == '*') {
            patIdx++;
            if (patIdx == patLen) return true;
            
            for (size_t i = strIdx; i <= strLen; i++) {
                if (matchPattern(pattern.substr(patIdx), str.substr(i))) 
                    return true;
            }
            return false;
        } else if (pattern[patIdx] == '[') {
            bool match = false;
            patIdx++;
            bool negate = false;
            
            if (patIdx < patLen && pattern[patIdx] == '!') {
                negate = true;
                patIdx++;
            }
            
            bool charMatched = false;
            for (; patIdx < patLen && pattern[patIdx] != ']'; patIdx++) {
                if (patIdx + 2 < patLen && pattern[patIdx + 1] == '-') {
                    char rangeStart = pattern[patIdx];
                    char rangeEnd = pattern[patIdx + 2];
                    if (str[strIdx] >= rangeStart && str[strIdx] <= rangeEnd) {
                        charMatched = true;
                    }
                    patIdx += 2;
                } else if (pattern[patIdx] == str[strIdx]) {
                    charMatched = true;
                }
            }
            
            if (negate) charMatched = !charMatched;
            
            if (!charMatched) return false;
            
            patIdx++;
            strIdx++;
        } else if (pattern[patIdx] == str[strIdx]) {
            patIdx++;
            strIdx++;
        } else {
            return false;
        }
    }
    
    while (patIdx < patLen && pattern[patIdx] == '*') {
        patIdx++;
    }
    
    return patIdx == patLen && strIdx == strLen;
}

std::vector<std::string> Terminal::expandWildcards(const std::string& pattern) {
    std::vector<std::string> result;
    
    if (!hasWildcard(pattern)) {
        result.push_back(pattern);
        return result;
    }
    
    std::filesystem::path dirPath;
    std::string filePattern;
    
    size_t lastSlash = pattern.find_last_of('/');
    if (lastSlash != std::string::npos) {
        dirPath = pattern.substr(0, lastSlash);
        filePattern = pattern.substr(lastSlash + 1);
    } else {
        dirPath = ".";
        filePattern = pattern;
    }
    
    if (filePattern.empty()) {
        result.push_back(pattern);
        return result;
    }
    
    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
        result.push_back(pattern);
        return result;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
        std::string filename = entry.path().filename().string();
        if (matchPattern(filePattern, filename)) {
            std::string path;
            if (dirPath == ".") {
                path = filename;
            } else {
                path = (std::filesystem::path(dirPath) / filename).string();
            }
            result.push_back(path);
        }
    }
    
    if (result.empty()) {
        result.push_back(pattern);
    }
    
    return result;
}

std::vector<std::string> Terminal::expandWildcardsInArgs(const std::vector<std::string>& args) {
    if (args.empty()) return args;
    
    std::vector<std::string> result;
    
    result.push_back(args[0]);
    
    for (size_t i = 1; i < args.size(); i++) {
        if (hasWildcard(args[i])) {
            std::vector<std::string> expanded = expandWildcards(args[i]);
            result.insert(result.end(), expanded.begin(), expanded.end());
        } else {
            result.push_back(args[i]);
        }
    }
    
    return result;
}

bool Terminal::executeInteractiveCommand(const std::string& command, std::string& result) {
    struct termios term_attr;
    tcgetattr(STDIN_FILENO, &term_attr);
    
    pid_t pid = fork();
    
    if (pid == -1) {
        result = "Failed to fork process: " + std::string(strerror(errno));
        return false;
    }
    
    if (pid == 0) {
        pid_t child_pid = getpid();
        setpgid(child_pid, child_pid);
        tcsetpgrp(STDIN_FILENO, child_pid);
        
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        
        if (chdir(currentDirectory.c_str()) != 0) {
            std::cerr << "Failed to change directory: " << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }
        
        setenv("PWD", currentDirectory.c_str(), 1);
        
        std::vector<std::string> args = parseCommandIntoArgs(command);
        if (args.empty()) {
            exit(EXIT_FAILURE);
        }
        
        std::vector<char*> argv;
        for (auto& arg : args) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);
        
        std::string executable = findExecutableInPath(args[0]);
        if (executable.empty()) {
            std::cerr << "Command not found: " << args[0] << std::endl;
            exit(EXIT_FAILURE);
        }
        
        execvp(executable.c_str(), argv.data());
        
        std::cerr << "Failed to execute " << args[0] << ": " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
    else {
        int status;
        
        tcsetpgrp(STDIN_FILENO, pid);
        
        if (waitpid(pid, &status, WUNTRACED) == -1) {
            result = "Error waiting for process: " + std::string(strerror(errno));
            tcsetpgrp(STDIN_FILENO, getpgid(0));
            tcsetattr(STDIN_FILENO, TCSADRAIN, &term_attr);
            return false;
        }
        
        tcsetpgrp(STDIN_FILENO, getpgid(0));
        
        tcsetattr(STDIN_FILENO, TCSADRAIN, &term_attr);
        
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status == 0) {
                result = "Command completed successfully";
                return true;
            } else {
                result = "Command failed with exit status " + std::to_string(exit_status);
                return false;
            }
        } else if (WIFSIGNALED(status)) {
            result = "Command terminated by signal " + std::to_string(WTERMSIG(status));
            return false;
        } else if (WIFSTOPPED(status)) {
            std::lock_guard<std::mutex> lock(jobsMutex);
            jobs.push_back(Job(pid, command, false));
            jobs.back().status = status;
            result = "Process stopped";
            return true;
        }
        
        result = "Command completed with unknown status";
        return false;
    }
}

pid_t Terminal::executeChildProcess(const std::string& command, bool foreground) {
    pid_t pid = fork();
    
    if (pid == -1) {
        throw std::runtime_error("Failed to fork process");
    }
    else if (pid == 0) {
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        
        pid_t childPid = getpid();
        if (setpgid(childPid, childPid) < 0) {
            std::cerr << "Failed to set process group: " << strerror(errno) << std::endl;
        }

        if (foreground) {
            tcsetpgrp(STDIN_FILENO, childPid);
        }

        if (!foreground) {
            if (setsid() < 0) {
                std::cerr << "Failed to create new session: " << strerror(errno) << std::endl;
            }
        }
        
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        
        if (chdir(currentDirectory.c_str()) != 0) {
            std::cerr << "cjsh: failed to change directory to " << currentDirectory << ": " 
                      << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }
        
        setenv("PWD", currentDirectory.c_str(), 1);
        
        std::vector<std::string> args = parseCommandIntoArgs(command);
        if (!args.empty()) {
            std::vector<char*> argv;
            for (auto& arg : args) {
                argv.push_back(arg.data());
            }
            argv.push_back(nullptr);
            
            std::string executable = findExecutableInPath(args[0]);
            if (!executable.empty()) {
                execvp(executable.c_str(), argv.data());
            }
        }
        
        std::cerr << "cjsh: command not found: " << args[0] << std::endl;
        exit(EXIT_FAILURE);
    }
    
    if (setpgid(pid, pid) < 0 && errno != EACCES) {
        std::cerr << "Parent: Failed to set process group: " << strerror(errno) << std::endl;
    }
    
    if (foreground) {
        tcsetpgrp(STDIN_FILENO, pid);
        
        waitForForegroundJob(pid);
        
        tcsetpgrp(STDIN_FILENO, getpgid(0));
    }
    
    return pid;
}

bool Terminal::changeDirectory(const std::string& dir, std::string& result) {
    std::string targetDir = dir;
    
    if (targetDir.empty() || targetDir == "~") {
        const char* homeDir = getenv("HOME");
        if (homeDir) {
            targetDir = homeDir;
        } else {
            result = "Error: Could not determine home directory - HOME environment variable is not set";
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
            result = "Error: Cannot go up from root directory";
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
            std::cerr << result << std::endl;
            return false;
        }
        
        if (!std::filesystem::is_directory(dirPath)) {
            result = "cd: " + targetDir + ": Not a directory";
            std::cerr << result << std::endl;
            return false;
        }
        
        try {
            currentDirectory = std::filesystem::canonical(dirPath).string();
        } catch (const std::filesystem::filesystem_error& e) {
            result = "cd: " + targetDir + ": " + e.what();
            std::cerr << result << std::endl;
            return false;
        }
    }
    
    if (chdir(currentDirectory.c_str()) != 0) {
        std::string errorMsg = "cd: " + std::string(strerror(errno));
        result = errorMsg;
        return false;
    }
    
    setenv("PWD", currentDirectory.c_str(), 1);
    
    result = "Changed directory to: " + currentDirectory;
    return true;
}

void Terminal::waitForForegroundJob(pid_t pid) {
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
    
    tcsetattr(STDIN_FILENO, TCSADRAIN, &term_settings);
}

void Terminal::updateJobStatus() {
    std::lock_guard<std::mutex> lock(jobsMutex);
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

void Terminal::listJobs() {
    updateJobStatus();
    
    std::lock_guard<std::mutex> lock(jobsMutex);
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

bool Terminal::bringJobToForeground(int jobId) {
    updateJobStatus();
    
    std::lock_guard<std::mutex> lock(jobsMutex);
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
    
    tcsetattr(STDIN_FILENO, TCSADRAIN, &term_settings);
    
    return true;
}

bool Terminal::sendJobToBackground(int jobId) {
    updateJobStatus();
    
    std::lock_guard<std::mutex> lock(jobsMutex);
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

bool Terminal::killJob(int jobId) {
    updateJobStatus();
    
    std::lock_guard<std::mutex> lock(jobsMutex);
    if (jobId <= 0 || jobId > static_cast<int>(jobs.size())) {
        return false;
    }
    
    Job& job = jobs[jobId - 1];
    
    if (kill(-job.pid, SIGTERM) < 0) {
        kill(job.pid, SIGTERM);
    }
    
    usleep(100000);
    
    if (kill(job.pid, 0) == 0) {
        if (kill(-job.pid, SIGKILL) < 0) {
            kill(job.pid, SIGKILL);
        }
    }
    
    jobs.erase(jobs.begin() + (jobId - 1));
    
    return true;
}

void Terminal::toggleDisplayWholePath(){
    setDisplayWholePath(!displayWholePath);
}

bool Terminal::isDisplayWholePath(){
    return displayWholePath;
}

std::vector<std::string> Terminal::getTerminalCacheUserInput(){
    return terminalCacheUserInput;
}

std::vector<std::string> Terminal::getTerminalCacheTerminalOutput(){
    return terminalCacheTerminalOutput;
}

void Terminal::clearTerminalCache(){
    terminalCacheUserInput.clear();
    terminalCacheTerminalOutput.clear();
}

std::string Terminal::returnMostRecentUserInput(){
    return terminalCacheUserInput.back();
}

std::string Terminal::returnMostRecentTerminalOutput(){
    return terminalCacheTerminalOutput.back();
}

std::string Terminal::getCurrentFilePath(){
    if (currentDirectory.empty()) {
        return std::filesystem::current_path().string();
    }
    return currentDirectory;
}

std::string Terminal::getCurrentFileName(){
    std::string currentDirectory = getCurrentFilePath();
    std::string currentFileName = std::filesystem::path(currentDirectory).filename().string();
    if (currentFileName.empty()) {
        return "/";
    }
    return currentFileName;
}

bool Terminal::isRootPath(const std::filesystem::path& path){
    return path == path.root_path();
}

void Terminal::addCommandToHistory(const std::string& command) {
    if (command.empty()) {
        return;
    }
    if (std::find(terminalCacheUserInput.begin(), terminalCacheUserInput.end(), command) != terminalCacheUserInput.end()) {
        return;
    }
    commandHistoryIndex = terminalCacheUserInput.size();
    terminalCacheUserInput.push_back(command);
}

void Terminal::setShellColor(const std::string& color){
    this->SHELL_COLOR = color;
}

void Terminal::setDirectoryColor(const std::string& color){
    this->DIRECTORY_COLOR = color;
}

void Terminal::setBranchColor(const std::string& color){
    this->BRANCH_COLOR = color;
}

void Terminal::setGitColor(const std::string& color){
    this->GIT_COLOR = color;
}

void Terminal::setPromptFormat(const std::string& format){
    this->PROMPT_FORMAT = format;
}

std::string Terminal::getShellColor() const {
    return SHELL_COLOR;
}

std::string Terminal::getDirectoryColor() const {
    return DIRECTORY_COLOR;
}

std::string Terminal::getBranchColor() const {
    return BRANCH_COLOR;
}

std::string Terminal::getGitColor() const {
    return GIT_COLOR;
}

std::string Terminal::getPromptFormat() const {
    return PROMPT_FORMAT;
}

void Terminal::terminateAllChildProcesses() {
    std::lock_guard<std::mutex> lock(jobsMutex);
    
    if (jobs.empty()) {
        return;
    }

    for (const auto& job : jobs) {
        if (kill(-job.pid, SIGTERM) < 0) {
            kill(job.pid, SIGTERM);
        }
    }
    
    usleep(10000);
    
    for (const auto& job : jobs) {
        if (kill(job.pid, 0) == 0) {
            if (kill(-job.pid, SIGKILL) < 0) {
                kill(job.pid, SIGKILL);
            }
        }
    }
    usleep(5000);
    for (const auto& job : jobs) {
        kill(-job.pid, SIGKILL);
        kill(job.pid, SIGKILL);
    }
    
    jobs.clear();
}

std::vector<std::string> Terminal::parseCommandIntoArgs(const std::string& command) {
    std::vector<std::string> args;
    std::istringstream iss(command);
    std::string arg;
    bool inQuotes = false;
    char quoteChar = 0;
    std::string currentArg;
    
    for (size_t i = 0; i < command.length(); i++) {
        char c = command[i];
        
        if ((c == '"' || c == '\'') && (i == 0 || command[i-1] != '\\')) {
            if (!inQuotes) {
                inQuotes = true;
                quoteChar = c;
            } else if (c == quoteChar) {
                inQuotes = false;
                quoteChar = 0;
            } else {
                currentArg += c;
            }
        } else if ((c == ' ' || c == '\t') && !inQuotes) {
            if (!currentArg.empty()) {
                args.push_back(currentArg);
                currentArg.clear();
            }
        } else {
            currentArg += c;
        }
    }
    
    if (!currentArg.empty()) {
        args.push_back(currentArg);
    }
    
    return args;
}

std::string Terminal::findExecutableInPath(const std::string& command) {
    if (aliases.find(command) != aliases.end()) {
        return command;
    }

    if (command.find('/') != std::string::npos) {
        std::string fullPath;
        if (command[0] == '/') {
            fullPath = command;
        } else {
            fullPath = (std::filesystem::path(currentDirectory) / command).string();
        }

        if (access(fullPath.c_str(), F_OK) == 0) {
            if (access(fullPath.c_str(), X_OK) == 0) {
                return fullPath;
            } else {
                std::cerr << "Error: '" << fullPath << "' exists but is not executable" << std::endl;
                return fullPath;
            }
        }
        std::cerr << "Error: '" << fullPath << "' not found" << std::endl;
        return "";
    }

    const char* pathEnv = getenv("PATH");
    if (!pathEnv) {
        std::cerr << "Error: PATH environment variable is not set" << std::endl;
        return "";
    }
    
    std::string path(pathEnv);
    std::string delimiter = ":";
    size_t pos = 0;
    std::string token;
    
    std::string currentDirPath = (std::filesystem::path(currentDirectory) / command).string();
    if (access(currentDirPath.c_str(), X_OK) == 0) {
        return currentDirPath;
    }
    
    while ((pos = path.find(delimiter)) != std::string::npos) {
        token = path.substr(0, pos);
        if (!token.empty()) {
            std::string candidatePath = token + "/" + command;
            
            if (access(candidatePath.c_str(), X_OK) == 0) {
                return candidatePath;
            }
        }
        
        path.erase(0, pos + delimiter.length());
    }

    if (!path.empty()) {
        std::string candidatePath = path + "/" + command;
        if (access(candidatePath.c_str(), X_OK) == 0) {
            return candidatePath;
        }
    }

    return command;
}

