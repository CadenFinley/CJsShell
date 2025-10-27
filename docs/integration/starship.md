# Starship Prompt Integration

## Prerequisites

Make sure the Starship binary is installed and on your `$PATH`:

```bash
brew install starship
```

Any installation method supported by Starship (cargo, curl script, distro package, etc.) will work-the theme only shells out to the executable.

## Install the bundled theme

Copy the example theme somewhere cjsh can load it on startup. The snippet below keeps everything under `~/.config/cjsh`, but any readable path works.

```bash
mkdir -p ~/.config/cjsh/themes
cp /path/to/CJsShell/themes/starship_example.cjsh ~/.config/cjsh/themes/starship.cjsh
```

Feel free to rename the destination file; just keep the suffix `.cjsh`.