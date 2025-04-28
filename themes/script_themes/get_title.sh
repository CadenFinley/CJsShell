#!/bin/bash
echo "$(whoami)@$(hostname -s): $(pwd | sed "s|$HOME|~|")"
