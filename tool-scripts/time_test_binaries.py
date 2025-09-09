#!/usr/bin/env python3

import subprocess
import time
import statistics
import sys
import os
from typing import List, Tuple, Dict

# Configuration
RUNS = 10
COMMANDS = [
    "-c ls",
    "--version", 
    "-c 'echo hello world'",
    "-c pwd",
    "-c 'echo $(date)'",
    "-c 'echo $SHELL'",
    "-c 'ls -lhaS'"
]
BASELINE_SHELLS = ["fish", "bash", "zsh", "cjsh"]
CJSH_BINARY_TYPES = [""]  # Empty string means no suffix

ENABLE_BASELINE_TESTS = True

# Global storage for results
all_results: List[List[Tuple[str, float, float, float]]] = []
all_commands: List[str] = []


def run_command_with_timing(shell_cmd: str, command: str) -> float:
    """Run a command and return the elapsed time in seconds."""
    full_command = f"{shell_cmd} {command}"
    
    start_time = time.perf_counter()
    try:
        # Use shell=True to properly handle command parsing
        subprocess.run(full_command, shell=True, capture_output=True, check=False)
    except Exception:
        # If command fails, still return the time it took
        pass
    end_time = time.perf_counter()
    
    return end_time - start_time


def test_command(command: str) -> None:
    """Test a single command across all shells."""
    results: List[Tuple[str, float, float, float]] = []
    
    print("----------------------------------------------------------------------")
    print(f"Testing command: {command}")
    print("----------------------------------------------------------------------")
    
    # Test cjsh binary types
    for binary_type in CJSH_BINARY_TYPES:
        shell_name = f"./cjsh{binary_type}"
        shell_path = f"./build/cjsh{binary_type}"
        
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
    all_commands.append(command)
    all_results.append(results)
    
    print(f"Completed testing: {command}")
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
    
    for i, command in enumerate(all_commands):
        print()
        print(f"Command: {command}")
        print("----------------------------------------------------------------------")
        
        for shell, average, min_time, max_time in all_results[i]:
            print(f"Average time for {shell}: {average:.6f} seconds")
            print(f"  Min time: {min_time:.6f} seconds")
            print(f"  Max time: {max_time:.6f} seconds")
        
        print("----------------------------------------------------------------------")
    
    print("======================================================================")


def main() -> None:
    """Main execution function."""
    # Change to the script's directory to ensure relative paths work
    script_dir = os.path.dirname(os.path.abspath(__file__))
    parent_dir = os.path.dirname(script_dir)
    os.chdir(parent_dir)
    
    # Test all commands
    for command in COMMANDS:
        test_command(command)
    
    # Print final summary
    print_summary()


if __name__ == "__main__":
    main()
