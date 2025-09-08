#!/bin/bash

RUNS=10
COMMANDS=("-c ls" "--version" "-c 'echo hello world'" "-c pwd" "-c 'echo $(date)'" "-c 'echo $SHELL'")
BASELINE_SHELLS=("fish" "bash" "zsh" "cjsh")
cjsh_binary_types=("")

ENABLE_BASELINE_TESTS=true

# Global arrays to store all results
ALL_RESULTS=()
ALL_COMMANDS=()

# Function to test a single command across all shells
test_command() {
  local COMMAND="$1"
  local shells=()
  local averages=()
  local min_times=()
  local max_times=()

  echo "----------------------------------------------------------------------"
  echo "Testing command: $COMMAND"
  echo "----------------------------------------------------------------------"

  for type in "${cjsh_binary_types[@]}"; do
    echo
    echo "Timing ./cjsh${type} $COMMAND"
    echo
    total_time=0
    min_time=9999999
    max_time=0
    
    for i in $(seq 1 $RUNS); do
      start_time=$(date +%s.%N)
      eval "./build/cjsh${type} $COMMAND"
      end_time=$(date +%s.%N)
      elapsed_time=$(echo "$end_time - $start_time" | bc)

      if (( $(echo "$elapsed_time < $min_time" | bc -l) )); then
        min_time=$elapsed_time
      fi
      if (( $(echo "$elapsed_time > $max_time" | bc -l) )); then
        max_time=$elapsed_time
      fi
      
      total_time=$(echo "$total_time + $elapsed_time" | bc)
    done

    average_time=$(echo "$total_time / $RUNS" | bc -l)
    shell_name="./cjsh${type}"
    shells+=("$shell_name")
    averages+=("$average_time")
    min_times+=("$min_time")
    max_times+=("$max_time")
  done

  if [ "$ENABLE_BASELINE_TESTS" = true ]; then
    for shell in "${BASELINE_SHELLS[@]}"; do
      echo
      echo "Timing $shell $COMMAND"
      echo
      total_time=0
      min_time=9999999
      max_time=0

      for i in $(seq 1 $RUNS); do
        start_time=$(date +%s.%N)
        eval "$shell $COMMAND"
        end_time=$(date +%s.%N)

        elapsed_time=$(echo "$end_time - $start_time" | bc)
        
        if (( $(echo "$elapsed_time < $min_time" | bc -l) )); then
          min_time=$elapsed_time
        fi
        if (( $(echo "$elapsed_time > $max_time" | bc -l) )); then
          max_time=$elapsed_time
        fi
        
        total_time=$(echo "$total_time + $elapsed_time" | bc)
      done

      average_time=$(echo "$total_time / $RUNS" | bc -l)
      shells+=("$shell")
      averages+=("$average_time")
      min_times+=("$min_time")
      max_times+=("$max_time")
    done
  fi

  echo "----------------------------------------------------------------------"

  temp_file=$(mktemp)

  for i in "${!shells[@]}"; do
    echo "${shells[$i]}:${averages[$i]}:${min_times[$i]}:${max_times[$i]}" >> "$temp_file"
  done

  sorted_results=$(sort -t: -k2 -n "$temp_file")

  # Store results for summary at the end
  ALL_COMMANDS+=("$COMMAND")
  ALL_RESULTS+=("$sorted_results")

  echo "Completed testing: $COMMAND"
  echo

  rm "$temp_file"
}

# Function to print all results summary
print_summary() {
  echo "======================================================================"
  echo "                           FINAL RESULTS SUMMARY"
  echo "======================================================================"
  echo "Total runs per command: $RUNS"
  ./build/cjsh --version
  echo "======================================================================"
  
  for i in "${!ALL_COMMANDS[@]}"; do
    echo
    echo "Command: ${ALL_COMMANDS[$i]}"
    echo "----------------------------------------------------------------------"
    while IFS=: read -r shell average min max; do
      echo "Average time for $shell: $average seconds"
      echo "  Min time: $min seconds"
      echo "  Max time: $max seconds"
    done <<< "${ALL_RESULTS[$i]}"
    echo "----------------------------------------------------------------------"
  done
  echo "======================================================================"
}

# Main execution: test all commands
for command in "${COMMANDS[@]}"; do
  test_command "$command"
done

# Print final summary
print_summary