# CJ's Shell

## Introduction

CJ's Shell (cjsh) is a lightweight shell with out of the box power and speed. Baked in are strong first party theme scripting with a custom theme DSL language, a full custom shell script interpreter with minor bash support, custom keybindings for text editing, custom syntax highlighting, fuzzy text auto completions, smart directory navigation, advanced history seaching, multiline editing, typeahead, and rich prompts. All with no external shell support and only 1 dependency which is already baked in. cjsh aims to be fast and responsive at all times. It is fully usable on all *nix like systems and Windows with WSL. cjsh aims to be an almost 1 to 1 switch over from other POSIX like shells.

## Why did I make cjsh?

Truly it was out of spite and annoyance, like all passion projects. I got so tired of all of these different prompt tools, frameworks, different shells entirely. Many of which were non POSIX compliant which basically made them useless for me. I found that no matter what combination of prompt tools, frameworks with different shells on top of having a bunch of rust rewrites of coreutils that it made the shell I was using so bloated, slow, and SO annoying to configure and setup on new machines or if I wanted to add a new tool all the while I felt like these tools could just be so much better. Which is exactly when I said "I bet I could do this so much better." So I did. I sought out to basically make bash + zoxide + eza + starship/ohmyposh/p10k + tab completions + auto completions + spell corrections + themes + custom keybindings + syntax highlighting + full multiline editing suite all while being lightweight, fast, and POSIX compliant. Oh and did I mention cjsh only uses one external library which is baked into cjsh itself. Yep, I did all of that. It has taken me a little over a year of on and off work, but I am super proud of where cjsh stands and I am continuing to make constant improvments and additions. It just works.

## Why POSIX?

I chose to make cjsh POSIX compliant so that it is actually usable in normal and basic terminal interactions and so that it will actually act and respond to how users expect it to. Instead of requiring users to have to learn a totally proprietary shell language, cjsh uses the basic sh scripting language plus some bash syntax for added functionality. I found that many shell scripts on Stack Overflow or Google for installations, build tools, and other various commands that users would need to run are all in sh. Using these commands in many other shells and they would simply not work or not work as intended and cause unforeseen issues. I did not want any of that. I wanted users to be able to switch over and not notice any difference when it came to interacting with scripting. I also found that I could get all of the same functionality and POSIX compliance while still having some very rich features all baked in on top that do not get in the way of POSIX compliance, such as custom cd or ls implementations.

## Why use cjsh?

cjsh was designed to just work. No need for any shell frameworks or custom implementations to download, or having to create your own completions etc. It just works. Themes can be written in a ruby/json like language and it can be written straight into script files and cjsh will know how to handle it. Themes are data oriented, hierarchical, and strongly typed. Almost any interactive feature in cjsh can be customized: keybinds, prompt, theme, syntax highlighting, completions, etc.

