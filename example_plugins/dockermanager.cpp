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
    
public:
    DockerManagerPlugin() {
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
        std::string dockerVersion = executeCommand(settings["docker_path"] + " --version");
        if (dockerVersion.find("Docker version") == std::string::npos) {
            std::cerr << "Docker is not installed or not in PATH. Plugin initialization failed." << std::endl;
            return false;
        }
        
        std::string dockerRunning = executeCommand(settings["docker_path"] + " info");
        if (dockerRunning.find("ERROR") != std::string::npos) {
            std::cerr << "Docker daemon is not running. Plugin initialization failed." << std::endl;
            return false;
        }
        
        std::cout << "Docker Manager plugin initialized successfully." << std::endl;
        std::cout << "Docker version: " << dockerVersion;
        return true;
    }
    
    void shutdown() override {
        std::cout << "Docker Manager plugin shutting down." << std::endl;
    }
    
    bool handleCommand(std::queue<std::string>& args) override {
        if (args.empty()) {
            std::cout << "Docker Manager usage: docker [command] [options]" << std::endl;
            std::cout << "Use 'docker help' for available commands." << std::endl;
            return true;
        }
        
        std::string command = args.front();
        args.pop();
        
        if (command == "help") {
            showHelp();
            return true;
        }
        else if (command == "ps" || command == "containers") {
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
        else {
            std::cout << "Unknown Docker command: " << command << std::endl;
            return false;
        }
    }
    
    std::vector<std::string> getCommands() const override {
        std::vector<std::string> commands;
        commands.push_back("docker");
        return commands;
    }
    
    std::map<std::string, std::string> getDefaultSettings() const override {
        return settings;
    }
    
    void updateSetting(const std::string& key, const std::string& value) override {
        settings[key] = value;
        std::cout << "Docker Manager setting updated: " << key << " = " << value << std::endl;
    }
    
private:
    void showHelp() {
        std::cout << "Docker Manager Plugin Commands:\n" << std::endl;
        std::cout << "  docker ps|containers [options]   List containers" << std::endl;
        std::cout << "  docker images [options]          List images" << std::endl;
        std::cout << "  docker start <container>         Start a container" << std::endl;
        std::cout << "  docker stop <container>          Stop a container" << std::endl;
        std::cout << "  docker restart <container>       Restart a container" << std::endl;
        std::cout << "  docker rm <container>            Remove a container" << std::endl;
        std::cout << "  docker rmi <image>               Remove an image" << std::endl;
        std::cout << "  docker pull <image>              Pull an image" << std::endl;
        std::cout << "  docker logs <container> [options] Show container logs" << std::endl;
        std::cout << "  docker exec <container> <command> Run a command in a container" << std::endl;
        std::cout << "  docker run [options] <image>     Run a new container" << std::endl;
        std::cout << "  docker networks [options]        List networks" << std::endl;
        std::cout << "  docker volumes                   List volumes" << std::endl;
        std::cout << "  docker info                      Show system-wide information" << std::endl;
        std::cout << "  docker stats [container]         Show container resource usage" << std::endl;
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
        
        // Build the command with all options
        std::vector<std::string> allArgs = parseArgs(args);
        for (const auto& arg : allArgs) {
            cmdOptions += " " + arg;
        }
        
        std::cout << "Running container with command: " << cmdOptions << std::endl;
        std::string result = executeCommand(cmdOptions);
        std::cout << result;
        
        return !result.empty();
    }
};

// Export plugin creation & destruction functions
IMPLEMENT_PLUGIN(DockerManagerPlugin)
