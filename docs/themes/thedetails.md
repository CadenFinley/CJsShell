# Themes

Programming and designing themes uses its own proprietary cjsh DSL language inspired by json and ruby. I know, it's cursed, but it works, it's pretty flexible and fairly easy to write and I'm stuck with it so yeah. Themes are data oriented, hierarchical, and strongly typed. Upon loading a theme, the theme is stored in a theme cache to avoid repeated file reads for themes. Also many prompt info variables are cached to avoid repeated unneeded calculations and executions for information. Any and all white space defined in content areas or in conditionals is rendered in the main prompt output. Frequently I brag about my themes being 2-4 times faster than starship and powerlevel10k at fastest, but you do have to create the themes in a crappy, custom scripting language so there are trade offs. 

## Loading Themes

To load a theme file, use the `source` command:

```bash
source path/to/theme.cjsh
```

For automatic loading on shell startup, add the source command to your `~/.cjshrc` file:

```bash
# In ~/.cjshrc
source ~/mythemes/gruvbox_dark.cjsh
```

Alternatively, theme definitions inlined in .cjshrc will be automatically loaded

Theme files included with cjsh can be found in the `themes/` directory of the repository.

### Theme Definition

Themes are started with a beginning structure:

```bash
theme_definition [optional name] {

}
```

> **Heads up:** Comment syntax (for example lines beginning with `#`) isn't currently supported inside a theme definition. Including comment markers will usually cause the parser to error out.

### Prompt Definitions and Theme Structure

Themes are made up of different prompt types: PS1 and GIT. The PS1 is the default prompt that is used at all times except when within a git repo, at which point the GIT prompt is used. There are two more additional prompts that have special features. Those prompts are the newline and inline_right prompts. It does not matter the order in which each prompt is defined in the theme file. The terminal window title can also be defined here in the theme file.

```bash
theme_definition "theme_name" {
  variables {

    }

    terminal_title ""

    fill {

    }

    ps1 {

    }

    git {

    }

    inline_right {

    }

    newline {

    }

    behavior {

    }

}
```

### Theme Variables

Then structure and behaviors are designed inside. Variables can be used to define commonly used characters, colors, conditional structures, and prompt segments. Comments are not supported anywhere in theme files right now, so avoid adding `#` or other comment-like markers.

```bash
variables {
    project_type_bg "{if = {IS_PYTHON_PROJECT} == 'true' ? #504945 : {if = {IS_NODEJS_PROJECT} == 'true' ? #504945 : {if = {IS_RUST_PROJECT} == 'true' ? #504945 : {if = {IS_GOLANG_PROJECT} == 'true' ? #504945 : {if = {IS_JAVA_PROJECT} == 'true' ? #504945 : {if = {IS_CPP_PROJECT} == 'true' ? #504945 : {if = {IS_CSHARP_PROJECT} == 'true' ? #504945 : {if = {IS_PHP_PROJECT} == 'true' ? #504945 : {if = {IS_RUBY_PROJECT} == 'true' ? #504945 : {if = {IS_KOTLIN_PROJECT} == 'true' ? #504945 : {if = {IS_SWIFT_PROJECT} == 'true' ? #504945 : {if = {IS_DART_PROJECT} == 'true' ? #504945 : {if = {IS_SCALA_PROJECT} == 'true' ? #504945 : #665c54}}}}}}}}}}}}}"
    project_language_badge " {if = {IS_PYTHON_PROJECT} == 'true' ?🐍 {PYTHON_VERSION} :}{if = {IS_NODEJS_PROJECT} == 'true' ?⚡ {NODEJS_VERSION} :}{if = {IS_RUST_PROJECT} == 'true' ?🦀 {RUST_VERSION} :}{if = {IS_GOLANG_PROJECT} == 'true' ?🐹 {GOLANG_VERSION} :}{if = {IS_JAVA_PROJECT} == 'true' ?☕ {JAVA_VERSION} :}{if = {IS_CPP_PROJECT} == 'true' ?⚙️ {CPP_VERSION} :}{if = {IS_CSHARP_PROJECT} == 'true' ?🔷 {CSHARP_VERSION} :}{if = {IS_PHP_PROJECT} == 'true' ?🐘 {PHP_VERSION} :}{if = {IS_RUBY_PROJECT} == 'true' ?💎 {RUBY_VERSION} :}{if = {IS_KOTLIN_PROJECT} == 'true' ?🎯 {KOTLIN_VERSION} :}{if = {IS_SWIFT_PROJECT} == 'true' ?🦉 {SWIFT_VERSION} :}{if = {IS_DART_PROJECT} == 'true' ?🎯 {DART_VERSION} :}{if = {IS_SCALA_PROJECT} == 'true' ?📈 {SCALA_VERSION} :}"

    segment "shared_user_segment" {
      content " {USERNAME} "
      fg "#ebdbb2"
      bg "#282828"
      separator "\uE0B0"
      separator_fg "#282828"
      separator_bg "#3c3836"
      forward_separator "\uE0B6"
      forward_separator_fg "#282828"
      forward_separator_bg RESET
      alignment "left"
    }

    segment "shared_status_segment" {
      content " [{STATUS}]"
      fg "{if = {STATUS} == 0 ? #b8bb26 : #fb4934}"
      bg "#3c3836"
      separator "\uE0B0"
      separator_fg "#3c3836"
      separator_bg "#3c3836"
      alignment "left"
    }

    segment "shared_duration_segment" {
      content "in {CMD_DURATION} "
      fg "#ebdbb2"
      bg "#3c3836"
      separator "\uE0B0"
      separator_fg "#3c3836"
      separator_bg RESET
      alignment "left"
    }

    segment "shared_language_badge_segment" {
      content "${project_language_badge}"
      fg "#83a598"
      bg "#504945"
      separator "\uE0B0"
      separator_fg "#504945"
      separator_bg "#665c54"
      alignment "left"
    }
  }
```

And can be used in a segment like this:

```bash
use_segment "shared_user_segment" as "userseg"
```
### Theme Segment behaviors

Segments use a tag system to that certain segments can be separated from the main prompt and can have their own behaviors defined only for a given tag. One prompt may have many different segments that make up its self. Segments are evaluated and rendered by hierarchical definition from top to bottom as defined in the prompt definition.

Here is an example of all available segment behaviors being used:

```bash
segment "user_segment" {
      content " {USERNAME} "
      fg "#d3869b"
      bg "#282828"
      separator "\uE0B0"
      separator_fg "#282828"
      separator_bg "#3c3836"
      forward_separator "\uE0B6"
      forward_separator_fg "#282828"
      forward_separator_bg RESET
      alignment "left"
      bold true
      italic false
      underline false
      dim false
      strikethrough false
    }
```

The separator is the separator to the right of the content and the forward separator is the one to the left. The alignment value allows the prompt to be anchored to a given side of the screen, center, left, or right. This placement will dynamically scale with terminal window size.

#### Text Style Properties

Segments support text styling properties;xw

- **`bold`** - Makes text bold/thicker (values: `true`, `false`, `yes`, `no`, `1`, `0`)
- **`italic`** - Renders text in italics (values: `true`, `false`, `yes`, `no`, `1`, `0`)
- **`underline`** - Underlines the text (values: `true`, `false`, `yes`, `no`, `1`, `0`)
- **`dim`** - Renders text with reduced intensity/dimmed (values: `true`, `false`, `yes`, `no`, `1`, `0`)
- **`strikethrough`** - Strikes through the text (values: `true`, `false`, `yes`, `no`, `1`, `0`)

All text style properties are optional and default to `false`. Multiple styles can be combined on the same segment. For example:

```bash
segment "emphasized_segment" {
      content " IMPORTANT "
      fg "#ff0000"
      bg "#000000"
      bold true
      underline true
      alignment "left"
    }

segment "subtle_segment" {
      content " (optional) "
      fg "#888888"
      bg "RESET"
      dim true
      italic true
      alignment "left"
    }
```

**Note:** Text style support depends on your terminal emulator. Most modern terminals support bold, italic, underline, and dim. Support for strikethrough varies by terminal.

### Theme Behaviors

Theme Behaviors have 3 potential toggles: cleanup, cleanup_empty_line, and newline_after_execution. Cleanup is only able to be used with a newline prompt and it removes the main prompt upon the user pressing enter on input and it will move the newline prompts and all text up to where the main prompt line started. cleanup_empty_line places a newline after the cleanup before execution. newline_after_execution is self-explanatory.

```bash
behavior {
    cleanup false
    cleanup_empty_line false
    newline_after_execution false
}
```

There is also the fill behavior. It defines what should fill the space in between main prompt alligned segements.

```bash
  fill {
    char " ",
    fg RESET
    bg RESET
  }
```

### Prompt Information Variables

The information variables are defined like {VAR_NAME} and will replace the var with the proper information. Some basic vars are: {USERNAME} {HOSTNAME} {PATH} {DIRECTORY} {TIME} Vars can be defined in any content field, and prompt type, conditional, and separator. You can see a comprehensive list at the bottom of this page. Some prompt tags should only be used while within a certain prompt, but you can kind of just put whatever information you want anywhere. Nothing is stopping you. Additionally custom EXEC prompt variables can be defined, see below.

### Default cjsh Theme

Here is what the default cjsh theme looks like and this theme is created upon creation of the .cjshrc file within cjsh:

```bash
#! usr/bin/env cjsh

theme_definition {
  terminal_title "{PATH}"

  fill {
    char "",
    fg RESET
    bg RESET
  }

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

  git_segments {
    segment "path" {
      content " {LOCAL_PATH} "
      fg "#55FF55"
      bg "RESET"
      separator " "
      separator_fg "#FFFFFF"
      separator_bg "RESET"
    }
    segment "branch" {
      content "{GIT_BRANCH}"
      fg "#FFFF55"
      bg "RESET"
    }
    segment "status" {
      content "{GIT_STATUS}"
      fg "#FF5555"
      bg "RESET"
      separator " $ "
      separator_fg "#FFFFFF"
      separator_bg "RESET"
    }
  }

  inline_right {
    segment "time" {
      content "[{TIME}]"
      fg "#888888"
      bg "RESET"
    }
  }

  behavior {
    cleanup false
    cleanup_empty_line false
    newline_after_execution false
  }
}
```

### Available Prompt Tags

```cpp
/*
 * Core identity:
 * {USERNAME}    - Current user's name
 * {HOSTNAME}    - System hostname
 * {PATH}        - Current working directory (with ~ for home)
 * {DIRECTORY}   - Name of the current directory
 *
 * Time and date:
 * {TIME}, {TIME24} - Current time (HH:MM:SS) in 24-hour format
 * {TIME12}     - Current time (HH:MM:SS) in 12-hour format
 * {DATE}       - Current date (YYYY-MM-DD)
 * {DAY}        - Current day of the month (1-31)
 * {MONTH}      - Current month (1-12)
 * {YEAR}       - Current year (YYYY)
 * {DAY_NAME}   - Name of the current day (e.g., Monday)
 * {MONTH_NAME} - Name of the current month (e.g., September)
 *
 * Command status:
 * {STATUS}       - Last command exit code as reported by $?
 * {EXIT_CODE}    - Last command exit code (empty when zero)
 * {EXIT_SYMBOL}  - Exit status symbol (✓ for success, ✗ for failure)
 * {CMD_SUCCESS}  - Whether the last command was successful (true/false)
 * {CMD_DURATION} - Duration of the last command (formatted if above threshold)
 * {CMD_DURATION_MS} - Duration of the last command in microseconds
 *
 * Directory helpers:
 * {DISPLAY_DIR}    - Enhanced directory display with repo/home contraction
 * {TRUNCATED_PATH} - Truncated path with symbol
 * {DIR_TRUNCATED}  - Whether directory display is truncated (true/false)
 * {REPO_PATH}      - Repository-relative path when inside a Git repo
 *
 * Git specifics (only inside Git repositories):
 * {LOCAL_PATH}  - Local path of the Git repository
 * {GIT_BRANCH}  - Current Git branch
 * {GIT_STATUS}  - Git status indicator (✓ for clean, * for dirty)
 * {GIT_AHEAD}   - Number of commits ahead of remote
 * {GIT_BEHIND}  - Number of commits behind remote
 * {GIT_STASHES} - Number of stashes in the repository
 * {GIT_STAGED}  - Indicator when staged changes exist (✓ or empty)
 * {GIT_CHANGES} - Number of uncommitted changes
 *
 * Language helpers:
 * {LANG_VER:<language>} - Active version for the given language
 *                         (e.g., python, node, go, rust, java, cpp, csharp,
 *                         php, ruby, kotlin, swift, dart, scala)
 *
 * Command execution tag:
 * {EXEC%%%<command>%%%<cache_duration>} - Execute a shell command with caching
 */
```

### Command Execution Tags

The shell supports executing arbitrary commands in your prompt with automatic caching:

 * {EXEC%%%<command>%%%<cache_duration>} - Execute a shell command with caching
   - `command`: The shell command to execute
   - `cache_duration`: Cache duration in seconds (optional, defaults to 30)
                       Use `-1` for permanent caching (execute once, cache forever)

Here are some examples:

```bash
# Show current time, cached for 60 seconds
{EXEC%%%date +%H:%M%%%60}

# Show git commit count, cached for 30 seconds (default)
{EXEC%%%git rev-list --count HEAD%%%}

# Show battery percentage on macOS, cached for 120 seconds
{EXEC%%%pmset -g batt | grep -Eo "\d+%" | cut -d% -f1%%%120}

# Show kubernetes context, cached for 10 seconds
{EXEC%%%kubectl config current-context%%%10}

# Show kernel version, cached permanently (execute only once)
{EXEC%%%uname -r%%%-1}

# Show hostname, cached permanently
{EXEC%%%hostname%%%-1}
```