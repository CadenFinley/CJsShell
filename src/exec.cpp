#include "exec.h"

Exec::Exec() {
  // Constructor
}
Exec::~Exec() {
  // Destructor
}
void Exec::execute_command_sync(const std::string& command) {
  std::cout << command << std::endl;
}
void Exec::execute_command_async(const std::string& command) {
  std::cout << command << std::endl;
}