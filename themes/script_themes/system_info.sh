#!/bin/bash
# Simple script to get system information for shell prompt

# CPU load (1-minute average)
CPU_LOAD=$(top -l 1 | grep "CPU usage" | awk '{print $3}' | tr -d '%')

# Memory usage
MEM_USED=$(vm_stat | grep "Pages active" | awk '{print $3}' | tr -d '.')
MEM_WIRED=$(vm_stat | grep "Pages wired down" | awk '{print $4}' | tr -d '.')
MEM_TOTAL=$(sysctl hw.memsize | awk '{print $2 / 1073741824}' | xargs printf "%.0f")
MEM_PERC=$(echo "scale=0; ($MEM_USED + $MEM_WIRED) * 4096 * 100 / ($MEM_TOTAL * 1073741824)" | bc)

# Battery percent if available
if [ -e /usr/bin/pmset ]; then
    BATT=$(pmset -g batt | grep -Eo "\d+%" | cut -d% -f1)
    if [ ! -z "$BATT" ]; then
        BATT_INFO="⚡${BATT}% "
    else
        BATT_INFO=""
    fi
else
    BATT_INFO=""
fi

# Output format for prompt
echo "${BATT_INFO}CPU:${CPU_LOAD}% MEM:${MEM_PERC}%"
