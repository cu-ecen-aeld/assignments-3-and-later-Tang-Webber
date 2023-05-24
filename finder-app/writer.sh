#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Error: Two arguments are required."
    exit 1
fi

writefile="$1"
writestr="$2"
if [ -z "$writefile" ]; then
    echo "Error: Writefile argument is missing."
    exit 1
fi
if [ -z "$writestr" ]; then
    echo "Error: Writestr argument is missing."
    exit 1
fi

mkdir -p "$(dirname "$writefile")"
if [ $? -ne 0 ]; then
    echo "Error: Failed to create the directory."
    exit 1
fi

echo "$writestr" > "$writefile"
if [ $? -ne 0 ]; then
    echo "Error: Failed to create the file."
    exit 1
fi

echo "File created successfully: $writefile"
exit 0
