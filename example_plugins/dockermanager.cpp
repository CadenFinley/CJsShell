#include "plugininterface.h"
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <queue>
#include <map>
#include <array>
#include <cstdio>
#include <functional>
#include <sstream>
#include <thread>
#include <chrono>

class DockerManagerPlugin : public PluginInterface {
private:
    std::map<std::string, std::string> settings;
    bool dockerAvailable;
    
    // Helper function to execute Docker commands and return output
    std::string executeCommand(const std::string& cmd) {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        
        if (!pipe) {
            return "Error executing command";
        }
        
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        
        return result;
    }
    
    // Parse arguments from queue into a vector
    std::vector<std::string> parseArgs(std::queue<std::string>& args) {
        std::vector<std::string> parsedArgs;
        while (!args.empty()) {
            parsedArgs.push_back(args.front());
            args.pop();
        }
        return parsedArgs;
    }
    
    // Check if Docker is available
    bool checkDockerInstalled() {
        std::string dockerVersion = executeCommand(settings["docker_path"] + " --version 2>&1");
        if (dockerVersion.find("Docker version") == std::string::npos) {
            return false;
        }
        
        std::string dockerRunning = executeCommand(settings["docker_path"] + " info 2>&1");
        if (dockerRunning.find("ERROR") != std::string::npos) {
            return false;
        }
        
        return true;
    }
    
public:
    DockerManagerPlugin() : dockerAvailable(false) {
        // Default settings
        settings["docker_path"] = "docker";
        settings["auto_refresh"] = "true";
        settings["default_timeout"] = "10";
    }
    
    ~DockerManagerPlugin() override {}
    
    std::string getName() const override {
        return "dockermanager";
    }
    
    std::string getVersion() const override {
        return "1.0.0";
    }
    
    std::string getDescription() const override {
        return "Docker container and image management plugin for DevToolsTerminal";
    }
    
    std::string getAuthor() const override {
        return "Caden Finley";
    }
    
    bool initialize() override {
        // Check if Docker is installed and running
        dockerAvailable = checkDockerInstalled();
        
        if (!dockerAvailable) {
            std::cerr << "Docker is not installed, not in PATH, or the Docker daemon is not running." << std::endl;
            std::cerr << "The plugin will load but commands requiring Docker will be disabled." << std::endl;
            std::cerr << "Use 'check' to verify Docker status." << std::endl;
            // We still return true so the plugin loads
            return true;
        }
        std::string dockerVersion = executeCommand(settings["docker_path"] + " --version");
        std::cout << "Docker version: " << dockerVersion;
        return true;
    }
    
    void shutdown() override {
        std::cout << "Docker Manager plugin shutting down." << std::endl;
    }
    
    bool handleCommand(std::queue<std::string>& args) override {
        if (args.empty()) {
            std::cout << "Docker Manager usage: docker [command] [options]" << std::endl;
            std::cout << "Use 'dockerhelp' for available commands." << std::endl;
            return true;
        }
        
        // Get the command from the queue
        std::string command = args.front();
        args.pop();
        
        if (command == "help") {
            showHelp();
            return true;
        }
        else if (command == "check") {
            return checkDocker();
        }
        
        // For all Docker commands, check if Docker is available first
        if (!dockerAvailable) {
            dockerAvailable = checkDockerInstalled();
            if (!dockerAvailable) {
                std::cout << "Error: Docker is not available. Please make sure Docker is installed and the daemon is running." << std::endl;
                std::cout << "Use 'check' to verify Docker status." << std::endl;
                return false;
            }
        }
        
        if (command == "ps" || command == "containers") {
            return listContainers(args);
        }
        else if (command == "images") {
            return listImages(args);
        }
        else if (command == "stats") {
            return showStats(args);
        }
        else if (command == "start") {
            return startContainer(args);
        }
        else if (command == "stop") {
            return stopContainer(args);
        }
        else if (command == "restart") {
            return restartContainer(args);
        }
        else if (command == "rm") {
            return removeContainer(args);
        }
        else if (command == "rmi") {
            return removeImage(args);
        }
        else if (command == "pull") {
            return pullImage(args);
        }
        else if (command == "logs") {
            return showLogs(args);
        }
        else if (command == "exec") {
            return execInContainer(args);
        }
        else if (command == "networks") {
            return listNetworks(args);
        }
        else if (command == "volumes") {
            return listVolumes(args);
        }
        else if (command == "info") {
            return showInfo();
        }
        else if (command == "run") {
            return runContainer(args);
        }
        else if (command == "build") {
            return buildImage(args);
        }
        else {
            std::cout << "Unknown Docker command: " << command << std::endl;
            return false;
        }
    }
    
    std::vector<std::string> getCommands() const override {
        std::vector<std::string> commands;
        commands.push_back("check");
        commands.push_back("ps");
        commands.push_back("containers");
        commands.push_back("images");
        commands.push_back("stats");
        commands.push_back("start");
        commands.push_back("stop");
        commands.push_back("restart");
        commands.push_back("rm");
        commands.push_back("rmi");
        commands.push_back("pull");
        commands.push_back("logs");
        commands.push_back("exec");
        commands.push_back("networks");
        commands.push_back("volumes");
        commands.push_back("info");
        commands.push_back("run");
        commands.push_back("build");
        return commands;
    }
    
    std::map<std::string, std::string> getDefaultSettings() const override {
        return settings;
    }
    
    void updateSetting(const std::string& key, const std::string& value) override {
        settings[key] = value;
        
        // If docker_path is updated, check if Docker is available with the new path
        if (key == "docker_path") {
            dockerAvailable = checkDockerInstalled();
            if (!dockerAvailable) {
                std::cout << "Warning: Docker is not available at the specified path." << std::endl;
            }
        }
        
        std::cout << "Docker Manager setting updated: " << key << " = " << value << std::endl;
    }
    
private:
    void showHelp() {
        std::cout << "Docker Manager Plugin Commands:\n" << std::endl;
        std::cout << "  check                     Check Docker installation status" << std::endl;
        std::cout << "  ps|containers [options]   List containers" << std::endl;
        std::cout << "  images [options]          List images" << std::endl;
        std::cout << "  start <container>         Start a container" << std::endl;
        std::cout << "  stop <container>          Stop a container" << std::endl;
        std::cout << "  restart <container>       Restart a container" << std::endl;
        std::cout << "  rm <container>            Remove a container" << std::endl;
        std::cout << "  rmi <image>               Remove an image" << std::endl;
        std::cout << "  pull <image>              Pull an image" << std::endl;
        std::cout << "  build [options] -t <tag> <path>  Build an image from a Dockerfile" << std::endl;
        std::cout << "  logs <container> [options] Show container logs" << std::endl;
        std::cout << "  exec <container> <command> Run a command in a container" << std::endl;
        std::cout << "  run [options] <image>     Run a new container" << std::endl;
        std::cout << "  networks [options]        List networks" << std::endl;
        std::cout << "  volumes                   List volumes" << std::endl;
        std::cout << "  info                      Show system-wide information" << std::endl;
        std::cout << "  stats [container]         Show container resource usage" << std::endl;
    }
    
    bool checkDocker() {
        dockerAvailable = checkDockerInstalled();
        
        if (dockerAvailable) {
            std::string dockerVersion = executeCommand(settings["docker_path"] + " --version");
            std::string dockerInfo = executeCommand(settings["docker_path"] + " info --format '{{.ServerVersion}}'");
            
            std::cout << "Docker is installed and running correctly." << std::endl;
            std::cout << "Docker client: " << dockerVersion;
            std::cout << "Docker server: " << dockerInfo << std::endl;
            return true;
        } else {
            std::cout << "Docker is not available. Please check your installation." << std::endl;
            
            #ifdef __APPLE__
            std::cout << "On macOS:" << std::endl;
            std::cout << "1. Make sure Docker Desktop is installed" << std::endl;
            std::cout << "2. Open Docker Desktop from your Applications folder" << std::endl;
            std::cout << "3. Wait for Docker Desktop to start completely (whale icon in menu bar)" << std::endl;
            #else
            std::cout << "1. Is Docker installed? Run 'which docker' to verify the path." << std::endl;
            std::cout << "2. Is the Docker daemon running? Try 'systemctl status docker' or 'dockerd'." << std::endl;
            std::cout << "3. Do you have proper permissions? Try running with sudo or add your user to the docker group." << std::endl;
            #endif
            
            std::cout << "4. If Docker is installed in a custom location, use 'docker.update docker_path /path/to/docker'." << std::endl;
            return false;
        }
    }
    
    bool listContainers(std::queue<std::string>& args) {
        std::string cmd = settings["docker_path"] + " ps";
        
        // Add -a to show all containers including stopped ones
        if (!args.empty() && args.front() == "-a") {
            cmd += " -a";
            args.pop();
        }
        
        std::cout << executeCommand(cmd);
        return true;
    }
    
    bool listImages(std::queue<std::string>& args) {
        std::string cmd = settings["docker_path"] + " images";
        
        // Parse any additional arguments
        auto parsedArgs = parseArgs(args);
        for (const auto& arg : parsedArgs) {
            cmd += " " + arg;
        }
        
        std::cout << executeCommand(cmd);
        return true;
    }
    
    bool startContainer(std::queue<std::string>& args) {
        if (args.empty()) {
            std::cout << "Error: Container ID or name required" << std::endl;
            return false;
        }
        
        std::string container = args.front();
        args.pop();
        
        std::cout << "Starting container " << container << "..." << std::endl;
        std::string result = executeCommand(settings["docker_path"] + " start " + container);
        std::cout << result;
        
        return !result.empty();
    }
    
    bool stopContainer(std::queue<std::string>& args) {
        if (args.empty()) {
            std::cout << "Error: Container ID or name required" << std::endl;
            return false;
        }
        
        std::string container = args.front();
        args.pop();
        
        std::string timeout = settings["default_timeout"];
        if (!args.empty()) {
            timeout = args.front();
            args.pop();
        }
        
        std::cout << "Stopping container " << container << "..." << std::endl;
        std::string result = executeCommand(settings["docker_path"] + " stop -t " + timeout + " " + container);
        std::cout << result;
        
        return !result.empty();
    }
    
    bool restartContainer(std::queue<std::string>& args) {
        if (args.empty()) {
            std::cout << "Error: Container ID or name required" << std::endl;
            return false;
        }
        
        std::string container = args.front();
        args.pop();
        
        std::cout << "Restarting container " << container << "..." << std::endl;
        std::string result = executeCommand(settings["docker_path"] + " restart " + container);
        std::cout << result;
        
        return !result.empty();
    }
    
    bool removeContainer(std::queue<std::string>& args) {
        if (args.empty()) {
            std::cout << "Error: Container ID or name required" << std::endl;
            return false;
        }
        
        std::string options;
        while (!args.empty() && args.front().substr(0,1) == "-") {
            options += args.front() + " ";
            args.pop();
        }
        
        if (args.empty()) {
            std::cout << "Error: Container ID or name required" << std::endl;
            return false;
        }
        
        std::string container = args.front();
        args.pop();
        
        std::cout << "Removing container " << container << "..." << std::endl;
        std::string result = executeCommand(settings["docker_path"] + " rm " + options + container);
        std::cout << result;
        
        return !result.empty();
    }
    
    bool removeImage(std::queue<std::string>& args) {
        if (args.empty()) {
            std::cout << "Error: Image ID or name required" << std::endl;
            return false;
        }
        
        std::string options;
        while (!args.empty() && args.front().substr(0,1) == "-") {
            options += args.front() + " ";
            args.pop();
        }
        
        if (args.empty()) {
            std::cout << "Error: Image ID or name required" << std::endl;
            return false;
        }
        
        std::string image = args.front();
        args.pop();
        
        std::cout << "Removing image " << image << "..." << std::endl;
        std::string result = executeCommand(settings["docker_path"] + " rmi " + options + image);
        std::cout << result;
        
        return !result.empty();
    }
    
    bool pullImage(std::queue<std::string>& args) {
        if (args.empty()) {
            std::cout << "Error: Image name required" << std::endl;
            return false;
        }
        
        std::string image = args.front();
        args.pop();
        
        std::cout << "Pulling image " << image << "..." << std::endl;
        std::string result = executeCommand(settings["docker_path"] + " pull " + image);
        std::cout << result;
        
        return !result.empty();
    }
    
    bool showLogs(std::queue<std::string>& args) {
        if (args.empty()) {
            std::cout << "Error: Container ID or name required" << std::endl;
            return false;
        }
        
        std::string options;
        while (!args.empty() && args.front().substr(0,1) == "-") {
            options += args.front() + " ";
            args.pop();
        }
        
        if (args.empty()) {
            std::cout << "Error: Container ID or name required" << std::endl;
            return false;
        }
        
        std::string container = args.front();
        args.pop();
        
        std::cout << "Showing logs for container " << container << "..." << std::endl;
        std::string result = executeCommand(settings["docker_path"] + " logs " + options + " " + container);
        std::cout << result;
        
        return !result.empty();
    }
    
    bool execInContainer(std::queue<std::string>& args) {
        if (args.empty()) {
            std::cout << "Error: Container ID or name required" << std::endl;
            return false;
        }
        
        std::string container = args.front();
        args.pop();
        
        if (args.empty()) {
            std::cout << "Error: Command required" << std::endl;
            return false;
        }
        
        std::string command;
        while (!args.empty()) {
            command += args.front() + " ";
            args.pop();
        }
        
        std::cout << "Executing in container " << container << ": " << command << std::endl;
        std::string result = executeCommand(settings["docker_path"] + " exec " + container + " " + command);
        std::cout << result;
        
        return !result.empty();
    }
    
    bool listNetworks(std::queue<std::string>& args) {
        std::string cmd = settings["docker_path"] + " network ls";
        
        // Parse any additional arguments
        auto parsedArgs = parseArgs(args);
        for (const auto& arg : parsedArgs) {
            cmd += " " + arg;
        }
        
        std::cout << executeCommand(cmd);
        return true;
    }
    
    bool listVolumes(std::queue<std::string>& args) {
        std::string cmd = settings["docker_path"] + " volume ls";
        
        // Parse any additional arguments
        auto parsedArgs = parseArgs(args);
        for (const auto& arg : parsedArgs) {
            cmd += " " + arg;
        }
        
        std::cout << executeCommand(cmd);
        return true;
    }
    
    bool showInfo() {
        std::cout << executeCommand(settings["docker_path"] + " info");
        return true;
    }
    
    bool showStats(std::queue<std::string>& args) {
        std::string cmd = settings["docker_path"] + " stats";
        
        // If container ID is specified, add it
        if (!args.empty()) {
            cmd += " " + args.front();
            args.pop();
        }
        
        // Add --no-stream to get a single snapshot rather than continuous updates
        cmd += " --no-stream";
        
        std::cout << executeCommand(cmd);
        return true;
    }
    
    bool runContainer(std::queue<std::string>& args) {
        if (args.empty()) {
            std::cout << "Error: Image name required" << std::endl;
            return false;
        }
        
        std::string cmdOptions = settings["docker_path"] + " run";
        std::string runCmd;
        
        // Process all arguments
        std::vector<std::string> allArgs;
        while (!args.empty()) {
            allArgs.push_back(args.front());
            args.pop();
        }
        
        // Check if we need to run in background mode
        bool hasDetachFlag = false;
        for (const auto& arg : allArgs) {
            if (arg == "-d" || arg == "--detach") {
                hasDetachFlag = true;
                break;
            }
        }
        
        // Build the command with all options
        for (const auto& arg : allArgs) {
            cmdOptions += " " + arg;
        }
        
        std::cout << "Running container with command: " << cmdOptions << std::endl;
        std::string result = executeCommand(cmdOptions);
        std::cout << result;
        
        if (hasDetachFlag && result.empty()) {
            std::cout << "Container started in detached mode." << std::endl;
        }
        
        return true;
    }

    bool buildImage(std::queue<std::string>& args) {
        if (args.empty()) {
            std::cout << "Error: Build options and path required" << std::endl;
            std::cout << "Usage: docker build [options] -t <tag> <path>" << std::endl;
            return false;
        }
        
        std::string cmd = settings["docker_path"] + " build";
        
        // Build the full command with all options
        std::vector<std::string> allArgs;
        while (!args.empty()) {
            allArgs.push_back(args.front());
            args.pop();
        }
        
        // Build the command with all options
        for (const auto& arg : allArgs) {
            cmd += " " + arg;
        }
        
        std::cout << "Building image with command: " << cmd << std::endl;
        std::string result = executeCommand(cmd);
        std::cout << result;
        
        return true;
    }
};

// Export plugin creation & destruction functions
IMPLEMENT_PLUGIN(DockerManagerPlugin)
