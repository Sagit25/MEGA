#!/usr/bin/env zsh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

FPS="${1:-30}"
FRAME_DIR="${2:-build/offline_frames}"
OUTPUT="${3:-build/mirage_offline.mp4}"
FRAME_PATTERN="$FRAME_DIR/frame_%04d.png"

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ffmpeg is required. Install it with: brew install ffmpeg" >&2
  exit 1
fi

if ! ls "$FRAME_DIR"/frame_*.png >/dev/null 2>&1; then
  echo "No frames found in: $FRAME_DIR" >&2
  echo "Expected files like: $FRAME_DIR/frame_0000.png" >&2
  exit 1
fi

mkdir -p "$(dirname "$OUTPUT")"

ffmpeg \
  -y \
  -framerate "$FPS" \
  -i "$FRAME_PATTERN" \
  -c:v libx264 \
  -pix_fmt yuv420p \
  "$OUTPUT"

echo "Video written: $OUTPUT"
