#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
PLACEMENT_ROOT="$PROJECT_ROOT/Input/placements"
OUTPUT_ROOT="$PROJECT_ROOT/Output/stageA"
LOG_DIR="$PROJECT_ROOT/logs/stageA"
EVENTS="${STAGEA_EVENTS:-100000}"

mkdir -p "$BUILD_DIR" "$LOG_DIR"

echo "=== Build ==="
cd "$BUILD_DIR"
cmake ..
make -j"$(nproc)"

if [ "$#" -gt 0 ]; then
  ratios=("$@")
else
  shopt -s nullglob
  ratios=()
  for dir in "$PLACEMENT_ROOT"/*; do
    [ -d "$dir" ] && ratios+=("$(basename "$dir")")
  done
fi

if [ "${#ratios[@]}" -eq 0 ]; then
  echo "No placement ratio folders found in: $PLACEMENT_ROOT"
  exit 1
fi

for ratio in "${ratios[@]}"; do
  ratio_dir="$PLACEMENT_ROOT/$ratio"
  if [ ! -d "$ratio_dir" ]; then
    echo "Missing placement ratio folder: $ratio_dir"
    exit 1
  fi

  bn_wt="${ratio%%-*}"
  zns_wt="${ratio#*-}"

  shopt -s nullglob
  placements=("$ratio_dir"/*.csv "$ratio_dir"/*.txt)
  if [ "${#placements[@]}" -eq 0 ]; then
    echo "No placement files found in: $ratio_dir"
    continue
  fi

  summary="$OUTPUT_ROOT/$ratio/neutron_transport_summary.csv"
  mkdir -p "$(dirname "$summary")"
  rm -f "$summary"

  echo
  echo "=== Stage A ratio $ratio: ${#placements[@]} placement files, $EVENTS events each ==="

  for placement in "${placements[@]}"; do
    base="$(basename "$placement")"
    macro="$BUILD_DIR/stageA_batch_current.mac"
    log="$LOG_DIR/${ratio}_${base%.*}.log"

    cat > "$macro" <<EOF
/control/verbose 2
/run/verbose 1
/event/verbose 0
/tracking/verbose 0

/cfg/setRunMode StageA_NeutronPatch
/cfg/setWeightRatio $bn_wt $zns_wt
/cfg/setPlacementFile $placement

/run/initialize
/run/beamOn $EVENTS
EOF

    echo ">>> $ratio / $base"
    BNZS_RUN_MODE=StageA_NeutronPatch \
      BNZS_PLACEMENT_FILE="$placement" \
      BNZS_USE_RANDOM_PLACEMENT=0 \
      ./B1 "$macro" > "$log" 2>&1
  done

  echo "Summary: $summary"
done

echo
echo "=== Stage A placement batch done ==="
