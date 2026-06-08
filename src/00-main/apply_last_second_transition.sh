#!/usr/bin/env zsh

set -euo pipefail

CALLER_DIR="$PWD"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

resolve_arg_path() {
  local path="$1"
  if [[ "$path" == /* ]]; then
    echo "$path"
  else
    echo "$CALLER_DIR/$path"
  fi
}

INPUT="${1:-$SCRIPT_DIR/build/main_offline.mp4}"
OUTPUT="${2:-$SCRIPT_DIR/build/main_offline_last_transition.mp4}"
if [[ $# -ge 1 ]]; then
  INPUT="$(resolve_arg_path "$INPUT")"
fi
if [[ $# -ge 2 ]]; then
  OUTPUT="$(resolve_arg_path "$OUTPUT")"
fi
FADE_SECONDS="${3:-1.0}"

# Normalized fixed window-opening rectangle in the offline render.
# Override these if the camera/window framing changes:
#   ./apply_last_second_transition.sh input.mp4 output.mp4 1.0 x y w h color
MASK_X="${4:-0.0}"
MASK_Y="${5:-0.088}"
MASK_W="${6:-1.0}"
MASK_H="${7:-0.732}"
END_COLOR="${8:-black}"

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ffmpeg is required. Install it with: brew install ffmpeg" >&2
  exit 1
fi

if ! command -v ffprobe >/dev/null 2>&1; then
  echo "ffprobe is required. Install it with: brew install ffmpeg" >&2
  exit 1
fi

if [[ ! -f "$INPUT" ]]; then
  echo "Input video not found: $INPUT" >&2
  exit 1
fi

if ! ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of csv=p=0 "$INPUT" >/dev/null; then
  echo "Input video is not readable by ffmpeg: $INPUT" >&2
  exit 1
fi

VIDEO_DURATION="$(ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 "$INPUT")"
OUTRO_START="$(awk -v duration="$VIDEO_DURATION" -v fade="$FADE_SECONDS" 'BEGIN {
  start = duration - fade;
  if (start < 0) start = 0;
  printf "%.6f", start;
}')"

mkdir -p "$(dirname "$OUTPUT")"

VIDEO_CODEC_ARGS=(-c:v libx264 -preset veryfast -pix_fmt yuv420p)
if [[ "${USE_VIDEOTOOLBOX:-0}" == "1" ]] && ffmpeg -hide_banner -encoders 2>/dev/null | grep -q "h264_videotoolbox"; then
  VIDEO_CODEC_ARGS=(-c:v h264_videotoolbox -allow_sw 1 -b:v 12M -pix_fmt yuv420p)
fi

MASK_EXPR_X="trunc(iw*${MASK_X}/2)*2"
MASK_EXPR_Y="trunc(ih*${MASK_Y}/2)*2"
MASK_EXPR_W="trunc(iw*${MASK_W}/2)*2"
MASK_EXPR_H="trunc(ih*${MASK_H}/2)*2"

FILTER="[0:v]setpts=PTS-STARTPTS,split=2[body_src][outro_src];"
FILTER+="[body_src]trim=start=0:end=${OUTRO_START},setpts=PTS-STARTPTS[body];"
FILTER+="[outro_src]trim=start=${OUTRO_START},setpts=PTS-STARTPTS,split=2[outro_base][outro_fx];"
FILTER+="[outro_fx]crop=w='${MASK_EXPR_W}':h='${MASK_EXPR_H}':x='${MASK_EXPR_X}':y='${MASK_EXPR_Y}',"
FILTER+="fade=t=out:st=0:d=${FADE_SECONDS}:color=${END_COLOR}[outro_window];"
FILTER+="[outro_base][outro_window]overlay=x='trunc(W*${MASK_X}/2)*2':y='trunc(H*${MASK_Y}/2)*2':format=auto[outro];"
FILTER+="[body][outro]concat=n=2:v=1:a=0[v]"

run_ffmpeg() {
  ffmpeg \
    -y \
    -i "$INPUT" \
    -filter_complex "$FILTER" \
    -map "[v]" \
    -map "0:a?" \
    "$@" \
    -c:a copy \
    "$OUTPUT"
}

if ! run_ffmpeg "${VIDEO_CODEC_ARGS[@]}"; then
  echo "Primary encoder failed. Retrying with libx264 veryfast..." >&2
  run_ffmpeg -c:v libx264 -preset veryfast -pix_fmt yuv420p
fi

echo "Last-second transition video written: $OUTPUT"
