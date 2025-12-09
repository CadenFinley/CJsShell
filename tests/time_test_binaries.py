#!/usr/bin/env python3

import subprocess
import time
import statistics
import sys
import os
from typing import List, Tuple, Dict, Optional

RUNS = 10

SHELL_COMMANDS = {
    "posix": {
        "loop": "-c 'for i in {1..5000}; do echo $i; done'",
        "loop_even": "-c 'for i in {1..5000}; do if [ $((i % 2)) -eq 0 ]; then echo $i; fi; done'",
        "prime_sieve": "-c 'MAX=100; primes=\"\"; i=2; while [ $i -le $MAX ]; do is_prime=1; for p in $primes; do if [ $((p*p)) -gt $i ]; then break; fi; if [ $((i % p)) -eq 0 ]; then is_prime=0; break; fi; done; if [ $is_prime -eq 1 ]; then echo $i; primes=\"$primes $i\"; fi; i=$((i+1)); done'",
        "branching": "-c 'count=0; for i in {1..2000}; do if [ $((i % 15)) -eq 0 ]; then count=$((count+1)); elif [ $((i % 3)) -eq 0 ]; then :; elif [ $((i % 5)) -eq 0 ]; then :; fi; done; echo $count'",
        "function_calls": "-c 'sum(){ out=0; for n in \"$@\"; do out=$((out+n)); done; echo \"$out\"; }; for i in {1..400}; do sum 1 2 3 4 5 >/dev/null; done'",
        "subshell_traversal": "-c 'for dir in /bin /usr/bin /usr/sbin; do if [ -d \"$dir\" ]; then (cd \"$dir\" && ls >/dev/null); fi; done'"
    },
    "fish": {
        "loop": "-c 'for i in (seq 5000); echo $i; end'",
        "loop_even": "-c 'for i in (seq 5000); if test (math \"$i % 2\") -eq 0; echo $i; end; end'",
        "prime_sieve": "-c 'set MAX 100; set primes; set i 2; while test $i -le $MAX; set is_prime 1; for p in $primes; if test (math \"$p * $p\") -gt $i; break; end; if test (math \"$i % $p\") -eq 0; set is_prime 0; break; end; end; if test $is_prime -eq 1; echo $i; set primes $primes $i; end; set i (math \"$i + 1\"); end'",
        "branching": "-c 'set count 0; for i in (seq 1 2000); if test (math \"$i % 15\") -eq 0; set count (math \"$count + 1\"); else if test (math \"$i % 3\") -eq 0; math \"$i + 0\" >/dev/null; else if test (math \"$i % 5\") -eq 0; math \"$i + 0\" >/dev/null; end; end; echo $count'",
        "function_calls": "-c 'function sum; set out 0; for n in $argv; set out (math \"$out + $n\"); end; echo $out; end; for i in (seq 1 400); sum 1 2 3 4 5 >/dev/null; end'",
        "subshell_traversal": "-c 'for dir in /bin /usr/bin /usr/sbin; if test -d $dir; pushd $dir >/dev/null; ls >/dev/null; popd >/dev/null; end; end'"
    },
    "nu": {
        "loop": "-c '1..5000 | each { |i| echo $i }'",
        "loop_even": "-c '1..5000 | where { |i| $i mod 2 == 0 } | each { |i| echo $i }'",
        "prime_sieve": "-c 'let MAX = 100; mut primes = []; mut i = 2; while $i <= $MAX { mut is_prime = true; for p in $primes { if ($p * $p) > $i { break }; if ($i mod $p) == 0 { $is_prime = false; break } }; if $is_prime { echo $i; $primes = ($primes | append $i) }; $i = $i + 1 }'",
        "branching": "-c 'mut count = 0; for i in 1..2000 { if (($i mod 15) == 0) { $count += 1 } else if (($i mod 3) == 0) { } else if (($i mod 5) == 0) { } }; echo $count'",
        "function_calls": "-c 'def sum [values: list<int>] { mut out = 0; for v in $values { $out += $v }; $out }; for _ in 1..400 { sum [1 2 3 4 5] | ignore }'",
        "subshell_traversal": "-c 'for dir in [/bin /usr/bin /usr/sbin] { if ($dir | path exists) { cd $dir; ls | ignore } }'"
    }
}

COMMAND_PLAN = [
    {"key": "loop", "description": "High-iteration loop with stdout output"},
    {"key": "loop_even", "description": "Loop with conditional filtering"},
    {"key": "prime_sieve", "description": "Generate primes up to 100 using incremental divisibility checks"},
    {"key": "branching", "description": "Nested conditionals with arithmetic checks"},
    {"key": "function_calls", "description": "Define and repeatedly invoke a function"},
    {"key": "subshell_traversal", "description": "Traverse directories using subshells or directory stack"}
]
BASELINE_SHELLS = ["cjsh", "bash", "zsh", "fish", "nu", "osh"]
CJSH_BINARY_TYPES = [""]

ENABLE_BASELINE_TESTS = True

all_results: List[List[Tuple[str, float, float, float]]] = []
all_commands: List[Dict[str, str]] = []


EXPECTED_OUTPUTS = {
    "hello": {"contains": "hello world", "exact": False},
    "loop": {"line_count": 5000, "tolerance": 0},
    "loop_even": {"line_count": 2500, "tolerance": 0},
    "prime_sieve": {"line_count": 25, "tolerance": 0, "contains": "97", "exact": False},
    "branching": {"contains": "133", "exact": True},
    "exit": {"returncode": 0},
}


def validate_command_output(shell_cmd: str, command: str, command_key: str) -> Tuple[bool, str]:
    """
    Validate that a command produces expected output.
    Returns (is_valid, error_message)
    """
    if command_key not in EXPECTED_OUTPUTS:
        return (True, "")  
    
    full_command = f"{shell_cmd} {command}"
    expected = EXPECTED_OUTPUTS[command_key]
    
    try:
        result = subprocess.run(full_command, shell=True, capture_output=True, 
                              text=True, timeout=30, check=False)
    except subprocess.TimeoutExpired:
        return (False, "Command timed out (>30s)")
    except Exception as e:
        return (False, f"Command execution failed: {str(e)}")
    
    
    if "returncode" in expected:
        if result.returncode != expected["returncode"]:
            return (False, f"Expected return code {expected['returncode']}, got {result.returncode}")
    
    
    if "contains" in expected:
        if expected.get("exact", False):
            
            if expected["contains"].strip() not in result.stdout.strip():
                return (False, f"Expected output to contain exactly '{expected['contains']}', got: {result.stdout.strip()[:100]}")
        else:
            
            if expected["contains"] not in result.stdout:
                return (False, f"Expected output to contain '{expected['contains']}', got: {result.stdout[:100]}")
    
    
    if "line_count" in expected:
        actual_lines = len([line for line in result.stdout.splitlines() if line.strip()])
        expected_lines = expected["line_count"]
        tolerance = expected.get("tolerance", 0)
        
        if abs(actual_lines - expected_lines) > tolerance:
            return (False, f"Expected {expected_lines} lines of output (±{tolerance}), got {actual_lines}")
    
    return (True, "")


def run_command_with_timing(shell_cmd: str, command: str) -> float:
    full_command = f"{shell_cmd} {command}"
    
    start_time = time.perf_counter()
    try:
        subprocess.run(full_command, shell=True, capture_output=True, check=False)
    except Exception:
        pass
    end_time = time.perf_counter()
    
    return (end_time - start_time) * 1000


def get_shell_command(shell: str, command_key: str) -> Optional[str]:
    if shell in ["bash", "zsh", "ksh", "osh"] or shell.startswith("./cjsh"):
        return SHELL_COMMANDS["posix"].get(command_key)
    if shell in ["fish", "../fish-shell/build/fish"]:
        return SHELL_COMMANDS["fish"].get(command_key)
    if shell == "nu":
        return SHELL_COMMANDS["nu"].get(command_key)
    if shell == "elvish":
        return SHELL_COMMANDS["elvish"].get(command_key)
    if shell == "ion":
        return SHELL_COMMANDS["ion"].get(command_key)
    if shell == "xonsh":
        return SHELL_COMMANDS["xonsh"].get(command_key)
    if shell in ["tcsh", "csh"]:
        return SHELL_COMMANDS["csh"].get(command_key)
    return SHELL_COMMANDS["posix"].get(command_key)


def test_command(command_spec: Dict[str, str]) -> None:
    command_key = command_spec["key"]
    results: List[Tuple[str, float, float, float]] = []
    
    print("----------------------------------------------------------------------")
    print(f"Testing command: {command_key}")
    description = command_spec.get("description", "")
    if description:
        print(description)
    print("----------------------------------------------------------------------")
    
    for binary_type in CJSH_BINARY_TYPES:
        shell_name = f"./cjsh{binary_type}"
        shell_path = f"./build/cjsh{binary_type}"
        command = get_shell_command(shell_name, command_key)

        if command is None:
            print(f"Skipping {shell_name}: command '{command_key}' not defined")
            continue
        
        
        print()
        print(f"Validating {shell_name} {command}")
        is_valid, error_msg = validate_command_output(shell_path, command, command_key)
        if not is_valid:
            print(f"  VALIDATION FAILED: {error_msg}")
            print(f"  Skipping performance test for {shell_name}")
            continue
        else:
            print(f"  Validation passed")
        
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

            if command is None:
                print()
                print(f"Skipping {shell}: command '{command_key}' not defined")
                continue
            
            
            print()
            print(f"Validating {shell} {command}")
            is_valid, error_msg = validate_command_output(shell, command, command_key)
            if not is_valid:
                print(f"  VALIDATION FAILED: {error_msg}")
                print(f"  Skipping performance test for {shell}")
                continue
            else:
                print(f"  Validation passed")
            
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
    
    all_commands.append(command_spec)
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
    
    for i, command_spec in enumerate(all_commands):
        command_key = command_spec["key"]
        print()
        description = command_spec.get("description", "")
        if description:
            print(f"Command: {command_key} - {description}")
        else:
            print(f"Command: {command_key}")
        print("----------------------------------------------------------------------")
        
        if not all_results[i]:
            print("No shells executed this command (unsupported).")
        else:
            for shell, average, min_time, max_time in all_results[i]:
                actual_command = get_shell_command(shell, command_key) or "N/A"
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
    
    for command_spec in COMMAND_PLAN:
        test_command(command_spec)
    
    print_summary()


if __name__ == "__main__":
    main()

