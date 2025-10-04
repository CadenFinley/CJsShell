# CJ's Shell

![Version](https://img.shields.io/github/v/release/CadenFinley/CJsShell?label=version&color=blue)

## Introduction

CJ's Shell (cjsh) is a lightweight shell with out of the box power and speed. Baked in are strong first party theme scripting with a custom theme DSL language, compiled dynamic library plugin support, a full custom shell script interpreter with minor bash support, custom keybindings for text editing, custom syntax highlight, fuzzy text auto completions, and a custom AI agent that stays out of your way. cjsh aims to be fast and responsive at all times. It is fully usable on all *nix like systems and Windows with WSL. cjsh aims to be an almost 1 to 1 switch over from other posix like shells.

## Why POSIX?
I chose to make cjsh POSIX so that it is actually usable in normal and basic terminal interactions and so that it will actually act and respond to how users expect it to. Instead of requiring user to have to learn a totally proprietary shell language, cjsh uses basic the sh scripting language plus some bash syntax for added functionality. I found that many shell scripts on stack overflow or google for installations, build tools, and other various commands that user would need to run are all in sh. Using these commands in many other shells and they would simply not work or not work as intended and cause unforseen issues. I did not want any of that. I wanted user to be able to switch over and not notice any difference when it came to interacting with scripting. I also found that I could get all of the same functionality and POSIX compliance while still having some very rich features all baked in on top that do not get in the way of POSIX compliance. Namely ai features or custom cd or ls implementations.

## Why use cjsh?
cjsh was designed to just work. No need for any shell frameworks or custom implementations to download, or having to create your own completions etc. It just works. Plugins can be written in any language you like just as long as it has a way to link with the pluginapi.h in any way shape or form. Themes can be written in a ruby/json like language and it can be written straight into script files and cjsh will know how to handle it. Themes are data oriented, heirarchical, and strongly typed. Almost any interactive feature in cjsh can we customized: keybinds, prompt, theme, syntax highlighting, completions, etc.

