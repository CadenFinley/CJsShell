#!/bin/bash

RUNS=100
COMMAND="-c ls"
BASELINE_SHELLS=("fish" "bash" "zsh")
cjsh_binary_types=("")

ENABLE_BASELINE_TESTS=true

shells=()
averages=()
min_times=()
max_times=()

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
    ./build/cjsh${type} $COMMAND
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

echo "----------------------------------------------------------------------"

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
      $shell $COMMAND
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

echo "Command used to test: $COMMAND"
echo "Results after $RUNS run\(s\):"
./build/cjsh --version
echo "----------------------------------------------------------------------"
while IFS=: read -r shell average min max; do
  echo "Average time for $shell: $average seconds"
  echo "  Min time: $min seconds"
  echo "  Max time: $max seconds"
done <<< "$sorted_results"
echo "----------------------------------------------------------------------"

rm "$temp_file"

# ----------------------------------------------------------------------
# Command used to test: --startup-test --no-source
# Results after 50 run\(s\):
# 2.3.13-PRERELEASE
# ----------------------------------------------------------------------
# Average time for ./cjsh: .05827370000000000000 seconds
#   Min time: .056715000 seconds
#   Max time: .062637000 seconds
# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
# Command used to test: --version
# Results after 50 run\(s\):
# 2.3.13-PRERELEASE
# ----------------------------------------------------------------------
# Average time for zsh: .01953018000000000000 seconds
#   Min time: .018906000 seconds
#   Max time: .026451000 seconds
# Average time for bash: .01969208000000000000 seconds
#   Min time: .019451000 seconds
#   Max time: .020413000 seconds
# Average time for fish: .02294484000000000000 seconds
#   Min time: .022452000 seconds
#   Max time: .031500000 seconds
# Average time for ./cjsh: .02683664000000000000 seconds
#   Min time: .026469000 seconds
#   Max time: .031983000 seconds
# ----------------------------------------------------------------------



# ----------------------------------------------------------------------
# Command used to test: -c ls
# Results after 50 run\(s\):
# 2.3.13-PRERELEASE
# ----------------------------------------------------------------------
# Average time for bash: .02672126000000000000 seconds
#   Min time: .026359000 seconds
#   Max time: .027498000 seconds
# Average time for ./cjsh: .02794312000000000000 seconds
#   Min time: .026916000 seconds
#   Max time: .032809000 seconds
# Average time for zsh: .02963912000000000000 seconds
#   Min time: .029228000 seconds
#   Max time: .031571000 seconds
# Average time for fish: .06182280000000000000 seconds
#   Min time: .060772000 seconds
#   Max time: .072573000 seconds
# ----------------------------------------------------------------------
