#!/usr/bin/env python3

import subprocess
import time
import statistics
import sys
import os
from typing import List, Tuple, Dict

RUNS = 10

SHELL_COMMANDS = {
    "posix": {
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
    "csh": {
        "ls": "-c 'ls'",
        "version": "-c 'echo $version'",
        "hello": "-c 'echo hello world'",
        "pwd": "-c 'pwd'",
        "date": "-c 'date'",
        "shell_var": "-c 'echo $SHELL'",
        "ls_long": "-c 'ls -la'",
        "exit": "-c 'exit'",
        "loop": "-c 'set i = 1; while ( $i <= 5000 ) echo $i; @ i = $i + 1; end'",
        "loop_even": "-c 'set i = 1; while ( $i <= 5000 ) if ( ( $i % 2 ) == 0 ) echo $i; endif; @ i = $i + 1; end'"
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

COMMAND_DESCRIPTIONS = [
    "ls", "version", "hello", "pwd", "date", "shell_var", "ls_long", "exit", "loop", "loop_even"
]
BASELINE_SHELLS = ["cjsh", "fish", "bash", "zsh", "nu", "elvish", "ion", "xonsh"]
CJSH_BINARY_TYPES = [""]

ENABLE_BASELINE_TESTS = True

all_results: List[List[Tuple[str, float, float, float]]] = []
all_commands: List[str] = []


def run_command_with_timing(shell_cmd: str, command: str) -> float:
    full_command = f"{shell_cmd} {command}"
    
    start_time = time.perf_counter()
    try:
        subprocess.run(full_command, shell=True, capture_output=True, check=False)
    except Exception:
        pass
    end_time = time.perf_counter()
    
    return (end_time - start_time) * 1000


def get_shell_command(shell: str, command_key: str) -> str:
    if shell in ["bash", "zsh", "ksh"] or shell.startswith("./cjsh"):
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
    elif shell in ["tcsh", "csh"]:
        return SHELL_COMMANDS["csh"][command_key]
    else:
        return SHELL_COMMANDS["posix"][command_key]


def test_command(command_key: str) -> None:
    results: List[Tuple[str, float, float, float]] = []
    
    print("----------------------------------------------------------------------")
    print(f"Testing command: {command_key}")
    print("----------------------------------------------------------------------")
    
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
    
    results.sort(key=lambda x: x[1])
    
    all_commands.append(command_key)
    all_results.append(results)
    
    print(f"Completed testing: {command_key}")
    print()


def get_cjsh_version() -> str:
    try:
        result = subprocess.run("./build/cjsh --version", shell=True, 
                              capture_output=True, text=True, check=False)
        return result.stdout.strip() if result.stdout else "Version unavailable"
    except Exception:
        return "Version unavailable"


def print_summary() -> None:
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
    missing_binaries = []
    
    for binary_type in CJSH_BINARY_TYPES:
        binary_path = f"./build/cjsh{binary_type}"
        if not os.path.isfile(binary_path):
            missing_binaries.append(binary_path)
        elif not os.access(binary_path, os.X_OK):
            print(f"Warning: {binary_path} exists but is not executable")
    
    if ENABLE_BASELINE_TESTS:
        for shell in BASELINE_SHELLS:
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
    script_dir = os.path.dirname(os.path.abspath(__file__))
    parent_dir = os.path.dirname(script_dir)
    os.chdir(parent_dir)
    
    if not check_binaries_exist():
        sys.exit(1)
    
    print("All required binaries found. Starting performance tests...")
    print()
    
    for command_key in COMMAND_DESCRIPTIONS:
        test_command(command_key)
    
    print_summary()


if __name__ == "__main__":
    main()
