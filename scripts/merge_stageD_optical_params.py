#!/usr/bin/env python3

import argparse
import csv
import math
from pathlib import Path


def mean(values):
    return sum(values) / len(values) if values else 0.0


def stddev(values):
    if len(values) < 2:
      return 0.0
    mu = mean(values)
    return math.sqrt(sum((v - mu) ** 2 for v in values) / (len(values) - 1))


def load_rows(base_dir: Path, ratio: str):
    root = base_dir / "Output" / "stageD_optical_homogenization" / ratio
    rows = []
    for path in sorted(root.glob("*/optical_homogenization_summary.csv")):
        with path.open(newline="") as f:
            reader = csv.DictReader(f)
            for row in reader:
                row["_path"] = str(path)
                rows.append(row)
    return rows


def main():
    parser = argparse.ArgumentParser(
        description="Merge StageD raw optical parameters over placements for one ratio."
    )
    parser.add_argument("--ratio", required=True, help="Ratio tag such as 1-2 or 2-1.")
    parser.add_argument(
        "--project-root",
        default=".",
        help="Project root containing Output/.",
    )
    args = parser.parse_args()

    base_dir = Path(args.project_root).resolve()
    rows = load_rows(base_dir, args.ratio)
    if not rows:
        raise SystemExit(f"No StageD summary CSV files found for ratio {args.ratio}")

    mu_a = [float(r["mu_a_raw_per_um"]) for r in rows]
    mu_s = [float(r["mu_s_raw_per_um"]) for r in rows]
    g_values = [float(r["g_raw"]) for r in rows]
    mu_sp = [float(r["mu_s_prime_raw_per_um"]) for r in rows]
    absorbed = [float(r["absorbed_fraction"]) for r in rows]
    path_length = [float(r["mean_path_length_um"]) for r in rows]
    reentry = [float(r["mean_num_reentry"]) for r in rows]
    photons = [int(float(r["n_photons"])) for r in rows]
    wavelengths = [float(r["wavelength_nm"]) for r in rows]

    out_dir = base_dir / "Output" / "optical_params" / args.ratio
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "rve_raw_optical_params_by_ratio.csv"

    with out_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "ratio",
                "wavelength_nm",
                "n_placements",
                "n_photons_total",
                "mu_a_mean_per_um",
                "mu_a_std_per_um",
                "mu_s_mean_per_um",
                "mu_s_std_per_um",
                "g_mean",
                "g_std",
                "mu_s_prime_mean_per_um",
                "mu_s_prime_std_per_um",
                "mean_absorbed_fraction",
                "mean_path_length_um",
                "mean_reentry_count",
                "notes",
            ]
        )
        writer.writerow(
            [
                args.ratio,
                mean(wavelengths),
                len(rows),
                sum(photons),
                mean(mu_a),
                stddev(mu_a),
                mean(mu_s),
                stddev(mu_s),
                mean(g_values),
                stddev(g_values),
                mean(mu_sp),
                stddev(mu_sp),
                mean(absorbed),
                mean(path_length),
                mean(reentry),
                "Ensemble average over StageD placements; one Geant4 run equals one placement.",
            ]
        )

    print(out_path)


if __name__ == "__main__":
    main()
