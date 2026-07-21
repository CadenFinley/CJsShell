# cjsh Changelog

This changelog documents tagged releases from `v1.1.2` through `v1.3.3`.

## 1.3.3 - 2026-07-08

Range: `v1.3.2..HEAD` (19 commits, 52 files changed)

### Highlights

- Finalized 1.3.3 as a stable release by clearing the default pre-release build marker.
- Added transient submitted-prompt styling (`PS1_FINAL` / `RPS1_FINAL`) and simplified prompt layout controls by removing legacy prompt-cleanup toggles.
- Improved interactive editing and menu UX with click-to-accept completion controls, multiline menu previews, and richer history metadata display.

### Added

- Added `cjshopt completion-click-accept` (`on|off|status`) plus completion/help metadata and isocline plumbing for click-accept behavior.
- Added `cjshopt menu-highlighting` (`none|single|all|reverse|status`) to syntax-highlight completion and history menu items with the existing buffer highlighter.
- Added transient prompt variables `PS1_FINAL` and `RPS1_FINAL` to restyle submitted prompt lines after Enter.
- Added the `file-argument` syntax-highlighting style and broader path-argument highlighting for existing file arguments.
- Added split-unknown command merge completions to recover from accidentally separated command tokens.
- Added richer history-search metadata rendering with relative timestamps, exit-code suffixes, and metadata-key previews.

### Changed

- Changed `cjshopt mouse-clicking` from boolean toggles to `disabled|simple|smart|status` modes with explicit smart suspend/resume behavior.
- Changed completion and history menus to handle multiline candidates and entries with inline previews while preserving scroll/selection behavior.
- Changed history deduplication to prefer entries that carry exit-code metadata when duplicate commands are collapsed.
- Changed docs/reference coverage across commands, editing, features, and themes for the new completion click mode, prompt-final variables, and prompt-layout model.
- Simplified `README.md` by removing duplicated preset/testing/doc-preview sections now covered in dedicated docs.

### Fixed

- Fixed history staging so interactive commands are recorded using expanded command text, with new regression coverage for `!!` replay behavior.
- Fixed completion-application edge cases where accepted completions could duplicate already-present single-line or multiline suffix text.
- Fixed unknown-command suggestion and highlighting paths to better handle accidentally split command tokens.
- Fixed status-line unknown-command suggestions by merging split-token candidates with standard suggestion output.
- Fixed multiline mouse-targeting and menu interaction edge cases in isocline PTY flows.

### Internal, CI, and Tests

- Expanded completion/highlighter/isocline regression coverage for click-accept toggles, multiline menus, metadata filtering, and mouse behavior.
- Continued cleanup/refactor passes across filesystem setup, parser helpers, loop handling, exec flow, and completion generation internals.

## 1.3.2 - 2026-07-07

Range: `v1.3.1..HEAD` (15 commits, 56 files changed)

### Highlights

- Added optional Enter-triggered spell autocorrection for single spell matches, with new `cjshopt` controls and docs.
- Improved parser/interpreter handling for subshell-style function definitions so `name() ( ... )` forms define cleanly, execute only on call, and preserve subshell scope.
- Fixed command-execution edge cases around stdout capture/job control, including command-substitution contamination and function pipelines under `stty tostop`.

### Added

- Added `cjshopt completion-spell-enter` (`on|off|status`) plus builtin completion/help metadata for the new toggle.
- Added isocline spell-on-enter plumbing (`ic_enable_spell_correct_on_enter`) and PTY coverage for Enter-time spell correction behavior.
- Added a dedicated function-pipeline job-control regression suite (`tests/core/test_function_pipeline_job_control.py`) and wired it into `tests/run_shell_tests.sh`.

### Changed

- Changed completion context scoping to track the innermost unclosed command substitution when completing inside `$(...)`.
- Refactored arithmetic-command parsing helpers into shared parser utilities and reused them across interpreter, conditional, and loop paths.
- Updated `exec`, `read`, and `generate-completions` internals for stricter option/error handling and cleaner execution flow.
- Synced local isocline behavior with newer upstream-oriented updates across completion, help, string-buffer, and TTY handling paths.

### Fixed

- Fixed Enter-submit behavior so spell correction stays opt-in while still allowing single-match auto-correction when explicitly enabled.
- Fixed command-substitution regressions where prior builtin stdout could leak into `$()`/backtick results and break follow-on redirections.
- Fixed negated command output handling so `!` forms respect quiet redirections without runtime noise.
- Fixed subshell-style function parsing/execution edge cases, including inline semicolon forms and variable-scope preservation.
- Fixed interactive job-control handling for function pipelines so foreground control and `tostop` scenarios do not spuriously stop jobs.

### Internal, CI, and Tests

- Expanded shell/isocline/completion regression coverage for spell-enter toggles, kitty control-sequence decoding, mouse-status rendering, command-substitution safety, and function syntax cases.
- Removed the deprecated release workflow file and continued cleanup passes across interpreter, exec, and builtin implementations.

## 1.3.1 - 2026-07-04

Range: `v1.3.0..HEAD` (13 commits, 45 files changed)

### Highlights

- Added a new interactive command palette with custom keybinding hooks and palette-only command entries for faster command execution.
- Introduced status-line controls (`status-reporting` and `status-line-callback`) so users can keep hints while customizing or muting validation output.
- Expanded mouse-driven editing controls with new runtime toggles and stronger click/scroll handling in completion and history menus.

### Added

- Added a command palette action (`command-palette`) with default `Alt+P` binding and searchable built-in actions.
- Added `cjshopt keybind ext` support for custom command bindings and palette-only snippets with optional titles.
- Added `cjshopt status-line-callback` for per-refresh shell-function status messages via `CJSH_STATUS_INPUT` / `CJSH_STATUS_OUTPUT`.
- Added `cjshopt completion-menu-expanded` to set expanded completion menus as the default behavior.
- Added `cjshopt mouse-clicking` and `cjshopt mouse-clicking-status-line` to control prompt-level mouse defaults and indicator visibility.

### Changed

- Changed status-line composition to combine callback output, spell hints, and validation summaries with safer refresh/state handling.
- Updated completion/history/help UI copy and menu headers to surface mouse-active context and improve interaction hints.
- Refined keybinding and palette plumbing across core/isocline integration for startup-driven customization.
- Updated documentation for new 1.3.1 `cjshopt` toggles, callback behavior, and command-driven keybinding flows.

### Fixed

- Fixed mouse click cursor placement when hints/completions add extra rendered lines in the editor view.
- Fixed mouse menu interaction edge cases around completion rendering/scrolling paths.
- Fixed command-palette entry import bounds/validation in isocline options handling.

### Internal, CI, and Tests

- Added new completion and PTY/isocline coverage for status-line controls, mouse toggles, command palette paths, and expanded completion defaults.
- Added automated release workflow support (`.github/workflows/release.yml`).
- Landed cleanup passes in filesystem internals and broad formatting normalization across touched modules.

## 1.3.0 - 2026-07-02

Range: `v1.2.0..HEAD` (64 commits, 301 files changed)

### Highlights

- Added major interactive/editor upgrades: richer keybinding behavior, stronger cursor navigation, and mouse-driven menu interaction.
- Introduced new shell builtins for variable declaration, runtime restart, and fast path navigation into cjsh-managed files/directories.
- Continued architecture cleanup with module layout refactors, execution-path reorganization, and parser/interpreter optimization passes.

### Added

- Added `declare` / `typeset` builtin support with attribute flags (including indexed array declarations and function-aware modes).
- Added `approot` builtin for jumping to or printing resolved cjsh paths (`config`, `cache`, `history`, `cjshenv`, profile/rc/logout files, and executable location).
- Added `restart` builtin for in-place shell re-exec with optional startup-flag reset via `--no-flags`.
- Added expanded keybinding behavior in isocline (including additional Alt bindings and stronger `Ctrl+A`/`Ctrl+E` handling).
- Added mouse-aware line-editor/menu interactions with runtime-togglable behavior and menu selection support.

### Changed

- Renamed/restructured source layout toward `cjsh-core` and updated build wiring around the new module naming.
- Refactored execution internals into a more explicit exec subsystem and tightened call paths across main loop, parser, and interpreter layers.
- Updated version/pre-release metadata flow in CMake/build metadata generation.
- Improved bash-compat behavior across core builtins and refreshed long-option/help output handling.
- Improved `ulimit` behavior and completion metadata paths.
- Reworked history frequency ranking implementation and follow-up tuning.

### Fixed

- Fixed hint-buffer clearing when returning to an empty interactive buffer.
- Fixed history-expansion regressions and associated parser integration edge cases (including issue #28 follow-up).
- Fixed completion/menu rendering issues, including scrolling behavior and small-menu mouse-toggle edge cases.
- Fixed POSIX control-flow/case handling regressions and additional parser validation corner cases.
- Fixed Linux linking issues around history-expansion tests and additional CI portability failures.
- Fixed build-setting regression tracked in issue #29.

### Internal, CI, and Tests

- Added broader line-reflow, keybinding, and PTY integration coverage for isocline behavior.
- Added stronger startup and regression coverage across shell/interactive paths.
- Improved shell test reporting and refreshed deprecated workflow usage in CI.
- Landed a broad optimization/cleanup pass touching parser, expansions, interpreter loops, and startup-related hot paths.

## 1.2.0 - 2026-04-04

Range: `v1.1.6..v1.2.0` (167 commits, 435 files changed)

### Highlights

- Major release with repository/build-system overhaul, new shell-language features, and broad UX/testing expansion.
- Merged build-system overhaul via PR #27 and completed a large project reorganization.

### Added

- Added `CJSH_HISTORY_FILE` support for explicit history-file routing.
- Added `--no-history` startup mode.
- Added command-not-found handling path.
- Added indexed array support.
- Added C-style arithmetic loop support.
- Added support to clear custom job names.

### Changed

- Reorganized source layout into dedicated components (including split `cjsh` and `cjsh-isocline` trees) and updated CMake/build plumbing.
- Stopped sourcing `.profile` by default and introduced updated environment-startup handling (`cjshenv` flow).
- Consolidated builtin/job-control internals and reduced global-context usage through common utility extraction.
- Centralized `setenv` operations through the variable manager.
- Improved auto-`cd` handling for `-` with matching syntax-highlighting/status-line behavior.

### Fixed

- Improved loop/signal handling (including stronger SIGSTP coverage).
- Improved job-selection defaults when only one background/stopped job exists.
- Improved error consistency by routing more `perror`/error paths through `error_out`.
- Improved completion generation and duplicate-suppression behavior.
- Improved key-sequence and visible-character handling in typeahead/editline paths.

### UX, Build, and Tests

- Added mouse-wheel support in history search and expanded completion menus.
- Improved prompt/menu collapse handling and terminal-health checks.
- Expanded build-system, PTY/isocline, array, arithmetic, hash, quoting, and regression test suites.
- Added dedicated build-system tests and updated CI wiring around the new layout.

## 1.1.6 - 2026-02-14

Range: `v1.1.5..v1.1.6` (81 commits, 159 files changed)

### Highlights

- Feature release centered on execution-mode flags, autobg behavior, completion expansion, and stronger prompt/typeahead reliability.

### Added

- Added script-dispatch support for cjsh scripting workflows.
- Added `--no-exec` handling (including pipeline behavior).
- Added automatic backgrounding support (`autobg`) with tests/docs (merged via PR #26).
- Added flags for disabling error suggestions and controlling prompt behavior in minimal modes.
- Added more completion coverage for variables and builtin-specific suggestions (`type`/`which`).

### Changed

- Updated Linux build behavior to support dynamic build flows.
- Adjusted default prompt/right-prompt behavior for safer minimal and non-set scenarios.
- Updated login/startup argument parsing to include additional flag handling.
- Continued POSIX-mode groundwork (skeleton + blocker tracking updates).

### Fixed

- Fixed jobs mutex/race issues and improved invalid-argument handling for job commands.
- Fixed abbreviation expansion from command-front positions.
- Fixed bracket/case validation edge cases and several parser/highlighter regressions.
- Fixed TTY/typeahead synchronization issues, including line-editing/newline edge behavior.
- Fixed export error handling and improved source-command error output.

### Internal, CI, and Tests

- Expanded syntax-highlighting and completion test coverage significantly.
- Stopped building tests by default in local builds while ensuring tests run in CI.
- Improved error-header consistency and removed duplicated version/header fragments.

## 1.1.5 - 2026-02-06

Range: `v1.1.4..v1.1.5` (87 commits, 289 files changed)

### Highlights

- Large refactor-heavy release that introduced async prompt handling, stronger error infrastructure, and broader POSIX/job-control hardening.
- Significant churn count is mostly driven by large-scale reorganization and generated formatting/refactor movement, not only net new features.

### Added

- Added continuation callbacks through isocline integration.
- Added case-sensitivity toggles for history search.
- Added `set globstar`, `readonly -f`, `read -t`, and `set -o` feature work.
- Added explicit fatal-error path handling and structured error logging.

### Changed

- Moved status-line handling into a dedicated module and surfaced unknown-command errors there.
- Switched completion caching to an LRU-based strategy and improved suggestion filtering quality.
- Prioritized executable files for `./` completion scenarios.
- Introduced full async prompt behavior with non-blocking TTY reads and session gating for prompt refresh.
- Centralized command validation for both syntax-highlighting and status-line evaluation.
- Refactored expansion/conditional evaluation paths and consolidated parameter/variable management.

### Fixed

- Fixed duplicate status-line redraw artifacts during terminal resize/reflow.
- Fixed execution-error routing so failures consistently pass through `error_out`.
- Fixed job-control exit/cleanup races (including immediate job-table updates after signals).
- Fixed background builtin behavior and force-exit semantics around logout/exit traps.
- Fixed Linux/macOS portability regressions introduced during strict-compile cleanup.

### Internal, CI, and Tests

- Removed direct `getenv` usage in favor of variable-manager pathways.
- Removed timeout-based test workarounds and tightened CI behavior.
- Expanded shell/isocline coverage across redirection, signal, readonly, and process-control paths.
- Per-file license/header cleanup and documentation refresh shipped alongside refactor work.

## 1.1.4 - 2026-01-30

Range: `v1.1.3..v1.1.4` (26 commits, 51 files changed)

### Highlights

- Small, focused UX/configuration release with prompt/status-line control, hash behavior improvements, and cleanup of deprecated flags.

### Added

- Added fuller status-line control surface through `cjshopt`-driven configuration updates.
- Reintroduced terminal window-title handling.

### Changed

- Removed deprecated `--no-smart-cd` support.
- Changed `hash -r` with no arguments to perform a full reset.
- Removed prompt-item configuration from startup flags and shifted configuration responsibility.
- Updated help/startup messaging for clearer boot-time guidance.
- Updated syntax-highlighting color choices and prompt-created-line context defaults.

### Fixed

- Prevented unnecessary hash rebuilds on hash command invocation.
- Removed arbitrary completion sorting in isocline to preserve stronger ordering guarantees.

### Internal and Docs

- Continued main-namespace cleanup and directory/name organization updates.
- Removed noisy shutdown output and cleaned older help text variants.

## 1.1.3 - 2026-01-28

Range: `v1.1.2..v1.1.3` (37 commits, 66 files changed)

### Highlights

- Focused release on startup-flow reorganization, validator restructuring, hash command behavior, and UI correctness.

### Added

- Added job resolution by `+` and `-` selectors in job-control workflows.
- Added safer `cwd`/`pwd` handling paths.
- Added clearer completion source tagging.

### Changed

- Reorganized first-boot/startup sequence and timing paths.
- Reimplemented command hashing and tied completion lookups more directly to path hash behavior.
- Refactored validator modules with broad multiline-validation updates.
- Disabled completion learning behavior to keep completion output more deterministic.

### Fixed

- Fixed right-aligned prompt rendering, including follow-cursor behavior.
- Fixed status-line cleanup before returning terminal control to child processes/callers.
- Fixed completion-menu collapse behavior on control-key interaction (`Ctrl+J`).

### Internal and Tests

- Expanded regression coverage for control-flow and filesystem edge cases.
- Included test harness cleanup after a revert/reapply cycle during this release window.

## 1.1.2 - 2026-01-27

Range: `v1.1.1..v1.1.2` (125 commits, 185 files changed)

### Highlights

- Stabilized interactive shell behavior with a large batch of job-control, completion, prompt, and validation fixes.
- Landed a broad cleanup cycle: legacy code removal, file reorganization, clang-tidy cleanup, and full clang-format pass.

### Added

- Added full secondary prompt (`PS2`) support, including a setting to disable PS2 line-number overrides.
- Added completions for all `cjshopt` options and extra job metadata in completion output.
- Added job-control quality-of-life features: `fg`, `bg`, and `kill` by command name, plus `kill` by job name.
- Added PID reporting when jobs are started in the background.
- Added invoked-as-`sh` warning with explicit suppression support.
- Added script line-continuation support.

### Changed

- Reworked completion behavior: configurable completion count, adjusted ordering/highlighting, and removed arbitrary completion/history limits.
- Split job-control builtins into dedicated files and moved shell hooks to a separate module.
- Improved help/status-line presentation with optional underline hints and cleaner keybinding hint display.
- Reworked startup/main-loop organization to reduce duplication and global-state coupling.

### Fixed

- Fixed completion memory leaks and crash cases (including long file completion entries).
- Fixed terminal-resize handling and prompt/menu interaction bugs.
- Fixed control-structure and loop redirection validation edge cases.
- Fixed fake error emission in inlined subshell loop callbacks.
- Fixed background signal flow after `waitpid`, plus safer handling for quickly exiting jobs.
- Fixed alias behavior through pipelines and several non-interactive invocation edge cases.

### Internal and Tests

- Added/updated behavior tests for isocline integration, `read`, control structures, and validation edge cases.
- Vendored isocline-related code was normalized with explicit license coverage and follow-up cleanups.
