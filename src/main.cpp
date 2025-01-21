#include <iostream>
#include "terminalpassthrough.h"

int main() {
    TerminalPassthrough terminalPassthrough;
    while (true) {
        terminalPassthrough.printCurrentTerminalPosition();
        std::string command;
        std::getline(std::cin, command);
        if (command == "exit") break;
        terminalPassthrough.executeCommand(command).join(); // Ensure the thread is joined
    }
    return 0;
}
