#include "prompt.h"

Prompt::Prompt() {
  // Constructor
}
Prompt::~Prompt() {
  // Destructor
}


std::string Prompt::get_prompt() {
  
  return "cjsh> ";
}
std::string Prompt::get_ai_prompt() {
  return "AI> ";
}