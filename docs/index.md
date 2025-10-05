# CJ's Shell

![Version](https://img.shields.io/github/v/release/CadenFinley/CJsShell?label=version&color=blue)

## Introduction

CJ's Shell (cjsh) is a lightweight shell with out of the box power and speed. Baked in are strong first party theme scripting with a custom theme DSL language, a full custom shell script interpreter with minor bash support, custom keybindings for text editing, custom syntax highlighting, fuzzy text auto completions, and smart directory navigation. cjsh aims to be fast and responsive at all times. It is fully usable on all *nix like systems and Windows with WSL. cjsh aims to be an almost 1 to 1 switch over from other POSIX like shells.

## Why POSIX?
I chose to make cjsh POSIX compliant so that it is actually usable in normal and basic terminal interactions and so that it will actually act and respond to how users expect it to. Instead of requiring users to have to learn a totally proprietary shell language, cjsh uses the basic sh scripting language plus some bash syntax for added functionality. I found that many shell scripts on Stack Overflow or Google for installations, build tools, and other various commands that users would need to run are all in sh. Using these commands in many other shells and they would simply not work or not work as intended and cause unforeseen issues. I did not want any of that. I wanted users to be able to switch over and not notice any difference when it came to interacting with scripting. I also found that I could get all of the same functionality and POSIX compliance while still having some very rich features all baked in on top that do not get in the way of POSIX compliance, such as custom cd or ls implementations.

## Why use cjsh?
cjsh was designed to just work. No need for any shell frameworks or custom implementations to download, or having to create your own completions etc. It just works. Themes can be written in a ruby/json like language and it can be written straight into script files and cjsh will know how to handle it. Themes are data oriented, hierarchical, and strongly typed. Almost any interactive feature in cjsh can be customized: keybinds, prompt, theme, syntax highlighting, completions, etc.

