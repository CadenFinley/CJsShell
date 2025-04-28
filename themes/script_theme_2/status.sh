#!/bin/bash

# Check if the last status code is passed as an argument
# This would require your shell to pass the $? value
if [ -f /tmp/last_status_code ]; then
  status=$(cat /tmp/last_status_code)
  if [ "$status" != "0" ]; then
    echo " ✘ $status "
  else
    echo ""
  fi
else
  echo ""
fi
