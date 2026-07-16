#!/bin/bash

# Test build script for UEFI bootloader

echo "Testing UEFI bootloader build..."

cd /home/ventilator3000/Projects/otsos2/src/boot/uefi

# Clean previous build
make clean 2>/dev/null

# Try to build
echo "Building..."
make

if [ $? -eq 0 ]; then
    echo "✅ Build successful!"
    ls -lh uefi_loader.efi 2>/dev/null || echo "No .efi file found"
else
    echo "❌ Build failed!"
    exit 1
fi
