#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
LOG_DIR="$PROJECT_ROOT/logs/stageB"

mkdir -p "$LOG_DIR"
mkdir -p "$BUILD_DIR"

echo "=== Build once ==="
cd "$BUILD_DIR"
cmake ..
make -j"$(nproc)"

cd "$PROJECT_ROOT"

REPLAY_MULTIPLIER="${STAGEB_REPLAY_MULTIPLIER:-1}"
SEED="${STAGEB_SHUFFLE_SEED:-20260427}"

echo "=== Stage B balanced placement cycling ==="
echo "Replay multiplier: $REPLAY_MULTIPLIER"
echo "Shuffle seed:      $SEED"

python3 "$PROJECT_ROOT/stageB_balanced_cycle.py" \
    --replay-multiplier "$REPLAY_MULTIPLIER" \
    --seed "$SEED" \
    "$@"
