#!/bin/bash

RUNS=100
COMMAND="-c not_a_command_"
BASELINE_SHELLS=("fish" "bash" "zsh")
cjsh_binary_types=("")

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
echo "Results after $RUNS run\(s\):"
echo "----------------------------------------------------------------------"
while IFS= read -r result; do
  echo "$result"
done <<< "$sorted_results"
echo "----------------------------------------------------------------------"



# ----------------------------------------------------------------------
# Command used to test: -c ls
# Results after 100 run\(s\):
# ----------------------------------------------------------------------
# Average time for bash: .02784732000000000000 seconds
# Average time for ./cjsh: .02892087000000000000 seconds
# Average time for zsh: .03072579000000000000 seconds
# Average time for fish: .06455507000000000000 seconds
# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
# Command used to test: -c exit
# Results after 100 run\(s\):
# ----------------------------------------------------------------------
# Average time for bash: .01964337000000000000 seconds
# Average time for zsh: .02233893000000000000 seconds
# Average time for ./cjsh: .03027081000000000000 seconds
# Average time for fish: .03632348000000000000 seconds
# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
# Command used to test: -c whoami
# Results after 100 run\(s\):
# ----------------------------------------------------------------------
# Average time for bash: .02864360000000000000 seconds
# Average time for zsh: .03195121000000000000 seconds
# Average time for ./cjsh: .04345792000000000000 seconds
# Average time for fish: .04902014000000000000 seconds
# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
# Command used to test: -c LS
# Results after 100 run\(s\):
# ----------------------------------------------------------------------
# Average time for bash: .02781995000000000000 seconds
# Average time for zsh: .03193810000000000000 seconds
# Average time for ./cjsh: .04050259000000000000 seconds
# Average time for fish: .04628635000000000000 seconds
# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
# Command used to test: -c cd
# Results after 100 run\(s\):
# ----------------------------------------------------------------------
# Average time for bash: .01957051000000000000 seconds
# Average time for zsh: .02334886000000000000 seconds
# Average time for ./cjsh: .02748185000000000000 seconds
# Average time for fish: .03580358000000000000 seconds
# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
# Command used to test: -c pwd
# Results after 100 run\(s\):
# ----------------------------------------------------------------------
# Average time for bash: .02025657000000000000 seconds
# Average time for zsh: .02473795000000000000 seconds
# Average time for fish: .03639285000000000000 seconds
# Average time for ./cjsh: .04022909000000000000 seconds
# ----------------------------------------------------------------------

# ----------------------------------------------------------------------
# Command used to test: -c not_a_command_
# Results after 100 run\(s\):
# ----------------------------------------------------------------------
# Average time for bash: .02066980000000000000 seconds
# Average time for zsh: .02492144000000000000 seconds
# Average time for ./cjsh: .03347663000000000000 seconds
# Average time for fish: .03887924000000000000 seconds
# ----------------------------------------------------------------------
