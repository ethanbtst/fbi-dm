#!/usr/bin/env bash

set -euo pipefail

usage() {
    printf 'Usage: %s [OUTPUT_DIR] [FPS]\n' "${0##*/}"
    printf 'Capture uncompressed framebuffer video until interrupted, then convert it to MP4.\n'
    printf 'Defaults: a new timestamped folder beside this script, 60 FPS.\n'
}

if [[ ${1:-} == "-h" || ${1:-} == "--help" ]]; then
    usage
    exit 0
fi

if (( $# > 2 )); then
    usage >&2
    exit 2
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
output_dir="${1:-"$script_dir/frames-$(date +%Y%m%d-%H%M%S)"}"
fps="${2:-60}"

if [[ ! $fps =~ ^[0-9]+([.][0-9]+)?$ ]] ||
    ! awk -v fps="$fps" 'BEGIN { exit !(fps > 0) }'; then
    printf 'Error: FPS must be a positive number: %s\n' "$fps" >&2
    exit 2
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
    printf 'Error: ffmpeg was not found in PATH.\n' >&2
    exit 127
fi

mkdir -p -- "$output_dir"
raw_file="$output_dir/capture.nut"
video_file="$output_dir/capture.mp4"

if [[ -e $raw_file || -e $video_file ]]; then
    printf 'Error: capture.nut or capture.mp4 already exists in %s; choose another output directory.\n' "$output_dir" >&2
    exit 1
fi

printf 'Capturing /dev/fb0 at %s FPS to %s\n' "$fps" "$raw_file"
printf 'Press Ctrl-C to stop capture and convert the recording to MP4.\n'

interrupted=0
capture_pid=
stop_capture() {
    interrupted=1
    if [[ -n $capture_pid ]]; then
        kill -INT "$capture_pid" 2>/dev/null || true
    fi
}
trap stop_capture INT TERM

set +e
ffmpeg -hide_banner -f fbdev -framerate "$fps" -i /dev/fb0 \
    -map 0:v:0 -c:v copy -f nut "$raw_file" &
capture_pid=$!
while true; do
    wait "$capture_pid"
    capture_status=$?
    if ! kill -0 "$capture_pid" 2>/dev/null; then
        break
    fi
done
capture_pid=
set -e

if (( capture_status != 0 && interrupted == 0 )); then
    printf 'Error: framebuffer capture failed (ffmpeg exit status %d).\n' "$capture_status" >&2
    exit "$capture_status"
fi

if [[ ! -s $raw_file ]]; then
    printf 'Error: capture produced no video data; skipping conversion.\n' >&2
    exit 1
fi

printf 'Converting capture to %s\n' "$video_file"
ffmpeg -hide_banner -n -i "$raw_file" -an \
    -c:v libx264 -preset veryfast -crf 18 -pix_fmt yuv420p "$video_file"
printf 'Done. Raw capture retained at %s\n' "$raw_file"
