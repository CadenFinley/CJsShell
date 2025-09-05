#!/bin/bash

RUNS=100
COMMAND="-c exit"
BASELINE_SHELLS=("fish" "bash" "zsh")
cjsh_binary_types=("_speed03" "_space" "_speed02")

ENABLE_BASELINE_TESTS=true

results=()

echo "----------------------------------------------------------------------"

for type in "${cjsh_binary_types[@]}"; do
  echo
  echo "Timing ./cjsh${type} $COMMAND"
  echo
  total_time=0
  for i in $(seq 1 $RUNS); do
    start_time=$(date +%s.%N)
    ./build/cjsh${type} $COMMAND
    end_time=$(date +%s.%N)
    elapsed_time=$(echo "$end_time - $start_time" | bc)
    total_time=$(echo "$total_time + $elapsed_time" | bc)
  done

  average_time=$(echo "$total_time / $RUNS" | bc -l)
  results+=("Average time for ./cjsh${type}: $average_time seconds")
done

echo "----------------------------------------------------------------------"

if [ "$ENABLE_BASELINE_TESTS" = true ]; then
  for shell in "${BASELINE_SHELLS[@]}"; do
    echo
    echo "Timing $shell $COMMAND"
    echo
    total_time=0

    for i in $(seq 1 $RUNS); do
      start_time=$(date +%s.%N)
      $shell $COMMAND
      end_time=$(date +%s.%N)

      elapsed_time=$(echo "$end_time - $start_time" | bc)
      total_time=$(echo "$total_time + $elapsed_time" | bc)
    done

    average_time=$(echo "$total_time / $RUNS" | bc -l)
    results+=("Average time for $shell: $average_time seconds")
  done
fi

echo "----------------------------------------------------------------------"

# Sort results in ascending order of time
sorted_results=$(printf "%s\n" "${results[@]}" | sort -t: -k2 -n)

# Print sorted results
echo "Command used to test: $COMMAND"
echo "Results after $RUNS run(s):"
echo "----------------------------------------------------------------------"
while IFS= read -r result; do
  echo "$result"
done <<< "$sorted_results"
echo "----------------------------------------------------------------------"



# ----------------------------------------------------------------------
# Command used to test: -c exit
# Results after 100 run(s):
# ----------------------------------------------------------------------
# Average time for bash: .02011879000000000000 seconds
# Average time for zsh: .02325702000000000000 seconds
# Average time for ./cjsh_space: .02830989000000000000 seconds
# Average time for ./cjsh_speed03: .02843293000000000000 seconds
# Average time for ./cjsh_speed02: .02895336000000000000 seconds
# Average time for fish: .03511248000000000000 seconds
# Average time for ./cjsh_noopt: .04083061000000000000 seconds
# ----------------------------------------------------------------------
