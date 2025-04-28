#!/bin/bash

# Get current username
username=$(whoami)
hostname=$(hostname -s)

# Display username@hostname
echo "$username@$hostname"
