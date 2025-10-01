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
    ./toolchain/build.sh
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
    find . -name "*.cpp" -o -name "*.h" -o -name "*.c" -o -name "*.hpp" | grep -v "build/" | grep -v "CMakeFiles/" | grep -v "_deps/" | xargs clang-format -i
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
- `/plugins`: Plugin system and example plugins
- `/themes`: Theme files
- `/tests`: Test files
- `/build`: Build output (not committed)
- `/toolchain`: Tool chain for cjsh


## Creating Plugins

CJ's Shell supports a powerful plugin system. To create a plugin:

1. Examine the existing plugins in the `/plugins` directory for reference
2. Plugins can be written in C, C++, or Rust using the provided examples
3. Use the Plugin API defined in `include/pluginapi.h`
4. Follow the plugin documentation in the `/plugins/README.md`

Example plugin structure:
```
my_plugin/
├── build.sh                # Required build script
├── src/
│   └── my_plugin.cpp
├── include/
│   └── pluginapi.h         # Copy of the latest plugin API header
└── README.md
```

Plugins must implement all required functions from the Plugin API, including:
- `plugin_get_info`
- `plugin_initialize`
- `plugin_shutdown`
- `plugin_handle_command`
- `plugin_get_commands`
- `plugin_get_subscribed_events`
- `plugin_get_default_settings`
- `plugin_update_setting`
- `plugin_free_memory`

## Creating Themes

CJ's Shell features a powerful theming system that allows you to customize the appearance of your shell prompt, colors, and other visual elements. Themes are defined in `.cjsh` files using a structured configuration syntax.

### Theme File Structure

Theme files should be placed in the `/themes` directory and follow the naming convention `theme_name.cjsh`. Each theme file begins with:

```bash
#! usr/bin/env cjsh

theme_definition {
  # Theme configuration goes here
}
```

### Core Theme Components

#### 1. Terminal Title
Set the terminal window title:
```bash
terminal_title "{SHELL} {SHELL_VER} | {USERNAME}@{HOSTNAME}"
```

#### 2. Fill Character
Define the background fill character and colors:
```bash
fill {
  char "",
  fg RESET
  bg RESET
}
```

#### 3. Primary Prompt (ps1)
Define the main prompt structure with segments:
```bash
ps1 {
  segment "username" {
    content "{USERNAME}@{HOSTNAME}:"
    fg "#5555FF"
    bg "RESET"
  }
  segment "directory" {
    content " {DIRECTORY} "
    fg "#55FF55"
    bg "RESET"
    separator " "
    separator_fg "#FFFFFF"
    separator_bg "RESET"
  }
  segment "prompt" {
    content "$ "
    fg "#FFFFFF"
    bg "RESET"
  }
}
```

#### 4. Git Integration
Add Git-aware segments for repository information:
```bash
git_segments {
  segment "branch" {
    content "{GIT_BRANCH}"
    fg "#FFFF55"
    bg "RESET"
  }
  segment "status" {
    content "{GIT_STATUS}"
    fg "#FF5555"
    bg "RESET"
  }
}
```

### Advanced Features

#### Requirements
Specify font and color requirements:
```bash
requirements {
  colors "true_color"
  fonts "FiraCode Nerd Font"
  fonts "Hack Nerd Font"
}
```

#### Variables
Define reusable variables for complex logic:
```bash
variables {
  project_type_bg "{if = {IS_PYTHON_PROJECT} == 'true' ? #504945 : #665c54}"
  project_language_badge "🐍 {PYTHON_VERSION}"
}
```

### Available Variables

CJ's Shell provides numerous built-in variables for dynamic content:

- **User Info**: `{USERNAME}`, `{HOSTNAME}`, `{UID}`, `{GID}`
- **Directory**: `{DIRECTORY}`, `{PATH}`, `{LOCAL_PATH}`
- **Shell**: `{SHELL}`, `{SHELL_VER}`, `{SHELL_PID}`
- **Git**: `{GIT_BRANCH}`, `{GIT_STATUS}`, `{GIT_AHEAD}`, `{GIT_BEHIND}`
- **Project Detection**: `{IS_PYTHON_PROJECT}`, `{IS_NODEJS_PROJECT}`, `{IS_RUST_PROJECT}`, etc.
- **Language Versions**: `{PYTHON_VERSION}`, `{NODEJS_VERSION}`, `{JAVA_VERSION}`, etc.
- **Time**: `{TIME}`, `{DATE}`, `{TIME_24H}`
- **System**: `{OS}`, `{ARCH}`, `{KERNEL}`

### Color Specification

Colors can be specified in multiple formats:
- **Hex**: `#FF5555`, `#AABBCC`
- **Named**: `RESET`, `BLACK`, `RED`, `GREEN`, `YELLOW`, `BLUE`, `MAGENTA`, `CYAN`, `WHITE`
- **ANSI**: `30-37` (foreground), `40-47` (background)
- **256-color**: `0-255`

### Best Practices

1. **Test thoroughly**: Test your theme in different scenarios (various directories, Git repos, different projects)
2. **Consider accessibility**: Ensure sufficient contrast for readability
3. **Font requirements**: If using special characters or symbols, specify required fonts in the `requirements` section
4. **Performance**: Avoid overly complex conditional logic that might slow down prompt rendering
5. **Fallbacks**: Provide fallback content for unsupported terminals or missing fonts

### Example Themes

Study the existing themes for reference:
- `default.cjsh`: Simple, universal theme
- `gruvbox_dark_simple.cjsh`: Modern theme with project detection
- `powerline_aligned.cjsh`: Advanced powerline-style theme

## Contributing Themes

When contributing a new theme to CJ's Shell:

1. **Create your theme file** in the `/themes` directory following the naming convention
2. **Test extensively** across different environments and use cases
3. **Document requirements** (fonts, color support) in the theme file
4. **Provide a screenshot** or example output in your pull request
5. **Include a brief description** of the theme's purpose and style
6. **Ensure compatibility** with both basic and advanced terminal features

### Theme Submission Checklist

- [ ] Theme file follows proper `.cjsh` syntax
- [ ] All required segments are properly defined
- [ ] Colors are specified correctly and provide good contrast
- [ ] Font requirements (if any) are documented
- [ ] Theme has been tested in multiple scenarios
- [ ] No syntax errors or parsing issues
- [ ] Theme name is descriptive and unique

By following these steps, you help maintain the quality and consistency of themes in CJ's Shell. Thank you for your contribution!
