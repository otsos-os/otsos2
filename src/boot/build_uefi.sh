#!/bin/bash

# Build script for OTSOS UEFI bootloader

set -e

echo "Building OTSOS UEFI Bootloader..."

# Check if we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/uefi"

# Create build directory
mkdir -p build
cd build

# Build the bootloader
make clean
make

# Check if build succeeded
if [ -f uefi_loader.efi ]; then
    echo "Build successful!"
    echo "UEFI bootloader: uefi_loader.efi"
    
    # Copy to output directory
    mkdir -p ../../../output
    cp uefi_loader.efi ../../../output/
    
    echo "Copied to output/uefi_loader.efi"
else
    echo "Build failed!"
    exit 1
fi

echo "Done!"
