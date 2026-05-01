#!/usr/bin/env python3
import argparse
import csv
import hashlib
import math
import os
import re
import random
import shutil
import subprocess
import sys
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Balanced Stage B replay across placement files."
    )
    parser.add_argument("ratios", nargs="*", help="Ratio folders such as 1-2 or 1-3.")
    parser.add_argument("--replay-multiplier", type=int, default=1)
    parser.add_argument("--seed", type=int, default=20260427)
    parser.add_argument("--min-thickness-um", type=float, default=30.0)
    parser.add_argument("--project-root", default=None)
    parser.add_argument("--build-dir", default=None)
    parser.add_argument("--executable", default=None)
    parser.add_argument("--macro", default=None)
    parser.add_argument("--keep-chunks", action="store_true")
    parser.add_argument("--keep-part-outputs", action="store_true")
    parser.add_argument("--merge-only", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    return parser.parse_args()


def natural_float_tag(path):
    name = Path(path).name
    key = "_neutron_capture_positions.csv"
    if not name.endswith(key):
        return math.inf
    try:
        return float(name[: -len(key)])
    except ValueError:
        return math.inf


def stable_seed(base_seed, *parts):
    h = hashlib.sha256()
    h.update(str(base_seed).encode("utf-8"))
    for part in parts:
        h.update(b"\0")
        h.update(str(part).encode("utf-8"))
    return int.from_bytes(h.digest()[:8], "big")


def read_capture_rows(csv_path, min_thickness_um):
    rows = []
    with open(csv_path, newline="") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if header is None:
            return [], []

        for row in reader:
            if not row:
                continue
            try:
                thickness = float(row[1])
            except (ValueError, IndexError):
                continue
            if thickness + 1.0e-12 < min_thickness_um:
                continue
            rows.append(row)
    return header, rows


def write_chunk(path, header, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)


def ratio_parts(ratio):
    if "-" not in ratio:
        raise ValueError(f"Invalid ratio folder name: {ratio}")
    lhs, rhs = ratio.split("-", 1)
    float(lhs)
    float(rhs)
    return lhs, rhs


def relative_to_dir(path, start_dir):
    path = Path(path)
    start_dir = Path(start_dir).resolve()
    candidate = path.resolve() if path.is_absolute() else (start_dir / path).resolve()
    try:
        return candidate.relative_to(start_dir).as_posix()
    except ValueError:
        return os.path.relpath(candidate, start_dir)


def run_one(build_dir, executable, macro, env, log_file, dry_run):
    log_file.parent.mkdir(parents=True, exist_ok=True)
    cmd = [str(executable), str(macro)]

    if dry_run:
        print("DRY RUN:", " ".join(cmd))
        print("  BNZS_INPUT_CSV=", env.get("BNZS_INPUT_CSV"))
        print("  BNZS_PLACEMENT_FILE=", env.get("BNZS_PLACEMENT_FILE"))
        print("  log=", log_file)
        return 0

    with open(log_file, "w") as log:
        return subprocess.call(cmd, cwd=build_dir, env=env, stdout=log, stderr=subprocess.STDOUT)


def merge_step_outputs(output_ratio_dir, source_tag, remove_parts):
    part_files = sorted(output_ratio_dir.glob(f"{source_tag}_m*_p*_alpha_li_steps.csv"))
    if not part_files:
        return 0, None

    merged_path = output_ratio_dir / f"{source_tag}_alpha_li_steps.csv"
    tmp_path = output_ratio_dir / f".{source_tag}_alpha_li_steps.tmp"

    total_rows = 0
    header_written = False

    with open(tmp_path, "w", newline="") as out:
        writer = None

        for part_file in part_files:
            with open(part_file, newline="") as f:
                reader = csv.reader(f)
                header = next(reader, None)
                if header is None:
                    continue

                if not header_written:
                    writer = csv.writer(out)
                    writer.writerow(header)
                    header_written = True

                for row in reader:
                    if not row:
                        continue
                    writer.writerow(row)
                    total_rows += 1

    if not header_written:
        tmp_path.unlink(missing_ok=True)
        return 0, None

    tmp_path.replace(merged_path)

    if remove_parts:
        for part_file in part_files:
            part_file.unlink(missing_ok=True)

    return total_rows, merged_path


def merge_existing_outputs(project_root, ratios, remove_parts, dry_run=False):
    output_root = project_root / "Output" / "stageB"
    if not ratios:
        ratios = sorted(p.name for p in output_root.iterdir() if p.is_dir()) if output_root.exists() else []

    pattern = re.compile(r"^(?P<tag>.+)_m\d+_p\d+_.+_alpha_li_steps\.csv$")
    merged_any = False

    for ratio in ratios:
        ratio_dir = output_root / ratio
        if not ratio_dir.is_dir():
            print(f">>> Skip {ratio}: output directory not found")
            continue

        source_tags = sorted(
            {
                match.group("tag")
                for path in ratio_dir.glob("*_alpha_li_steps.csv")
                for match in [pattern.match(path.name)]
                if match is not None
            }
        )

        for source_tag in source_tags:
            if dry_run:
                part_count = len(list(ratio_dir.glob(f"{source_tag}_m*_p*_alpha_li_steps.csv")))
                print(
                    f"DRY RUN: would merge {part_count} files for "
                    f"{ratio}/{source_tag} -> {source_tag}_alpha_li_steps.csv"
                )
                continue

            rows_merged, merged_path = merge_step_outputs(ratio_dir, source_tag, remove_parts)
            if merged_path is not None:
                merged_any = True
                print(
                    f">>> Merged {ratio} {source_tag}: "
                    f"rows={rows_merged} -> {merged_path.name}"
                )

    if not merged_any:
        print("No Stage B part outputs found to merge.")


def main():
    args = parse_args()
    if args.replay_multiplier <= 0:
        raise SystemExit("--replay-multiplier must be > 0")

    project_root = Path(args.project_root).resolve() if args.project_root else Path(__file__).resolve().parent
    build_dir = Path(args.build_dir).resolve() if args.build_dir else project_root / "build"
    executable = Path(args.executable).resolve() if args.executable else build_dir / "B1"
    macro = Path(args.macro).resolve() if args.macro else project_root / "run.mac"

    input_root = project_root / "Input" / "neutron_capture_positions"
    placement_root = project_root / "Input" / "placements"
    chunk_root = build_dir / "stageB_balanced_chunks"
    log_root = project_root / "logs" / "stageB" / "balanced"

    ratios = args.ratios
    if not ratios:
        ratios = sorted(p.name for p in input_root.iterdir() if p.is_dir() and "-" in p.name)

    if not ratios:
        raise SystemExit(f"No ratio directories found under {input_root}")

    if args.merge_only:
        merge_existing_outputs(
            project_root,
            ratios,
            remove_parts=not args.keep_part_outputs,
            dry_run=args.dry_run,
        )
        return

    if not args.dry_run and not executable.exists():
        raise SystemExit(f"Executable not found: {executable}")

    if not args.keep_chunks and chunk_root.exists():
        shutil.rmtree(chunk_root)

    total_runs = 0
    failed_runs = 0

    for ratio in ratios:
        bn_wt, zns_wt = ratio_parts(ratio)
        ratio_input_dir = input_root / ratio
        ratio_placement_dir = placement_root / ratio

        capture_files = sorted(
            ratio_input_dir.glob("*_neutron_capture_positions.csv"),
            key=lambda p: (natural_float_tag(p), p.name),
        )
        placement_files = sorted(ratio_placement_dir.glob("*.csv"))

        if not capture_files:
            print(f">>> Skip {ratio}: no capture CSV files in {ratio_input_dir}")
            continue
        if not placement_files:
            print(f">>> Skip {ratio}: no placement CSV files in {ratio_placement_dir}")
            continue

        print()
        print(f"=== Ratio {ratio}: {len(capture_files)} capture files, {len(placement_files)} placements ===")

        for capture_file in capture_files:
            header, rows = read_capture_rows(capture_file, args.min_thickness_um)
            if not rows:
                print(f">>> Skip {ratio}/{capture_file.name}: no compatible records")
                continue

            source_tag = capture_file.name.replace("_neutron_capture_positions.csv", "")

            shuffled = list(rows)
            rng = random.Random(stable_seed(args.seed, ratio, capture_file.name))
            rng.shuffle(shuffled)

            placements = list(placement_files)
            rng.shuffle(placements)

            for replay_idx in range(args.replay_multiplier):

                chunks = [[] for _ in placements]
                for idx, row in enumerate(shuffled):
                    # A replay round shifts each record to a different placement
                    # while preserving near-equal chunk sizes.
                    chunks[(idx + replay_idx) % len(placements)].append(row)

                for placement_idx, (placement_file, chunk_rows) in enumerate(zip(placements, chunks)):
                    if not chunk_rows:
                        continue

                    placement_tag = placement_file.stem
                    chunk_name = (
                        f"{source_tag}_m{replay_idx + 1:02d}_"
                        f"p{placement_idx + 1:04d}_{placement_tag}_"
                        "neutron_capture_positions.csv"
                    )
                    chunk_path = chunk_root / ratio / source_tag / f"m{replay_idx + 1:02d}" / chunk_name
                    write_chunk(chunk_path, header, chunk_rows)

                    log_file = (
                        log_root
                        / ratio
                        / source_tag
                        / f"m{replay_idx + 1:02d}"
                        / f"p{placement_idx + 1:04d}_{placement_tag}.log"
                    )

                    env = os.environ.copy()
                    env["BNZS_RUN_MODE"] = "StageB_ReplayAlphaLi"
                    env["BNZS_INPUT_CSV"] = relative_to_dir(chunk_path, build_dir)
                    env.pop("BNZS_INPUT_DIR", None)
                    env["BNZS_BN_WT"] = bn_wt
                    env["BNZS_ZNS_WT"] = zns_wt
                    env["BNZS_OUTPUT_RATIO"] = ratio
                    env["BNZS_PLACEMENT_FILE"] = relative_to_dir(placement_file, project_root)

                    total_runs += 1
                    print(
                        f">>> {ratio} {source_tag} m{replay_idx + 1:02d} "
                        f"placement {placement_idx + 1}/{len(placements)} "
                        f"events={len(chunk_rows)} placement={placement_tag}"
                    )

                    code = run_one(build_dir, executable, macro, env, log_file, args.dry_run)
                    if code != 0:
                        failed_runs += 1
                        print(f"!!! Failed code={code}: {log_file}")
                        raise SystemExit(code)

            output_ratio_dir = project_root / "Output" / "stageB" / ratio
            rows_merged, merged_path = merge_step_outputs(
                output_ratio_dir,
                source_tag,
                remove_parts=not args.keep_part_outputs,
            )
            if merged_path is not None:
                print(
                    f">>> Merged {ratio} {source_tag}: "
                    f"rows={rows_merged} -> {merged_path.name}"
                )

    print()
    print(f"=== Stage B balanced cycling complete: runs={total_runs}, failed={failed_runs} ===")
    print(f"Logs:   {log_root}")
    print(f"Output: {project_root / 'Output' / 'stageB'}")


if __name__ == "__main__":
    main()
