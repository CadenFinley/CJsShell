#!/usr/bin/env bash
# Quick test script to verify the threaded input monitor is working

echo "Testing CJ's Shell with threaded input monitor..."

# Test 1: Basic startup
echo "Test 1: Basic shell startup with threading"
echo "echo 'Threading test successful'" | ./build/cjsh --debug 2>&1 | grep -E "(ThreadedInputMonitor|Threading test successful)" | head -5

echo -e "\nTest 2: Check if monitor starts and stops correctly"
echo "echo 'exit'" | timeout 5s ./build/cjsh --debug 2>&1 | grep -E "(ThreadedInputMonitor|started successfully|stopped)" | head -10

echo -e "\nTest 3: Check cleanup on exit"
echo "echo 'exit'" | ./build/cjsh --debug 2>&1 | grep -E "(Shutting down|global instance shut down|Cleanup complete)" | tail -5

echo -e "\nAll basic tests completed!"