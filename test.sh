#!/bin/bash
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD="$DIR/build"

mkdir -p "$BUILD"

echo "=== Configuring ==="
cmake -S "$DIR" -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug

echo ""
echo "=== Building ==="
cmake --build "$BUILD" --target test_all -j"$(nproc)"

echo ""
echo "=== Running Tests ==="
cd "$BUILD"
ctest --output-on-failure --test-dir "$BUILD"
