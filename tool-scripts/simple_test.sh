#!/usr/bin/env cjsh
# Simple test script that cjsh can handle
echo "Testing cjsh script execution capabilities"
echo "Current working directory:"
pwd
echo $SHELL
version
echo "Listing files:"
ls -la | head -3
echo "Testing variable assignment:"
TEST_VAR="Hello from cjsh"
echo $TEST_VAR
echo "Testing command execution:"
echo "Date: $(date)"
echo "Script completed successfully!"
