#!/bin/bash

# Read the environment file line by line
while IFS='=' read -r key value; do
  # Ignore lines starting with '#' (comments) and empty lines
  if [[ "$key" != "#"* && -n "$key" ]]; then
    # Call vc env add for each variable
    echo -n "$value" | vc env add "$key" production
  fi
done < your_environment_file.env
