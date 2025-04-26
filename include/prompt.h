#pragma once
#include <iostream>
#include <string>

// this returns the prompt for the user
class Prompt {
  public:
    Prompt();
    ~Prompt();
    std::string get_prompt();
    std::string get_ai_prompt();
};