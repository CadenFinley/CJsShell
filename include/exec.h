#pragma once

#include <map>
#include <string>
#include <sys/_types/_pid_t.h>

class Exec {
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
  
public:
  Exec();
  ~Exec();

  // parser will handle aliases, envvars
  pid_t executeCommandSync(const std::vector<std::string>& commands);
  pid_t executeCommandASync(const std::vector<std::string>& commands);

  // gets the last output of the command
  std::string getLastOutput();

  // gets running jobs
  void listJobs();

private:
  std::string lastOutput;
  std::mutex jobsMutex;
  std::atomic<bool> shouldTerminate;
  std::vector<Job> jobs;
  struct termios original_termios;
  bool terminal_state_saved;

  std::string findExecutableInPath(const std::string& command);
  std::string processCommandSubstitution(const std::string& command);
  std::vector<std::string> splitByPipes(const std::string& command);
  bool executeCommandWithPipes(const std::vector<std::string>& commands, std::string& result);
  bool handleRedirection(const std::string& command, std::vector<std::string>& args, std::vector<RedirectionInfo>& redirections);
  bool setupRedirection(const std::vector<RedirectionInfo>& redirections, std::vector<int>& savedFds);
  void restoreRedirection(const std::vector<int>& savedFds);
  bool hasWildcard(const std::string& arg);
  bool matchPattern(const std::string& pattern, const std::string& str);
  std::vector<std::string> expandWildcards(const std::string& pattern);
  std::vector<std::string> expandWildcardsInArgs(const std::vector<std::string>& args);
  pid_t executeChildProcess(const std::string& command, bool foreground = true);
  bool changeDirectory(const std::string& dir, std::string& result);
  void waitForForegroundJob(pid_t pid);
  void updateJobStatus();
  bool parseAndExecuteCommand(const std::string& command, std::string& result);
  bool executeIndividualCommand(const std::string& command, std::string& result);
  bool executeInteractiveCommand(const std::string& command, std::string& result);

  bool bringJobToForeground(int jobId);
  bool sendJobToBackground(int jobId);
  bool killJob(int jobId);
  bool setupTerminalForShellMode();
  void cleanupTerminalAfterShellMode();
  bool isStandaloneShell() const;
  void terminateAllChildProcesses();
  void setTerminationFlag(bool terminate) { shouldTerminate = terminate; }   
  static void signalHandlerWrapper(int signum, siginfo_t* info, void* context);
  void saveTerminalState();
  void restoreTerminalState();

protected:

};