#!/usr/bin/env python3

import subprocess
import time
import statistics
import sys
import os
from typing import List, Tuple, Dict

# Configuration
RUNS = 15

# Shell-specific command mappings
# Each command is mapped to shell-specific syntax
SHELL_COMMANDS = {
    "posix": {  # For bash, zsh, cjsh, and other POSIX-compatible shells
        "ls": "-c ls",
        "version": "--version",
        "hello": "-c 'echo hello world'",
        "pwd": "-c pwd",
        "date": "-c 'echo $(date)'",
        "shell_var": "-c 'echo $SHELL'",
        "ls_long": "-c 'ls -la'",
        "exit": "-c exit",
        "loop": "-c 'for i in {1..5000}; do echo $i; done'",
        "loop_even": "-c 'for i in {1..5000}; do if [ $((i % 2)) -eq 0 ]; then echo $i; fi; done'"
    },
    "fish": {
        "ls": "-c ls",
        "version": "--version",
        "hello": "-c 'echo hello world'",
        "pwd": "-c pwd",
        "date": "-c 'echo (date)'",
        "shell_var": "-c 'echo $SHELL'",
        "ls_long": "-c 'ls -la'",
        "exit": "-c exit",
        "loop": "-c 'for i in (seq 5000); echo $i; end'",
        "loop_even": "-c 'for i in (seq 5000); if test (math \"$i % 2\") -eq 0; echo $i; end; end'"
    },
    "nu": {
        "ls": "-c 'ls'",
        "version": "--version",
        "hello": "-c 'echo hello world'",
        "pwd": "-c 'pwd'",
        "date": "-c 'date now | get datetime'",
        "shell_var": "-c 'echo $env.SHELL?'",
        "ls_long": "-c 'ls -l'",
        "exit": "-c 'exit'",
        "loop": "-c '1..5000 | each { |i| echo $i }'",
        "loop_even": "-c '1..5000 | where { |i| $i mod 2 == 0 } | each { |i| echo $i }'"
    },
    "elvish": {
        "ls": "-c 'ls'",
        "version": "--version",
        "hello": "-c 'echo hello world'",
        "pwd": "-c 'pwd'",
        "date": "-c 'echo (date)'",
        "shell_var": "-c 'echo $E:SHELL'",
        "ls_long": "-c 'ls -la'",
        "exit": "-c 'exit'",
        "loop": "-c 'for i [(range 1 5001)] { echo $i }'",
        "loop_even": "-c 'for i [(range 1 5001)] { if (== (% $i 2) 0) { echo $i } }'"
    },
    "ion": {
        "ls": "-c 'ls'",
        "version": "--version",
        "hello": "-c 'echo hello world'",
        "pwd": "-c 'pwd'",
        "date": "-c 'echo $(date)'",
        "shell_var": "-c 'echo $SHELL'",
        "ls_long": "-c 'ls -la'",
        "exit": "-c 'exit'",
        "loop": "-c 'for i in 1..5000; echo $i; end'",
        "loop_even": "-c 'for i in 1..5000; if test $((i % 2)) -eq 0; echo $i; end; end'"
    },
    "xonsh": {
        "ls": "-c 'ls'",
        "version": "--version",
        "hello": "-c 'echo \"hello world\"'",
        "pwd": "-c 'pwd'",
        "date": "-c 'echo hello'",
        "shell_var": "-c 'echo $SHELL'",
        "ls_long": "-c 'ls -la'",
        "exit": "-c 'exit'",
        "loop": "-c 'for i in range(1, 5001): print(i)'",
        "loop_even": "-c 'for i in range(1, 5001):\n    if i % 2 == 0:\n        print(i)'"
    }
}

# Command descriptions for output
COMMAND_DESCRIPTIONS = [
    "ls", "version", "hello", "pwd", "date", "shell_var", "ls_long", "exit", "loop", "loop_even"
]

BASELINE_SHELLS = ["fish", "bash", "zsh", "nu", "elvish", "ion", "xonsh"]
#CJSH_BINARY_TYPES = ["", "_speed03", "_speed02", "_debug"]  # Empty string means no suffix
CJSH_BINARY_TYPES = [""]

ENABLE_BASELINE_TESTS = True

# Global storage for results
all_results: List[List[Tuple[str, float, float, float]]] = []
all_commands: List[str] = []


def run_command_with_timing(shell_cmd: str, command: str) -> float:
    """Run a command and return the elapsed time in milliseconds."""
    full_command = f"{shell_cmd} {command}"
    
    start_time = time.perf_counter()
    try:
        # Use shell=True to properly handle command parsing
        subprocess.run(full_command, shell=True, capture_output=True, check=False)
    except Exception:
        # If command fails, still return the time it took
        pass
    end_time = time.perf_counter()
    
    return (end_time - start_time) * 1000  # Convert to milliseconds


def get_shell_command(shell: str, command_key: str) -> str:
    """Get the appropriate command syntax for a given shell."""
    if shell in ["bash", "zsh"] or shell.startswith("./cjsh"):
        return SHELL_COMMANDS["posix"][command_key]
    elif shell == "fish":
        return SHELL_COMMANDS["fish"][command_key]
    elif shell == "nu":
        return SHELL_COMMANDS["nu"][command_key]
    elif shell == "elvish":
        return SHELL_COMMANDS["elvish"][command_key]
    elif shell == "ion":
        return SHELL_COMMANDS["ion"][command_key]
    elif shell == "xonsh":
        return SHELL_COMMANDS["xonsh"][command_key]
    else:
        # Default to POSIX for unknown shells
        return SHELL_COMMANDS["posix"][command_key]


def test_command(command_key: str) -> None:
    """Test a single command across all shells."""
    results: List[Tuple[str, float, float, float]] = []
    
    print("----------------------------------------------------------------------")
    print(f"Testing command: {command_key}")
    print("----------------------------------------------------------------------")
    
    # Test cjsh binary types
    for binary_type in CJSH_BINARY_TYPES:
        shell_name = f"./cjsh{binary_type}"
        shell_path = f"./build/cjsh{binary_type}"
        command = get_shell_command(shell_name, command_key)
        
        print()
        print(f"Timing {shell_name} {command}")
        print()
        
        times: List[float] = []
        
        for i in range(RUNS):
            elapsed_time = run_command_with_timing(shell_path, command)
            times.append(elapsed_time)
        
        average_time = statistics.mean(times)
        min_time = min(times)
        max_time = max(times)
        
        results.append((shell_name, average_time, min_time, max_time))
    
    # Test baseline shells if enabled
    if ENABLE_BASELINE_TESTS:
        for shell in BASELINE_SHELLS:
            command = get_shell_command(shell, command_key)
            
            print()
            print(f"Timing {shell} {command}")
            print()
            
            times: List[float] = []
            
            for i in range(RUNS):
                elapsed_time = run_command_with_timing(shell, command)
                times.append(elapsed_time)
            
            average_time = statistics.mean(times)
            min_time = min(times)
            max_time = max(times)
            
            results.append((shell, average_time, min_time, max_time))
    
    print("----------------------------------------------------------------------")
    
    # Sort results by average time
    results.sort(key=lambda x: x[1])
    
    # Store results for summary
    all_commands.append(command_key)
    all_results.append(results)
    
    print(f"Completed testing: {command_key}")
    print()


def get_cjsh_version() -> str:
    """Get the version of cjsh."""
    try:
        result = subprocess.run("./build/cjsh --version", shell=True, 
                              capture_output=True, text=True, check=False)
        return result.stdout.strip() if result.stdout else "Version unavailable"
    except Exception:
        return "Version unavailable"


def print_summary() -> None:
    """Print the final results summary."""
    print("======================================================================")
    print("                           FINAL RESULTS SUMMARY")
    print("======================================================================")
    print(f"Total runs per command: {RUNS}")
    print(get_cjsh_version())
    print("======================================================================")
    
    for i, command_key in enumerate(all_commands):
        print()
        print(f"Command: {command_key}")
        print("----------------------------------------------------------------------")
        
        for shell, average, min_time, max_time in all_results[i]:
            actual_command = get_shell_command(shell, command_key)
            print(f"Average time for {shell} ({actual_command}): {average:.3f} ms")
            print(f"  Min time: {min_time:.3f} ms")
            print(f"  Max time: {max_time:.3f} ms")
        
        print("----------------------------------------------------------------------")
    
    print("======================================================================")


def check_binaries_exist() -> bool:
    """Check if all required binaries exist and are executable."""
    missing_binaries = []
    
    # Check cjsh binaries
    for binary_type in CJSH_BINARY_TYPES:
        binary_path = f"./build/cjsh{binary_type}"
        if not os.path.isfile(binary_path):
            missing_binaries.append(binary_path)
        elif not os.access(binary_path, os.X_OK):
            print(f"Warning: {binary_path} exists but is not executable")
    
    # Check baseline shells if enabled
    if ENABLE_BASELINE_TESTS:
        for shell in BASELINE_SHELLS:
            # Use 'which' command to check if shell exists in PATH
            try:
                result = subprocess.run(f"which {shell}", shell=True, 
                                      capture_output=True, check=False)
                if result.returncode != 0:
                    missing_binaries.append(shell)
            except Exception:
                missing_binaries.append(shell)
    
    if missing_binaries:
        print("Error: The following required binaries are missing or not accessible:")
        for binary in missing_binaries:
            if binary.startswith("./build/"):
                print(f"  {binary} (build the project first)")
            else:
                print(f"  {binary} (install or check PATH)")
        print()
        print("Please ensure all required binaries are available before running tests.")
        return False
    
    return True


def main() -> None:
    """Main execution function."""
    # Change to the script's directory to ensure relative paths work
    script_dir = os.path.dirname(os.path.abspath(__file__))
    parent_dir = os.path.dirname(script_dir)
    os.chdir(parent_dir)
    
    # Check if all required binaries exist before starting tests
    if not check_binaries_exist():
        sys.exit(1)
    
    print("All required binaries found. Starting performance tests...")
    print()
    
    # Test all commands
    for command_key in COMMAND_DESCRIPTIONS:
        test_command(command_key)
    
    # Print final summary
    print_summary()


if __name__ == "__main__":
    main()
