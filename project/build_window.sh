#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

CONFIG="${CONFIG:-Release}"
GENERATOR="${GENERATOR:-Visual Studio 17 2022}"
ARCH="${ARCH:-x64}"

if [[ "${1:-}" == "--clean" ]]; then
  rm -rf build
  shift
fi

cmake_args=(-S . -B build)

if [[ -n "$GENERATOR" && "$GENERATOR" != "default" ]]; then
  cmake_args+=(-G "$GENERATOR")
fi

if [[ -n "$ARCH" && "$ARCH" != "default" ]]; then
  cmake_args+=(-A "$ARCH")
fi

cmake "${cmake_args[@]}"
cmake --build build --config "$CONFIG" --parallel

if [[ -f "../dlls/assimp-vc140-mt.dll" ]]; then
  for output_dir in "build/$CONFIG" "build"; do
    if [[ -d "$output_dir" ]]; then
      cp -f "../dlls/assimp-vc140-mt.dll" "$output_dir/"
    fi
  done
fi

if [[ -f "build/$CONFIG/main.exe" ]]; then
  echo "Build finished: $SCRIPT_DIR/build/$CONFIG/main.exe"
  echo "Run with: cd $SCRIPT_DIR/build/$CONFIG && ./main.exe"
elif [[ -f "build/main.exe" ]]; then
  echo "Build finished: $SCRIPT_DIR/build/main.exe"
  echo "Run with: cd $SCRIPT_DIR/build && ./main.exe"
else
  echo "Build finished, but main.exe was not found in the expected output directories."
fi
