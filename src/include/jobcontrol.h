#ifndef JOBCONTROL_H
#define JOBCONTROL_H

#include <vector>
#include <string>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

// Process state enumeration
enum class ProcessState {
    RUNNING,
    STOPPED,
    COMPLETED,
    TERMINATED
};

// Process representation
struct Process {
    pid_t pid;
    int status;
    ProcessState state;
    std::string command;
    
    Process(pid_t p, const std::string& cmd) : 
        pid(p), status(0), state(ProcessState::RUNNING), command(cmd) {}
};

// Job representation (process group)
class Job {
private:
    pid_t pgid;
    std::vector<Process> processes;
    std::string commandLine;
    int jobId;
    bool foreground;
    
public:
    Job(pid_t pg, const std::string& cmd, int id, bool fg = false);
    
    void addProcess(pid_t pid, const std::string& cmd);
    bool updateProcessStatus(pid_t pid, int status);
    bool isCompleted() const;
    bool isStopped() const;
    
    void continueForeground(int shellTerminal, struct termios* shellTmodes);
    void continueBackground();
    void stop();
    
    int getJobId() const { return jobId; }
    pid_t getpgid() const { return pgid; }
    const std::string& getCommandLine() const { return commandLine; }
};

// JobControl class for managing jobs
class JobControl {
private:
    std::vector<Job> jobs;
    int nextJobId;
    int shellTerminal;
    struct termios shellTmodes;
    pid_t shellPgid;
    
public:
    JobControl(int terminal, struct termios* tmodes, pid_t shellpg);
    
    Job* addJob(pid_t pgid, const std::string& cmd, bool foreground = false);
    Job* findJob(pid_t pgid);
    Job* findJobById(int jobId);
    
    void updateJobs();
    void putJobInForeground(Job* job, bool cont);
    void putJobInBackground(Job* job, bool cont);
    
    void printJobs() const;
    void removeCompletedJobs();
    void foregroundJob(int jobId);
    void backgroundJob(int jobId);
    
    void killJob(int jobId);
    void handleSigchld();
};

#endif // JOBCONTROL_H
