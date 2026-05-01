#!/usr/bin/env python3
import argparse
import subprocess
import sys
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Run the Stage C pipeline for multiple thicknesses: split source "
            "generation, per-placement optical RVE runs, and macro coupling."
        )
    )
    parser.add_argument("--ratio-tag", required=True, help="Ratio folder, e.g. 1-1.5")
    parser.add_argument(
        "--thicknesses",
        required=True,
        help="Comma/space separated thickness labels, e.g. '30,50,100,200,500,700,1000'.",
    )
    parser.add_argument(
        "--project-root",
        default=None,
        help="Project root. Defaults to the directory containing this script.",
    )
    parser.add_argument(
        "--max-placements",
        type=int,
        default=0,
        help="Per thickness: run first N placements. Use 0 for all.",
    )
    parser.add_argument(
        "--samples-per-step",
        type=int,
        default=1,
        help="Stage C sampled optical photons per ZnS step.",
    )
    parser.add_argument(
        "--l-att-um",
        type=float,
        required=True,
        help="Macro attenuation length in um.",
    )
    parser.add_argument(
        "--optical-params",
        default="1.5 1000000 2.1 10 2.36 50",
        help="Six values: matrix_n matrix_abs_um bn_n bn_abs_um zns_n zns_abs_um.",
    )
    parser.add_argument(
        "--skip-source",
        action="store_true",
        help="Skip stageC_make_zns_sources.py if split source files already exist.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print commands without running them.",
    )
    parser.add_argument(
        "--bootstrap",
        type=int,
        default=0,
        help="Bootstrap iterations for final macro coupling curves.",
    )
    parser.add_argument(
        "--no-angle",
        action="store_true",
        help="Only write depth-only macro coupling curve.",
    )
    return parser.parse_args()


def parse_thicknesses(value):
    raw = value.replace(",", " ").split()
    cleaned = []
    seen = set()
    for item in raw:
        label = item.strip()
        if not label:
            continue
        try:
            float(label)
        except ValueError as exc:
            raise SystemExit(f"Invalid thickness label: {label}") from exc
        if label not in seen:
            cleaned.append(label)
            seen.add(label)
    if not cleaned:
        raise SystemExit("No thicknesses were provided.")
    return cleaned


def run_cmd(cmd, cwd, dry_run):
    print("  " + " ".join(str(x) for x in cmd))
    if not dry_run:
        subprocess.run([str(x) for x in cmd], cwd=str(cwd), check=True)


def main():
    args = parse_args()
    if args.max_placements < 0:
        raise SystemExit("--max-placements must be >= 0")
    if args.samples_per_step <= 0:
        raise SystemExit("--samples-per-step must be > 0")
    if args.l_att_um <= 0.0:
        raise SystemExit("--l-att-um must be > 0")

    project_root = Path(args.project_root).resolve() if args.project_root else Path(__file__).resolve().parent
    thicknesses = parse_thicknesses(args.thicknesses)
    python = sys.executable

    print(f"Project root:      {project_root}")
    print(f"Ratio:             {args.ratio_tag}")
    print(f"Thicknesses:       {' '.join(thicknesses)}")
    print(f"Max placements:    {args.max_placements if args.max_placements else 'all'}")
    print(f"Samples per step:  {args.samples_per_step}")
    print(f"L_att_um:          {args.l_att_um:g}")
    print(f"Dry run:           {'yes' if args.dry_run else 'no'}")

    for thickness in thicknesses:
        print(f"\n=== thickness {thickness} um ===")

        if not args.skip_source:
            run_cmd(
                [
                    python,
                    project_root / "stageC_make_zns_sources.py",
                    "--ratio-tag",
                    args.ratio_tag,
                    "--thickness-range",
                    f"{thickness}-{thickness}",
                    "--split-by-placement",
                ],
                project_root,
                args.dry_run,
            )

        run_cmd(
            [
                python,
                project_root / "stageC_run_batch.py",
                "--ratio-tag",
                args.ratio_tag,
                "--thickness",
                thickness,
                "--max-placements",
                str(args.max_placements),
                "--samples-per-step",
                str(args.samples_per_step),
                "--optical-params",
                args.optical_params,
                "--run",
            ],
            project_root,
            args.dry_run,
        )

    print("\n=== macro coupling: depth-only ===")
    depth_cmd = [
        python,
        project_root / "stageC_macro_coupling.py",
        "--ratio-tag",
        args.ratio_tag,
        "--l-att-um",
        str(args.l_att_um),
        "--macro-model",
        "depth-only",
    ]
    if args.bootstrap > 0:
        depth_cmd.extend(["--bootstrap", str(args.bootstrap)])
    run_cmd(depth_cmd, project_root, args.dry_run)

    if not args.no_angle:
        print("\n=== macro coupling: angle-resolved ===")
        angle_output = project_root / "Output" / "stageC" / args.ratio_tag / "thickness_light_yield_curve_angle.csv"
        angle_cmd = [
            python,
            project_root / "stageC_macro_coupling.py",
            "--ratio-tag",
            args.ratio_tag,
            "--l-att-um",
            str(args.l_att_um),
            "--macro-model",
            "angle-resolved",
            "--output",
            angle_output,
        ]
        if args.bootstrap > 0:
            angle_cmd.extend(["--bootstrap", str(args.bootstrap)])
        run_cmd(angle_cmd, project_root, args.dry_run)


if __name__ == "__main__":
    main()
