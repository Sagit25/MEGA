#!/usr/bin/env zsh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

HOMEBREW_PREFIX="$(brew --prefix)"

export PKG_CONFIG_PATH="$HOMEBREW_PREFIX/lib/pkgconfig:$HOMEBREW_PREFIX/opt/glfw/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
export LIBRARY_PATH="$HOMEBREW_PREFIX/lib:$HOMEBREW_PREFIX/opt/glfw/lib:${LIBRARY_PATH:-}"
export CPATH="$HOMEBREW_PREFIX/include:${CPATH:-}"

if [[ "${1:-}" == "--clean" ]]; then
  rm -rf build
fi

cmake -S . -B build
cmake --build build -j"$(sysctl -n hw.logicalcpu)"

echo "Build finished: $SCRIPT_DIR/build/main"