#include "tutorial.h"

#include "cjsh.h"
#include "isocline/isocline.h"

void start_tutorial() {
  std::cout << "Let's get you started with a quick tutorial!" << std::endl;
  std::cout << "If you would like to skip the tutorial (which you can at any "
               "time), please enter: 'tutorial skip'"
            << std::endl;
  std::cout << "If you would like to do the tutorial, please just press your "
               "enter key"
            << std::endl;

  if (!tutorial_input("", " > ")) {
    return;
  }

  std::cout << "In this tutorial, you will learn the basics of using CJSH and "
               "really any shell in general."
            << std::endl;
  std::cout << "This is your system prompt:\n" << std::endl;
  std::cout << g_shell->get_prompt() << "\n" << std::endl;
  std::cout << "It shows you where you are in your shell." << std::endl;
  std::cout << "You can see that we are currently in the " << getenv("PWD")
            << " directory." << std::endl;
  if (getenv("PWD") == getenv("HOME")) {
    std::cout
        << "Or otherwise known as your '~' directory or your HOME directory."
        << std::endl;
    std::cout << "This is a special directory that is unique to each user."
              << std::endl;
    std::cout << "This directory will come up again later." << std::endl;
  }

  std::cout << "Now, let's try some basic commands." << std::endl;
  std::cout
      << "You can use the 'ls' command to list files in the current directory."
      << std::endl;
  std::cout << "Try it out by typing: ls" << std::endl;
  if (!tutorial_input("ls", g_shell->get_prompt())) {
    return;
  }

  std::cout << "\nGreat job! Now you can see all of the files in the current "
               "directory."
            << std::endl;
  std::cout << "Now lets try changing our directory." << std::endl;
  std::cout << "You can use the 'cd' command to change directories."
            << std::endl;
  std::cout << "Lets go UP one directory in our file system." << std::endl;
  std::cout << "Try it out by typing: cd .." << std::endl;
  if (!tutorial_input("cd ..", g_shell->get_prompt())) {
    return;
  }

  std::cout
      << "Great job! We have now gone UP one directory in our file system."
      << std::endl;
  std::cout << "Now lets re-run the 'ls' command to see the files in the new "
               "directory."
            << std::endl;
  if (!tutorial_input("ls", g_shell->get_prompt())) {
    return;
  }

  std::cout
      << "\nAwesome! Now you can see all of the files in the new directory."
      << std::endl;
  std::cout << "You can also see now that our prompt has changed to reflect "
               "the new directory."
            << std::endl;
  std::cout << "This is a helpful feature that allows you to always know your "
               "current location."
            << std::endl;
  std::cout << "Now lets try to go back to your HOME directory." << std::endl;
  std::cout << "No matter where you are or how lost you are, you can always "
               "run 'cd ~' to return to your HOME directory."
            << std::endl;
  std::cout << "Try it out by typing: cd ~" << std::endl;
  if (!tutorial_input("cd ~", g_shell->get_prompt())) {
    return;
  }

  std::cout << "\nGreat job! You are now back in your HOME directory."
            << std::endl;

  std::cout << "Now lets try running the 'help' command." << std::endl;
  std::cout << "Try it out by typing: help" << std::endl;
  if (!tutorial_input("help", g_shell->get_prompt())) {
    return;
  }

  std::cout << "\nGreat job! You have completed the tutorial." << std::endl;
  std::cout << "To see all basic shell commands, you can always just type: help"
            << std::endl;
}

bool tutorial_input(const std::string& expected_input,
                    const std::string& prompt) {
  char* input;
  while (true) {
    input = ic_readline(prompt.c_str());
    if (input) {
      if (strcmp(input, "tutorial skip") == 0) {
        std::cout << "Skipping tutorial..." << std::endl;
        free(input);
        return false;
      }
      if (strcmp(input, expected_input.c_str()) == 0) {
        g_shell->execute(input);
        free(input);
        return true;
      } else {
        std::cout << "It looks like you entered: " << input << std::endl;
        std::cout << "That's not quite right. Give it another try!"
                  << std::endl;
        free(input);
        continue;
      }
    }
  }
}
