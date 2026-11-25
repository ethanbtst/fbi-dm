#!/usr/bin/env bash

set -euo pipefail

usage() {
    printf 'Usage: %s [OUTPUT_DIR] [INTERVAL_SECONDS]\n' "${0##*/}"
    printf 'Capture framebuffer PNGs with fbgrab until interrupted.\n'
    printf 'Defaults: a new timestamped folder beside this script, 1 second between captures.\n'
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
interval="${2:-1}"

if [[ ! $interval =~ ^[0-9]+([.][0-9]+)?$ ]] ||
    ! awk -v interval="$interval" 'BEGIN { exit !(interval > 0) }'; then
    printf 'Error: interval must be a positive number of seconds: %s\n' "$interval" >&2
    exit 2
fi

if ! command -v fbgrab >/dev/null 2>&1; then
    printf 'Error: fbgrab was not found in PATH.\n' >&2
    exit 127
fi

mkdir -p -- "$output_dir"

if compgen -G "$output_dir/frame_[0-9][0-9][0-9][0-9][0-9][0-9].png" >/dev/null; then
    printf 'Error: numbered frame PNGs already exist in %s; choose an empty output directory.\n' "$output_dir" >&2
    exit 1
fi

printf 'Capturing frames to %s with a %s-second delay between captures. Press Ctrl-C to stop.\n' "$output_dir" "$interval"

frame=0
trap 'exit 0' INT TERM

while true; do
    printf -v filename 'frame_%06d.png' "$frame"
    fbgrab "$output_dir/$filename"
    ((frame += 1))
    sleep "$interval"
done