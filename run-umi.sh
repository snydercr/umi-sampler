#!/usr/bin/env bash
set -euo pipefail

# Always run from the project dir (optional, but avoids path issues)
cd /home/umi-pi5-0/umi-sampler

# Run the program (add args if you want)
./build/umi-sampler

# Keep the terminal open so you can see logs/errors
echo
echo "UMI Sampler exited with code $?."
read -rp "Press Enter to close this window..."
