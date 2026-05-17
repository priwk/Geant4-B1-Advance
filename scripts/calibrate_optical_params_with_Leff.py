#!/usr/bin/env python3

import argparse
import csv
import math
from pathlib import Path


def read_csv_rows(path: Path):
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def main():
    parser = argparse.ArgumentParser(
        description="Calibrate StageD raw optical parameters against experimental effective attenuation length."
    )
    parser.add_argument("--ratio", required=True, help="Ratio tag such as 1-2 or 2-1.")
    parser.add_argument(
        "--project-root",
        default=".",
        help="Project root containing Output/.",
    )
    parser.add_argument(
        "--experimental-leff",
        required=True,
        help="Path to experimental_Leff.csv",
    )
    args = parser.parse_args()

    base_dir = Path(args.project_root).resolve()
    raw_path = base_dir / "Output" / "optical_params" / args.ratio / "rve_raw_optical_params_by_ratio.csv"
    exp_path = Path(args.experimental_leff).resolve()

    raw_rows = read_csv_rows(raw_path)
    exp_rows = read_csv_rows(exp_path)
    if not raw_rows:
        raise SystemExit(f"No raw optical params found in {raw_path}")

    exp_by_key = {
        (row["ratio"], row["wavelength_nm"]): row
        for row in exp_rows
    }

    out_dir = base_dir / "Output" / "optical_params" / args.ratio
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "calibrated_optical_params.csv"

    with out_path.open("w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "ratio",
                "wavelength_nm",
                "mu_a_raw_per_um",
                "mu_s_raw_per_um",
                "g_raw",
                "mu_s_prime_raw_per_um",
                "mu_a_calibrated_per_um",
                "mu_s_calibrated_per_um",
                "g_calibrated",
                "mu_s_prime_calibrated_per_um",
                "n_eff_initial",
                "L_eff_exp_um",
                "calibration_method",
                "warning",
            ]
        )

        for raw in raw_rows:
            key = (raw["ratio"], raw["wavelength_nm"])
            exp = exp_by_key.get(key)

            mu_a_raw = float(raw["mu_a_mean_per_um"])
            mu_s_raw = float(raw.get("mu_s_mean_per_um", raw.get("mu_s_total_mean_per_um", 0.0)))
            g_raw = float(raw.get("g_mean", raw.get("g_total_mean", 0.0)))
            mu_sp_raw = float(raw.get("mu_s_prime_mean_per_um", raw.get("mu_s_prime_total_mean_per_um", 0.0)))

            mu_a_cal = mu_a_raw
            mu_sp_cal = mu_sp_raw
            mu_s_cal = mu_s_raw
            g_cal = g_raw
            n_eff_initial = ""
            leff = ""
            warning = ""
            method = "no_experimental_leff"

            if exp is not None:
                method = "diffusion_initial_guess_fixed_g_fixed_mu_s_prime"
                leff = exp["L_eff_exp_um"]
                leff_val = float(leff)
                if mu_sp_raw >= 0.0 and leff_val > 0.0:
                    disc = mu_sp_raw * mu_sp_raw + 4.0 / (3.0 * leff_val * leff_val)
                    mu_a_guess = 0.5 * (-mu_sp_raw + math.sqrt(disc))
                    if mu_a_guess >= 0.0:
                        mu_a_cal = mu_a_guess
                        mu_sp_cal = mu_sp_raw
                        mu_s_cal = mu_sp_cal / max(1.0e-12, 1.0 - g_raw)
                    else:
                        warning = (
                            "mu_a_calibrated became negative under the fixed-g fixed-mu_s_prime initial guess."
                        )
                else:
                    warning = "Experimental L_eff or mu_s_prime_raw is invalid."

            writer.writerow(
                [
                    raw["ratio"],
                    raw["wavelength_nm"],
                    mu_a_raw,
                    mu_s_raw,
                    g_raw,
                    mu_sp_raw,
                    mu_a_cal,
                    mu_s_cal,
                    g_cal,
                    mu_sp_cal,
                    n_eff_initial,
                    leff,
                    method,
                    warning,
                ]
            )

    print(out_path)


if __name__ == "__main__":
    main()
