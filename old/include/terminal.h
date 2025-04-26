#ifndef TERMINAL_H
#define TERMINAL_H
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
#include <regex>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <array>
#include <map>
#include <chrono>
#include <mutex>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <atomic>

class Terminal {
public:
    struct Job {
        pid_t pid;
        std::string command;
        bool foreground;
        int status;
        
        Job(pid_t p, const std::string& cmd, bool fg = true) : pid(p), command(cmd), foreground(fg), status(0) {}
    };

    struct RedirectionInfo {
        int type;
        std::string file;
    };

    Terminal();
    ~Terminal();

    std::string getTerminalName();
    std::string returnCurrentTerminalPosition();
    std::string expandPromptFormat(const std::string& format);
    int getTerminalCurrentPositionRawLength();
    void printCurrentTerminalPosition();

    std::vector<std::string> getFilesAtCurrentPath(const bool& includeHidden, const bool& fullFilePath, const bool& includeDirectories);
    std::string getFullPathOfFile(const std::string& file);
    std::string getCurrentFilePath();
    void setDisplayWholePath(bool displayWholePath);
    void toggleDisplayWholePath();
    bool isDisplayWholePath();
    bool isRootPath(const std::filesystem::path& path);
    
    std::thread executeCommand(std::string command);
    bool executeCommandSync(const std::string& command);
    void addCommandToHistory(const std::string& command);
    void setAliases(const std::map<std::string, std::string>& aliases);

    std::vector<std::string> getTerminalCacheUserInput();
    std::vector<std::string> getTerminalCacheTerminalOutput();
    void clearTerminalCache();
    std::string returnMostRecentUserInput();
    std::string returnMostRecentTerminalOutput();

    void setShellColor(const std::string& color);
    void setDirectoryColor(const std::string& color);
    void setBranchColor(const std::string& color);
    void setGitColor(const std::string& color);
    void setPromptFormat(const std::string& format);
    std::string getShellColor() const;
    std::string getDirectoryColor() const;
    std::string getBranchColor() const;
    std::string getGitColor() const;
    std::string getPromptFormat() const;

    void listJobs();
    bool bringJobToForeground(int jobId);
    bool sendJobToBackground(int jobId);
    bool killJob(int jobId);
    
    bool setupTerminalForShellMode();
    void cleanupTerminalAfterShellMode();
    bool isStandaloneShell() const;

    const std::vector<Job>& getActiveJobs() const { return jobs; }
    void setTerminationFlag(bool terminate) { shouldTerminate = terminate; }
    void terminateAllChildProcesses();
    
    
    std::string expandAliases(const std::string& command);
    std::string processCommandSubstitution(const std::string& command);
    std::vector<std::string> splitByPipes(const std::string& command);
    bool executeCommandWithPipes(const std::vector<std::string>& commands, std::string& result);
    bool handleRedirection(const std::string& command, std::vector<std::string>& args, 
                          std::vector<RedirectionInfo>& redirections);
    bool setupRedirection(const std::vector<RedirectionInfo>& redirections, 
                         std::vector<int>& savedFds);
    void restoreRedirection(const std::vector<int>& savedFds);
    
    bool hasWildcard(const std::string& arg);
    bool matchPattern(const std::string& pattern, const std::string& str);
    std::vector<std::string> expandWildcards(const std::string& pattern);
    std::vector<std::string> expandWildcardsInArgs(const std::vector<std::string>& args);
    
    void processExportCommand(const std::string& exportLine, std::string& result);
    std::string expandEnvironmentVariables(const std::string& input);

    
    static void signalHandlerWrapper(int signum, siginfo_t* info, void* context);
    void saveTerminalState();
    void restoreTerminalState();

private:
    std::string currentDirectory;
    bool displayWholePath;
    std::vector<std::string> terminalCacheUserInput;
    std::vector<std::string> terminalCacheTerminalOutput;
    std::map<std::string, std::string> aliases;
    std::string SHELL_COLOR = "\033[1;31m";
    std::string RESET_COLOR = "\033[0m";
    std::string DIRECTORY_COLOR = "\033[1;34m";
    std::string BRANCH_COLOR = "\033[1;33m";
    std::string GIT_COLOR = "\033[1;32m";
    std::string PROMPT_FORMAT = "cjsh \\w";
    int commandHistoryIndex = -1;
    int terminalCurrentPositionRawLength = 0;
    std::string terminalName;

    std::chrono::steady_clock::time_point lastGitStatusCheck = std::chrono::steady_clock::now() - std::chrono::seconds(30);
    std::string cachedGitDir;
    std::string cachedStatusSymbols;
    bool cachedIsCleanRepo;

    std::mutex gitStatusMutex;
    std::mutex jobsMutex;
    std::atomic<bool> isGitStatusCheckRunning{false};
    std::atomic<bool> shouldTerminate;

    std::string getCurrentFileName();
    std::string removeSpecialCharacters(const std::string& input);

    std::vector<std::string> parseCommandIntoArgs(const std::string& command);
    std::string findExecutableInPath(const std::string& command);

    std::vector<Job> jobs;
    
    struct termios original_termios;
    bool terminal_state_saved;
    
    pid_t executeChildProcess(const std::string& command, bool foreground = true);
    bool changeDirectory(const std::string& dir, std::string& result);
    void waitForForegroundJob(pid_t pid);
    void updateJobStatus();
    bool parseAndExecuteCommand(const std::string& command, std::string& result);
    bool executeIndividualCommand(const std::string& command, std::string& result);
    
    bool executeInteractiveCommand(const std::string& command, std::string& result);
};

#endif
