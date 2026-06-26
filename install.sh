#!/bin/bash
set -e

BUILD_DIR="build"
BINARY="dragon_editor"

echo "[*] Configuring..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "[*] Building..."
cmake --build "$BUILD_DIR" -j$(nproc)

echo "[*] Installing to /usr/local/bin..."
sudo install -m 755 "$BUILD_DIR/$BINARY" /usr/local/bin/

echo "[+] Done. Run with: $BINARY"
