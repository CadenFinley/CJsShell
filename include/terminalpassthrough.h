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

    /**
     * @brief Get the name of the terminal based on the operating system.
     * @return Terminal name as a string.
     */
    std::string getTerminalName();

    /**
     * @brief Set whether to display the whole path or just the current directory name.
     * @param displayWholePath Boolean flag to set the display mode.
     */
    void setDisplayWholePath(bool displayWholePath);

    /**
     * @brief Print the current terminal position to the console.
     */
    void printCurrentTerminalPosition();

    /**
     * @brief Return the current terminal position as a string.
     * @return Current terminal position.
     */
    std::string returnCurrentTerminalPosition();

    /**
     * @brief Execute a command in the terminal.
     * @param command Command to execute.
     * @return A thread running the command.
     */
    std::thread executeCommand(std::string command); // Update declaration

    /**
     * @brief Toggle the display mode between whole path and current directory name.
     */
    void toggleDisplayWholePath();

    /**
     * @brief Check if the display mode is set to show the whole path.
     * @return True if displaying the whole path, false otherwise.
     */
    bool isDisplayWholePath();

    /**
     * @brief Get the user input cache.
     * @return Vector of user input strings.
     */
    std::vector<std::string> getTerminalCacheUserInput();

    /**
     * @brief Get the terminal output cache.
     * @return Vector of terminal output strings.
     */
    std::vector<std::string> getTerminalCacheTerminalOutput();

    /**
     * @brief Clear the terminal cache for both user input and terminal output.
     */
    void clearTerminalCache();

    /**
     * @brief Return the most recent user input from the cache.
     * @return Most recent user input string.
     */
    std::string returnMostRecentUserInput();

    /**
     * @brief Return the most recent terminal output from the cache.
     * @return Most recent terminal output string.
     */
    std::string returnMostRecentTerminalOutput();

private:
    std::string currentDirectory;
    bool displayWholePath;
    std::vector<std::string> terminalCacheUserInput;
    std::vector<std::string> terminalCacheTerminalOutput;

    /**
     * @brief Get the current file path.
     * @return Current file path as a string.
     */
    std::string getCurrentFilePath();

    /**
     * @brief Get the current file name.
     * @return Current file name as a string.
     */
    std::string getCurrentFileName();

    /**
     * @brief Check if the given path is the root path.
     * @param path Filesystem path to check.
     * @return True if the path is the root path, false otherwise.
     */
    bool isRootPath(const fs::path& path);
};

#endif // TERMINALPASSTHROUGH_H
