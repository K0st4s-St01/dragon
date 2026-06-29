#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
BUILD_TYPE="Debug"
CLEAN=false
JOBS=""

usage() {
    cat <<EOF
Usage: ./test.sh [options]

Options:
  --clean             Remove the build directory before configuring
  --release           Use CMAKE_BUILD_TYPE=Release
  --debug             Use CMAKE_BUILD_TYPE=Debug (default)
  --build-dir DIR     Use a custom build directory (default: ./build)
  --jobs N            Parallel build jobs (default: detected CPU count)
  -h, --help          Show this help
EOF
}

jobs_default() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
    else
        getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2\n'
    fi
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --clean) CLEAN=true ;;
        --release) BUILD_TYPE="Release" ;;
        --debug) BUILD_TYPE="Debug" ;;
        --build-dir)
            shift
            [ "$#" -gt 0 ] || { echo "error: --build-dir needs a value" >&2; exit 1; }
            BUILD_DIR="$1"
            ;;
        --jobs|-j)
            shift
            [ "$#" -gt 0 ] || { echo "error: --jobs needs a value" >&2; exit 1; }
            JOBS="$1"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
    shift
done

if [ -z "$JOBS" ]; then
    JOBS="$(jobs_default)"
fi

if [ "$CLEAN" = true ] && [ -d "$BUILD_DIR" ]; then
    echo "==> Removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

echo "==> Configuring tests ($BUILD_TYPE)"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "==> Building tests with $JOBS job(s)"
cmake --build "$BUILD_DIR" --target test_all --parallel "$JOBS"

echo "==> Running tests"
ctest --test-dir "$BUILD_DIR" --output-on-failure
