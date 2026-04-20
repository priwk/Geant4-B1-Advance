#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
INPUT_DIR="$PROJECT_ROOT/Input"
LOG_DIR="$PROJECT_ROOT/logs"
MAC_FILE="$PROJECT_ROOT/Run.mac"

mkdir -p "$LOG_DIR"

echo "=== Build once ==="
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake ..
make -j"$(nproc)"

echo "=== Batch processing all capture CSV files ==="

shopt -s nullglob
files=("$INPUT_DIR"/*_neutron_capture_positions.csv)

if [ ${#files[@]} -eq 0 ]; then
    echo "No input files found in: $INPUT_DIR"
    exit 1
fi

for csv in "${files[@]}"; do
    base="$(basename "$csv" _neutron_capture_positions.csv)"
    echo
    echo ">>> Processing: $base"

    export BNZS_INPUT_CSV="$csv"

    ./B1 "$MAC_FILE" > "$LOG_DIR/${base}.log" 2>&1

    echo ">>> Done: $base"
done

echo
echo "=== All files processed ==="
echo "Logs:   $LOG_DIR"
echo "Output: $BUILD_DIR/Output"