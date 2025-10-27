# CJ's Shell

## Introduction

CJ's Shell (cjsh) is a POSIX-based interactive shell that pairs familiar script compatibility with integrated modern features. Built in are first-party theme scripting with a custom DSL, a POSIX shell interpreter with bash extensions, customizable keybindings, syntax highlighting, fuzzy completions, smart directory navigation, advanced history search, multiline editing, typeahead, and rich prompts. Everything ships in one binary with a single vendored dependency, so cjsh works out of the box on all *nix-like systems and Windows via WSL. cjsh delivers a POSIX+ experience, standard scripting semantics with an enhanced interactive layer you can dial up or down as needed.

The core scripting engine targets roughly 95% POSIX coverage, validated by 1500+ standards-focused tests, while the interactive layer intentionally stretches beyond POSIX through configurable POSIX+ features.

## Why did I make cjsh?

Truly it was out of spite and annoyance, like all passion projects. I got so tired of all of these different prompt tools, frameworks, different shells entirely. Many of which were non POSIX compliant which basically made them useless for me. I found that no matter what combination of prompt tools, frameworks with different shells on top of having a bunch of rust rewrites of coreutils that it made the shell I was using so bloated, slow, and SO annoying to configure and setup on new machines or if I wanted to add a new tool all the while I felt like these tools could just be so much better. Which is exactly when I said "I bet I could do this so much better." So I did. I set out to basically make bash + zoxide + eza + starship/ohmyposh/p10k + tab completions + auto completions + spell corrections + themes + custom keybindings + syntax highlighting + full multiline editing suite while keeping the scripting surface familiar. Oh and did I mention cjsh only uses one external library which is baked into cjsh itself. Yep, I did all of that. It has taken me a little over a year of on and off work, but I am super proud of where cjsh stands and I am continuing to make constant improvements and additions. It just works.

## Why POSIX?

I chose to make cjsh POSIX-based so that it is actually usable in normal and basic terminal interactions and so that it will act and respond the way users expect. Instead of requiring people to learn a proprietary shell language, cjsh uses the basic sh scripting language plus some bash syntax for added functionality. I found that many shell scripts on Stack Overflow or Google for installations, build tools, and other various commands that users would need to run are all in sh. Using these commands in many other shells and they would simply not work or not work as intended and cause unforeseen issues. I did not want any of that. I wanted users to be able to switch over and not notice any difference when it came to interacting with scripting. I also found that I could keep the script engine standards-aligned while layering rich, optional interactive features on top, such as custom `cd` or `ls` implementations.

## Why use cjsh?

cjsh was designed to just work. No need for any shell frameworks or custom implementations to download, or having to create your own completions etc. It just works. Themes can be written in a ruby/json like language and it can be written straight into script files and cjsh will know how to handle it. Themes are data oriented, hierarchical, and strongly typed. Almost any interactive feature in cjsh can be customized: keybinds, prompt, theme, syntax highlighting, completions, etc.

