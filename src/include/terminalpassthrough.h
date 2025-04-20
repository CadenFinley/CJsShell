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

class TerminalPassthrough {
public:
    struct Job {
        pid_t pid;
        std::string command;
        bool foreground;
        int status;
        
        Job(pid_t p, const std::string& cmd, bool fg = true) 
            : pid(p), command(cmd), foreground(fg), status(0) {}
    };

    TerminalPassthrough();
    ~TerminalPassthrough();

    std::string getTerminalName();
    std::string returnCurrentTerminalPosition();
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
    void addCommandToHistory(const std::string& command);
    void setAliases(const std::map<std::string, std::string>& aliases);
    std::string getPreviousCommand();
    std::string getNextCommand();

    std::vector<std::string> getTerminalCacheUserInput();
    std::vector<std::string> getTerminalCacheTerminalOutput();
    void clearTerminalCache();
    std::string returnMostRecentUserInput();
    std::string returnMostRecentTerminalOutput();
    
    std::vector<std::string> getCommandHistory(size_t count);

    void setShellColor(const std::string& color);
    void setDirectoryColor(const std::string& color);
    void setBranchColor(const std::string& color);
    void setGitColor(const std::string& color);
    std::string getShellColor() const;
    std::string getDirectoryColor() const;
    std::string getBranchColor() const;
    std::string getGitColor() const;

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
    bool is_standalone_shell;
    
    pid_t executeChildProcess(const std::string& command, bool foreground = true);
    bool changeDirectory(const std::string& dir, std::string& result);
    void waitForForegroundJob(pid_t pid);
    void updateJobStatus();
    void parseAndExecuteCommand(const std::string& command, std::string& result);
    bool executeIndividualCommand(const std::string& command, std::string& result);
    
    bool saveTerminalState();
    bool restoreTerminalState();
    void setStandaloneMode(bool standalone);
    
    bool executeInteractiveCommand(const std::string& command, std::string& result);
};

void processProfileFile(const std::string& filePath);

#endif // TERMINALPASSTHROUGH_H
