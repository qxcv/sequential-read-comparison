#!/bin/bash

set -euo pipefail

# Create a smaller seed file of random data (e.g., 1GB)
dd if=/dev/urandom of=seedfile bs=1M count=1024

# Create the target file
rm -f benchmark-file
touch benchmark-file

# Append the seed file to the target file 48 times to get a 48GB file
for i in {1..48}; do
    cat seedfile >> benchmark-file
done

# Remove the seed file if it's no longer needed
rm seedfile

