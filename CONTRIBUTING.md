# Contributing to CJ's Shell

Thank you for your interest in contributing to CJ's Shell (cjsh)! This document provides guidelines and instructions for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
  - [Project Setup](#project-setup)
  - [Building the Project](#building-the-project)
- [How to Contribute](#how-to-contribute)
  - [Reporting Bugs](#reporting-bugs)
  - [Suggesting Enhancements](#suggesting-enhancements)
  - [Pull Requests](#pull-requests)
- [Development Guidelines](#development-guidelines)
  - [Coding Style](#coding-style)
  - [Testing](#testing)
  - [Documentation](#documentation)
- [Project Structure](#project-structure)
- [Creating Plugins](#creating-plugins)
- [Creating Themes](#creating-themes)

## Code of Conduct

By participating in this project, you are expected to uphold our Code of Conduct, which is to treat all contributors with respect and foster an inclusive environment.

## Getting Started

### Project Setup

1. Fork the repository on GitHub
2. Clone your fork to your local machine:
   ```bash
   git clone https://github.com/YOUR-USERNAME/CJsShell.git
   cd CJsShell
   ```
3. Add the original repository as an upstream remote:
   ```bash
   git remote add upstream https://github.com/CadenFinley/CJsShell.git
   ```

### Building the Project

To build the project locally:

```bash
mkdir build && cd build
cmake ..
make
```

## How to Contribute

### Reporting Bugs

If you find a bug, please create an issue on the GitHub repository with the following information:

- A clear, descriptive title
- Steps to reproduce the issue
- Expected behavior versus actual behavior
- Your environment information (OS, shell version, etc.)
- Any relevant logs or error messages

### Suggesting Enhancements

We welcome suggestions for new features! Please include:

- A clear description of the feature
- Why it would be valuable to the project
- Any implementation ideas you may have

### Pull Requests

1. Create a new branch for your feature or bugfix:
   ```bash
   git checkout -b feature/your-feature-name
   ```
   or
   ```bash
   git checkout -b fix/issue-you-are-fixing
   ```

2. Make your changes with proper commits
3. Push to your fork:
   ```bash
   git push origin feature/your-feature-name
   ```
4. Create a pull request to the `master` branch of the original repository
5. Include a description of your changes in the pull request

## Development Guidelines

### Coding Style

Please follow these guidelines when writing code:

- Use consistent indentation (spaces, not tabs)
- Keep lines to a reasonable length (80-120 characters)
- Write clear, descriptive comments
- Follow C++ best practices
- Use meaningful variable and function names

This project uses clang-format to enforce consistent code style. Before submitting a pull request, please run the following command from the project root to format your code:

```bash
clang-format -i src/*/*.cpp && clang-format -i src/*.cpp && clang-format -i include/*.h && clang-format -i include/*/*.h && clang-format -i plugins/*/*.cpp
```

This will automatically format all source files according to the project's style guidelines.

### Testing

- Add tests for new features
- Ensure all tests pass before submitting a pull request
- Run existing tests with:
  ```bash
  cd build
  make test
  ```

### Documentation

- Update documentation for any changed functionality
- Add documentation for new features
- Include comments in your code for complex logic
- Update the README.md if necessary

## Project Structure

The repository is organized as follows:

- `/include`: Header files
- `/src`: Source files
- `/plugins`: Plugin system
- `/themes`: Theme files
- `/tests`: Test files
- `/build`: Build output (not committed)
- `/vendor`: Any thrid party build dependencies that can be staticly linked
- `/tool-scripts`: Any scripts used for building or any general automation


## Creating Plugins

CJ's Shell supports a powerful plugin system. To create a plugin:

1. Examine the existing plugins in the `/plugins` directory for reference
2. Plugins can be written in any language that can compile to a shared library
3. Use the Plugin API defined in `include/pluginapi.h`
4. Follow the plugin documentation in the README

Example plugin structure:
```
my_plugin/
├── CMakeLists.txt
├── src/
│   └── my_plugin.cpp
└── README.md
```

## Creating Themes

Themes in CJ's Shell are defined using JSON. To create a theme:

1. Examine existing themes in the `/themes` directory
2. Create a new JSON file with your theme definition
3. Include all required theme elements
4. Test your theme by placing it in the .config/cjsh/themes directory

Example theme structure:
```json
{
    "ai_segments": [
        {
            "bg_color": "RESET",
            "content": " {AI_MODEL} ",
            "fg_color": "#FF55FF",
            "separator": " / ",
            "separator_bg": "RESET",
            "separator_fg": "#FFFFFF",
            "tag": "model"
        },
        {
            "bg_color": "RESET",
            "content": "{AI_AGENT_TYPE} ",
            "fg_color": "#55FFFF",
            "separator": "",
            "separator_bg": "RESET",
            "separator_fg": "RESET",
            "tag": "mode"
        }
    ],
    "fill_bg_color": "RESET",
    "fill_char": "",
    "fill_fg_color": "RESET",
    "git_segments": [
        {
            "bg_color": "RESET",
            "content": " {LOCAL_PATH} ",
            "fg_color": "#55FF55",
            "separator": " ",
            "separator_bg": "RESET",
            "separator_fg": "#FFFFFF",
            "tag": "path"
        },
        {
            "bg_color": "RESET",
            "content": "{GIT_BRANCH}",
            "fg_color": "#FFFF55",
            "separator": "",
            "separator_bg": "RESET",
            "separator_fg": "RESET",
            "tag": "branch"
        },
        {
            "bg_color": "RESET",
            "content": "{GIT_STATUS}",
            "fg_color": "#FF5555",
            "separator": " $ ",
            "separator_bg": "RESET",
            "separator_fg": "#FFFFFF",
            "tag": "status"
        }
    ],
    "newline_segments": [],
    "ps1_segments": [
        {
            "bg_color": "RESET",
            "content": "{USERNAME}@{HOSTNAME}:",
            "fg_color": "#5555FF",
            "separator": "",
            "separator_bg": "RESET",
            "separator_fg": "RESET",
            "tag": "username"
        },
        {
            "bg_color": "RESET",
            "content": " {DIRECTORY} ",
            "fg_color": "#55FF55",
            "separator": " ",
            "separator_bg": "RESET",
            "separator_fg": "#FFFFFF",
            "tag": "directory"
        },
        {
            "bg_color": "RESET",
            "content": "$ ",
            "fg_color": "#FFFFFF",
            "separator": "",
            "separator_bg": "RESET",
            "separator_fg": "RESET",
            "tag": "prompt"
        }
    ],
    "terminal_title": "{PATH}"
}
```

---
