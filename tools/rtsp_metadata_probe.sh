#!/usr/bin/env bash
# Inspect an RTSP session for ONVIF/WiseAI metadata without printing credentials.
# Usage: rtsp_metadata_probe.sh '<rtsp-url>' [seconds] [output-directory]
set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
    echo "usage: $0 '<rtsp-url>' [seconds] [output-directory]" >&2
    exit 64
fi

url=$1
seconds=${2:-8}
out_dir=${3:-"rtsp_metadata_probe_$(date +%Y%m%d_%H%M%S)"}

if ! [[ "$seconds" =~ ^[1-9][0-9]*$ ]]; then
    echo "seconds must be a positive integer" >&2
    exit 64
fi
if ! command -v ffprobe >/dev/null 2>&1; then
    echo "ffprobe is required (install FFmpeg)" >&2
    exit 69
fi

mkdir -p "$out_dir"

# Keep the report useful without copying a password into the terminal or report.
safe_url=$(printf '%s' "$url" | sed -E 's#(rtsp://[^:/@]+:)[^@]*@#\1***@#')
{
    printf 'target=%s\n' "$safe_url"
    printf 'duration_s=%s\n' "$seconds"
    printf 'created=%s\n' "$(date -Is)"
} > "$out_dir/README.txt"

common=(ffprobe -v error -rtsp_transport tcp -rw_timeout 8000000)

sanitize_stderr() {
    local raw=$1
    local clean=$2
    if [[ -f "$raw" ]]; then
        sed -E 's#(rtsp://[^:/@]+:)[^@[:space:]]*@#\1***@#g' "$raw" > "$clean"
        rm -f "$raw"
    fi
}

stream_err="$out_dir/.ffprobe_streams.stderr"
if ! "${common[@]}" -show_streams -of json "$url" \
        > "$out_dir/streams.json" 2> "$stream_err"; then
    sanitize_stderr "$stream_err" "$out_dir/ffprobe_streams.log"
    echo "RTSP stream open failed; see $out_dir/README.txt" >&2
    exit 1
fi
sanitize_stderr "$stream_err" "$out_dir/ffprobe_streams.log"

echo "streams.json written: $out_dir/streams.json"

# Data and subtitle tracks are the normal FFmpeg labels for ONVIF metadata.  Keep
# one packet sample for each label; an empty file is meaningful (no such track).
for selector in d s; do
    output="$out_dir/packets_${selector}.json"
    packet_err="$out_dir/.ffprobe_${selector}.stderr"
    set +e
    timeout "$((seconds + 3))" "${common[@]}" \
        -select_streams "$selector" \
        -show_packets \
        -show_entries packet=stream_index,codec_type,pts_time,dts_time,size,data \
        -show_data \
        -read_intervals "%+${seconds}" \
        -of json "$url" > "$output" 2> "$packet_err"
    rc=$?
    set -e
    sanitize_stderr "$packet_err" "$out_dir/ffprobe_${selector}.log"
    if [[ $rc -ne 0 && $rc -ne 124 ]]; then
        printf 'selector=%s ffprobe_rc=%s\n' "$selector" "$rc" >> "$out_dir/README.txt"
    fi
    echo "packets_${selector}.json written: $output"
done

cat <<EOF
Probe complete: $out_dir
Next checks:
  1. Inspect streams.json for codec_type=data or subtitle and metadata/onvif labels.
  2. Inspect packets_d.json / packets_s.json for XML or JSON payload bytes.
  3. Remove credentials before sharing any packet sample.
EOF
