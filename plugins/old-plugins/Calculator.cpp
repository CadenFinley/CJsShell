#include <cctype>
#include <cmath>
#include <iostream>
#include <map>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

#include "../src/include/plugininterface.h"

class Calculator : public PluginInterface {
 private:
  std::map<std::string, double> memory;
  double lastResult;

  bool isOperator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '^';
  }

  int getPrecedence(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    if (op == '^') return 3;
    return 0;
  }

  double applyOperator(double a, double b, char op) {
    switch (op) {
      case '+':
        return a + b;
      case '-':
        return a - b;
      case '*':
        return a * b;
      case '/':
        if (b == 0) throw std::runtime_error("Division by zero");
        return a / b;
      case '^':
        return std::pow(a, b);
      default:
        return 0;
    }
  }

  double evaluateExpression(const std::string& expr) {
    std::stack<double> values;
    std::stack<char> ops;

    std::string token;
    std::istringstream tokenStream(expr);

    while (std::getline(tokenStream, token, ' ')) {
      if (token.empty()) continue;
      if (token == "sin") {
        std::string arg;
        std::getline(tokenStream, arg, ' ');
        values.push(
            std::sin(std::stod(arg) * M_PI / 180.0));  // Assuming degrees
      } else if (token == "cos") {
        std::string arg;
        std::getline(tokenStream, arg, ' ');
        values.push(
            std::cos(std::stod(arg) * M_PI / 180.0));  // Assuming degrees
      } else if (token == "tan") {
        std::string arg;
        std::getline(tokenStream, arg, ' ');
        values.push(
            std::tan(std::stod(arg) * M_PI / 180.0));  // Assuming degrees
      } else if (token == "sqrt") {
        std::string arg;
        std::getline(tokenStream, arg, ' ');
        double value = std::stod(arg);
        if (value < 0)
          throw std::runtime_error("Square root of negative number");
        values.push(std::sqrt(value));
      } else if (token == "log") {
        std::string arg;
        std::getline(tokenStream, arg, ' ');
        double value = std::stod(arg);
        if (value <= 0) throw std::runtime_error("Log of non-positive number");
        values.push(std::log10(value));
      } else if (token == "ln") {
        std::string arg;
        std::getline(tokenStream, arg, ' ');
        double value = std::stod(arg);
        if (value <= 0)
          throw std::runtime_error("Natural log of non-positive number");
        values.push(std::log(value));
      } else if (token == "pi") {
        values.push(M_PI);
      } else if (token == "e") {
        values.push(M_E);
      } else if (token == "ans") {
        values.push(lastResult);
      } else if (memory.find(token) != memory.end()) {
        values.push(memory[token]);
      } else if (isOperator(token[0])) {
        while (!ops.empty() &&
               getPrecedence(ops.top()) >= getPrecedence(token[0])) {
          double val2 = values.top();
          values.pop();
          double val1 = values.top();
          values.pop();
          char op = ops.top();
          ops.pop();
          values.push(applyOperator(val1, val2, op));
        }
        ops.push(token[0]);
      } else {
        try {
          values.push(std::stod(token));
        } catch (const std::exception& e) {
          throw std::runtime_error("Invalid token: " + token);
        }
      }
    }

    while (!ops.empty()) {
      double val2 = values.top();
      values.pop();
      double val1 = values.top();
      values.pop();
      char op = ops.top();
      ops.pop();
      values.push(applyOperator(val1, val2, op));
    }

    if (values.size() != 1) throw std::runtime_error("Invalid expression");

    return values.top();
  }

 public:
  Calculator() : lastResult(0) {}
  ~Calculator() throw() {}

  std::string getName() const { return "Calculator"; }

  std::string getVersion() const { return "1.0"; }

  std::string getDescription() const {
    return "A scientific calculator plugin for DevToolsTerminal.";
  }

  std::string getAuthor() const { return "Caden Finley"; }

  bool initialize() { return true; }

  void shutdown() { return; }

  bool handleCommand(std::queue<std::string>& args) {
    if (args.empty()) return false;

    std::string cmd = args.front();
    args.pop();

    if (cmd == "calc" || cmd == "c") {
      if (args.empty()) {
        std::cout << "Usage: calc <expression>" << std::endl;
        std::cout << "Example: calc 2 + 2" << std::endl;
        std::cout << "Available functions: sin, cos, tan, sqrt, log, ln"
                  << std::endl;
        std::cout << "Constants: pi, e, ans (last result)" << std::endl;
        return true;
      }

      std::string expression;
      while (!args.empty()) {
        expression += args.front() + " ";
        args.pop();
      }

      try {
        lastResult = evaluateExpression(expression);
        std::cout << lastResult << std::endl;
      } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
      }
      return true;
    } else if (cmd == "store" || cmd == "s") {
      if (args.size() < 2) {
        std::cout << "Usage: store <variable_name> <value>" << std::endl;
        return true;
      }

      std::string varName = args.front();
      args.pop();

      std::string valueStr;
      while (!args.empty()) {
        valueStr += args.front() + " ";
        args.pop();
      }

      try {
        double value = evaluateExpression(valueStr);
        memory[varName] = value;
        std::cout << "Stored: " << varName << " = " << value << std::endl;
      } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
      }
      return true;
    } else if (cmd == "vars" || cmd == "v") {
      if (memory.empty()) {
        std::cout << "No variables stored." << std::endl;
      } else {
        std::cout << "Stored variables:" << std::endl;
        for (std::map<std::string, double>::const_iterator it = memory.begin();
             it != memory.end(); ++it) {
          std::cout << it->first << " = " << it->second << std::endl;
        }
      }
      return true;
    } else if (cmd == "clear" || cmd == "clr") {
      memory.clear();
      lastResult = 0;
      std::cout << "Memory cleared." << std::endl;
      return true;
    }

    return false;
  }

  std::vector<std::string> getCommands() const {
    std::vector<std::string> cmds;
    cmds.push_back("calc");
    cmds.push_back("c");
    cmds.push_back("store");
    cmds.push_back("s");
    cmds.push_back("vars");
    cmds.push_back("v");
    cmds.push_back("clear");
    cmds.push_back("clr");
    return cmds;
  }

  int getInterfaceVersion() const { return 1; }

  std::vector<std::string> getSubscribedEvents() const { return {}; }

  std::map<std::string, std::string> getDefaultSettings() const {
    std::map<std::string, std::string> settings;
    settings.insert(std::make_pair("angle_unit", "degrees"));
    settings.insert(std::make_pair("precision", "4"));
    return settings;
  }

  void updateSetting(const std::string& key, const std::string& value) {
    std::cout << "Calculator updated setting " << key << " to " << value
              << std::endl;
  }
};

PLUGIN_API PluginInterface* createPlugin() { return new Calculator(); }
PLUGIN_API void destroyPlugin(PluginInterface* plugin) { delete plugin; }
