#!/usr/bin/env python3

import subprocess
import time
import statistics
import sys
import os
from typing import List, Tuple, Dict

# Configuration
RUNS = 30
COMMANDS = [
    "-c ls",
    "--version",
    "-c 'echo hello world'",
    "-c pwd",
    "-c 'echo $(date)'",
    "-c 'echo $SHELL'",
    "-c 'ls -la'",
    "-c exit",
    "-c 'for i in {1..1000}; do echo $i; done'"
]
BASELINE_SHELLS = ["fish", "bash", "zsh", "nu", "elvish"]
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
            print(f"Average time for {shell}: {average:.3f} ms")
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
    for command in COMMANDS:
        test_command(command)
    
    # Print final summary
    print_summary()


if __name__ == "__main__":
    main()
