```plaintext
   ______    _______ __  __
  / ____/   / / ___// / / /
 / /   __  / /\__ \/ /_/ / 
/ /___/ /_/ /___/ / __  /  
\____/\____//____/_/ /_/   
```

![Version](https://img.shields.io/github/v/release/CadenFinley/CJsShell?label=version&color=blue)
[![Build status](https://ci.appveyor.com/api/projects/status/5m6bgk8lxf3ge256/branch/master?svg=true)](https://ci.appveyor.com/project/CadenFinley/cjsshell/branch/master)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/4e33a26accb6450da43c91c7b8e872e7)](https://app.codacy.com/gh/CadenFinley/CJsShell/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)

## CJ's Shell

CJ's Shell (cjsh) is a custom login shell with out of the box power. It comes with features like a built in AI assistant who only offers help when you ask for it, a super power plugin engine with a super versitile, language agnostic plugin api, super customizable themes, and a vibrant color engine.

> ⚠️ **WARNING**: This project is still in active development. There are known bugs. Please do not use this unless you are okay with random issues maybe. idk open a pull request or issue if you run into them thanks

## Installation

### MACOS Install

This installs cjsh from a custom brew tap hosted at: https://github.com/CadenFinley/homebrew-tap

```bash
brew install cadenfinley/tap/cjsh
```

### LINUX Install

To install cjsh on your Linux distribution:

1. First follow the manual installation steps below to build the project and then navigate to the tool-scripts directory in the root of the repo
2. Make the installation script executable:
   ```bash
   chmod +x ./tool-scripts/linux_install_from_local.sh
   ```
3. After the build is complete, run the installation script with sudo privileges:
   ```bash
   sudo ./tool-scripts/linux_install_from_local.sh
   ```

This script will:
- Install the cjsh binary to `/usr/local/bin/`
- Install the man page if available
- Create the necessary `.cjsh` directory structure in your home folder
- Show instructions for setting cjsh as your default shell

### Manual Installation

You can also build and install from source:

```bash
# Clone the repository
git clone https://github.com/CadenFinley/CJsShell.git
cd CJsShell

# Build the project
mkdir build && cd build
cmake ..
make
```

## Usage

### Launching cjsh
Start an interactive session:
```bash
cjsh
```

Start a login session:
```bash
cjsh --login
```
Or run a one‑off command:
```bash
cjsh -c "ls -la"
```
Available startup flags:
- `-l`, `--login` Start in login mode  
- `--set-as-shell` Show instructions to set cjsh as your login shell  
- `--no-update` Disable automatic update checks  
- `-d`, `--debug` Enable debug logging  

### Common Built‑In Commands
- `help` Display help and usage information  
- `cd [dir]` Change directory  
- `export VAR=val` Set environment variables  
- `plugin [subcommand]` Manage plugins (`available`, `enable`, `disable`, `install`, `uninstall`, etc.)  
- `theme [subcommand]` Manage themes (`list`, `set`, `current`, etc.)  
- `aihelp` Invoke the AI assistant  
- `version` Show the current cjsh version  

### Configuration Files
- `~/.cjprofile` Login‑mode setup (env vars, PATH, startup args)  
- `~/.cjshrc` Interactive‑mode setup (aliases, functions, themes, plugins)  

### Tips
- Use ↑/↓ to cycle through your history  
- Run `source ~/.cjshrc` to reload your interactive config  
- Scripts can be executed with `cjsh -c "/path/to/script.sh"`  

## Third‑Party Components

- **isocline**  
  https://github.com/daanx/isocline (MIT License)
 
- **nlohmann/json**  
  https://github.com/nlohmann/json (MIT License)
 
- **libcurl**  
  https://github.com/curl/curl (MIT License)
 
- **OpenSSL** (optional, for SSL support)  
  https://www.openssl.org (Apache License 2.0)
 
- **GLib (glib-2.0)**  
  https://developer.gnome.org/glib (LGPL 2.0+)

## License

This project is licensed under the MIT License.

## Author

Caden Finley @ Abilene Christian University (c) 2025
