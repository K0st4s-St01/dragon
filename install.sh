#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
BUILD_TYPE="Release"
PREFIX="/usr/local"
DESTDIR_VALUE=""
INSTALL_DEPS=false
CLEAN=false
RUN_TESTS=false
DO_INSTALL=true
CONFIGURE_ONLY=false
JOBS=""

usage() {
    cat <<EOF
Usage: ./install.sh [options]

Builds Dragon with CMake and optionally installs it.

Options:
  --deps              Install system dependencies for this distro
  --clean             Remove the build directory before configuring
  --debug             Build with CMAKE_BUILD_TYPE=Debug
  --release           Build with CMAKE_BUILD_TYPE=Release (default)
  --build-dir DIR     Use a custom build directory (default: ./build)
  --prefix DIR        Install prefix (default: /usr/local)
  --destdir DIR       Staging root for packaging
  --jobs N            Parallel build jobs (default: detected CPU count)
  --test              Run unit tests after building
  --no-install        Build only; skip cmake --install
  --configure-only    Configure and stop
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

run_privileged() {
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        echo "error: this action needs elevated privileges, but sudo was not found" >&2
        return 1
    fi
}

install_deps() {
    if [ -f /etc/arch-release ]; then
        echo "==> Installing dependencies with pacman"
        run_privileged pacman -S --needed --noconfirm cmake gcc pkg-config glfw-x11 mesa tree-sitter
    elif [ -f /etc/debian_version ] || [ -f /etc/lsb-release ]; then
        echo "==> Installing dependencies with apt"
        run_privileged apt update
        run_privileged apt install -y cmake build-essential pkg-config libglfw3-dev libgl-dev libtree-sitter-dev
    elif [ -f /etc/fedora-release ] || [ -f /etc/redhat-release ]; then
        echo "==> Installing dependencies with dnf"
        run_privileged dnf install -y cmake gcc gcc-c++ pkg-config glfw-devel mesa-libGL-devel tree-sitter-devel
    else
        cat >&2 <<EOF
error: unknown distro. Install these manually:
  cmake, C/C++ compiler, pkg-config, GLFW3 development files,
  OpenGL/Mesa development files, tree-sitter development files
EOF
        return 1
    fi
}

check_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "error: missing required tool: $1" >&2
        exit 1
    fi
}

check_vendor() {
    if [ -f "$ROOT_DIR/vendor/tomlc99/toml.c" ] && [ -f "$ROOT_DIR/vendor/glad/src/glad.c" ]; then
        return
    fi

    if [ -f "$ROOT_DIR/.gitmodules" ] && [ -d "$ROOT_DIR/.git" ]; then
        git -C "$ROOT_DIR" submodule update --init --recursive
    fi

    if [ ! -f "$ROOT_DIR/vendor/tomlc99/toml.c" ] || [ ! -f "$ROOT_DIR/vendor/glad/src/glad.c" ]; then
        cat >&2 <<EOF
error: vendored sources are missing.
Clone with submodules or initialize them:
  git submodule update --init --recursive
EOF
        exit 1
    fi
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --deps) INSTALL_DEPS=true ;;
        --clean) CLEAN=true ;;
        --debug) BUILD_TYPE="Debug" ;;
        --release) BUILD_TYPE="Release" ;;
        --build-dir)
            shift
            [ "$#" -gt 0 ] || { echo "error: --build-dir needs a value" >&2; exit 1; }
            BUILD_DIR="$1"
            ;;
        --prefix)
            shift
            [ "$#" -gt 0 ] || { echo "error: --prefix needs a value" >&2; exit 1; }
            PREFIX="$1"
            ;;
        --destdir)
            shift
            [ "$#" -gt 0 ] || { echo "error: --destdir needs a value" >&2; exit 1; }
            DESTDIR_VALUE="$1"
            ;;
        --jobs|-j)
            shift
            [ "$#" -gt 0 ] || { echo "error: --jobs needs a value" >&2; exit 1; }
            JOBS="$1"
            ;;
        --test) RUN_TESTS=true ;;
        --no-install) DO_INSTALL=false ;;
        --configure-only) CONFIGURE_ONLY=true ;;
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

if [ "$INSTALL_DEPS" = true ]; then
    install_deps
fi

check_tool cmake
check_vendor

if [ "$CLEAN" = true ] && [ -d "$BUILD_DIR" ]; then
    echo "==> Removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

echo "==> Configuring ($BUILD_TYPE)"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

if [ "$CONFIGURE_ONLY" = true ]; then
    echo "==> Configure complete"
    exit 0
fi

echo "==> Building with $JOBS job(s)"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

if [ "$RUN_TESTS" = true ]; then
    echo "==> Running tests"
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

if [ "$DO_INSTALL" = true ]; then
    echo "==> Installing to $PREFIX"
    if [ -n "$DESTDIR_VALUE" ]; then
        env DESTDIR="$DESTDIR_VALUE" cmake --install "$BUILD_DIR" --prefix "$PREFIX"
    elif [ -w "$PREFIX" ] || { [ ! -e "$PREFIX" ] && [ -w "$(dirname "$PREFIX")" ]; }; then
        cmake --install "$BUILD_DIR" --prefix "$PREFIX"
    else
        run_privileged cmake --install "$BUILD_DIR" --prefix "$PREFIX"
    fi
fi

echo "==> Done"
echo "Run with: dragon_editor"
