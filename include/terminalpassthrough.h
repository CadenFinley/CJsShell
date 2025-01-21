#ifndef TERMINALPASSTHROUGH_H
#define TERMINALPASSTHROUGH_H

#include <string>
#include <thread>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

class TerminalPassthrough {
public:
    TerminalPassthrough();  // Constructor declaration

    std::string getTerminalName();
    void setDisplayWholePath(bool displayWholePath);
    void printCurrentTerminalPosition();
    std::string returnCurrentTerminalPosition();
    std::thread executeCommand(std::string command); // Update declaration
    void toggleDisplayWholePath();
    bool isDisplayWholePath();
    std::vector<std::string> getTerminalCacheUserInput();
    std::vector<std::string> getTerminalCacheTerminalOutput();
    void clearTerminalCache();
    std::string returnMostRecentUserInput();
    std::string returnMostRecentTerminalOutput();

private:
    std::string currentDirectory;
    bool displayWholePath;
    std::vector<std::string> terminalCacheUserInput;
    std::vector<std::string> terminalCacheTerminalOutput;

    std::string getCurrentFilePath();
    std::string getCurrentFileName();
    bool isRootPath(const fs::path& path);
};

#endif // TERMINALPASSTHROUGH_H
