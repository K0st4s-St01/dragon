#!/bin/bash
set -e

BUILD_DIR="build"
BINARY="dragon_editor"

# Install dependencies based on distro
install_deps() {
    if [ -f /etc/arch-release ]; then
        echo "[*] Detected Arch Linux - installing dependencies..."
        sudo pacman -S --needed --noconfirm cmake gcc pkg-config glfw-x11 mesa tree-sitter
    elif [ -f /etc/debian_version ] || [ -f /etc/lsb-release ]; then
        echo "[*] Detected Debian/Ubuntu - installing dependencies..."
        sudo apt update
        sudo apt install -y cmake build-essential pkg-config libglfw3-dev libgl-dev libtree-sitter-dev
    elif [ -f /etc/redhat-release ]; then
        echo "[*] Detected Fedora/RHEL - installing dependencies..."
        sudo dnf install -y cmake gcc pkg-config glfw-devel mesa-libGL-devel tree-sitter-devel
    else
        echo "[!] Unknown distro - please install manually:"
        echo "    cmake, pkg-config, glfw, gl/mesa, tree-sitter"
        read -p "Press Enter to continue..."
    fi
}

# Clean build directory
clean_build() {
    if [ -d "$BUILD_DIR" ]; then
        echo "[*] Cleaning build directory..."
        rm -rf "$BUILD_DIR"
    fi
}

# Init git submodules
init_submodules() {
    if [ -f .gitmodules ]; then
        echo "[*] Initializing git submodules..."
        git submodule update --init --recursive
    fi
    
    # Verify vendor files exist
    if [ ! -f vendor/tomlc99/toml.c ]; then
        echo "[!] vendor/tomlc99/toml.c not found!"
        echo "    If you downloaded a zip, clone the repo instead:"
        echo "    git clone --recurse-submodules <repo-url>"
        exit 1
    fi
}

# Parse arguments
INSTALL_DEPS=false
CLEAN=true
for arg in "$@"; do
    case $arg in
        --deps) INSTALL_DEPS=true ;;
        --no-clean) CLEAN=false ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --deps       Install system dependencies"
            echo "  --no-clean   Don't clean build directory before building"
            echo "  --help       Show this help"
            exit 0
            ;;
    esac
done

# Install dependencies if requested
if [ "$INSTALL_DEPS" = true ]; then
    install_deps
fi

# Init git submodules
init_submodules

# Clean build directory
if [ "$CLEAN" = true ]; then
    clean_build
fi

echo "[*] Configuring..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "[*] Building..."
cmake --build "$BUILD_DIR" -j$(nproc)

echo "[*] Installing to /usr/local/bin..."
sudo install -m 755 "$BUILD_DIR/$BINARY" /usr/local/bin/

echo "[+] Done. Run with: $BINARY"
