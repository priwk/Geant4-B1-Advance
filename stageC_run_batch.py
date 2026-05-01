#!/usr/bin/env python3
import argparse
import csv
import subprocess
import sys
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Generate and optionally run StageC_OpticalRVE macros for split "
            "single-placement ZnS source CSV files."
        )
    )
    parser.add_argument("--ratio-tag", required=True, help="Ratio folder, e.g. 1-1.5")
    parser.add_argument("--thickness", required=True, help="Thickness label, e.g. 500")
    parser.add_argument(
        "--project-root",
        default=None,
        help="Project root. Defaults to the directory containing this script.",
    )
    parser.add_argument(
        "--build-dir",
        default=None,
        help="Build directory containing B1. Defaults to <project-root>/build.",
    )
    parser.add_argument(
        "--source-dir",
        default=None,
        help=(
            "Split source directory. Defaults to "
            "Input/alpha_li_steps/<ratio>/by_placement/<thickness>."
        ),
    )
    parser.add_argument(
        "--max-placements",
        type=int,
        default=0,
        help="Run only the first N placement source files. Use 0 for all.",
    )
    parser.add_argument(
        "--start-index",
        type=int,
        default=0,
        help="Skip the first N sorted placement source files.",
    )
    parser.add_argument(
        "--samples-per-step",
        type=int,
        default=1,
        help="Stage C sampled optical photons per ZnS step.",
    )
    parser.add_argument(
        "--beam-on",
        type=int,
        default=0,
        help=(
            "Geant4 /run/beamOn value for every source. Use 0 to count positive "
            "n_photon_step rows in each source CSV."
        ),
    )
    parser.add_argument(
        "--optical-params",
        default="1.5 1000000 2.1 10 2.36 50",
        help=(
            "Six values: matrix_n matrix_abs_um bn_n bn_abs_um zns_n zns_abs_um. "
            "Default is the current placeholder set."
        ),
    )
    parser.add_argument(
        "--macro-dir",
        default=None,
        help="Directory for generated macros. Defaults to Output/stageC_macros/<ratio>/<thickness>.",
    )
    parser.add_argument(
        "--run",
        action="store_true",
        help="Actually execute B1 for each generated macro. Default only prints commands.",
    )
    parser.add_argument(
        "--b1-name",
        default="B1",
        help="Executable name inside build-dir. Default: B1.",
    )
    parser.add_argument(
        "--run-macro-coupling",
        action="store_true",
        help="Run stageC_macro_coupling.py after all selected placements finish.",
    )
    parser.add_argument(
        "--l-att-um",
        type=float,
        default=None,
        help="L_att for optional macro coupling.",
    )
    parser.add_argument(
        "--macro-model",
        choices=["depth-only", "angle-resolved"],
        default="depth-only",
        help="Macro coupling model if --run-macro-coupling is used.",
    )
    return parser.parse_args()


def as_g4_path(path):
    return Path(path).as_posix()


def relpath(path, start):
    return as_g4_path(Path(path).resolve().relative_to(Path(start).resolve()))


def count_positive_sources(source_csv):
    count = 0
    with Path(source_csv).open(newline="", encoding="utf-8-sig") as f:
        for row in csv.DictReader(f):
            try:
                if float(row.get("n_photon_step", "0")) > 0.0:
                    count += 1
            except ValueError:
                pass
    return count


def macro_text(source_rel_to_build, samples_per_step, beam_on, optical_params):
    return "\n".join(
        [
            "/control/verbose 1",
            "/run/verbose 1",
            "/event/verbose 0",
            "/tracking/verbose 0",
            "",
            "/cfg/setRunMode StageC_OpticalRVE",
            f"/cfg/setOpticalSourceCsv {source_rel_to_build}",
            "/cfg/setSourceSampling uniformAlongStep",
            f"/cfg/setSamplePhotonsPerStep {samples_per_step}",
            f"/cfg/setOpticalParams {optical_params}",
            "",
            "/run/initialize",
            f"/run/beamOn {beam_on}",
            "",
        ]
    )


def main():
    args = parse_args()
    if args.samples_per_step <= 0:
        raise SystemExit("--samples-per-step must be > 0")
    if args.beam_on < 0:
        raise SystemExit("--beam-on must be >= 0")
    if args.max_placements < 0 or args.start_index < 0:
        raise SystemExit("--max-placements and --start-index must be >= 0")

    project_root = Path(args.project_root).resolve() if args.project_root else Path(__file__).resolve().parent
    build_dir = Path(args.build_dir).resolve() if args.build_dir else project_root / "build"
    source_dir = (
        Path(args.source_dir).resolve()
        if args.source_dir
        else project_root / "Input" / "alpha_li_steps" / args.ratio_tag / "by_placement" / args.thickness
    )
    macro_dir = (
        Path(args.macro_dir).resolve()
        if args.macro_dir
        else project_root / "Output" / "stageC_macros" / args.ratio_tag / args.thickness
    )

    if not source_dir.is_dir():
        raise SystemExit(f"Source directory not found: {source_dir}")
    if not build_dir.is_dir():
        raise SystemExit(f"Build directory not found: {build_dir}")

    sources = sorted(source_dir.glob(f"{args.thickness}_*_zns_step_sources.csv"))
    sources = sources[args.start_index :]
    if args.max_placements > 0:
        sources = sources[: args.max_placements]
    if not sources:
        raise SystemExit(f"No split source files found in {source_dir}")

    macro_dir.mkdir(parents=True, exist_ok=True)
    b1_path = build_dir / args.b1_name

    print(f"Project root:  {project_root}")
    print(f"Build dir:     {build_dir}")
    print(f"Source dir:    {source_dir}")
    print(f"Macro dir:     {macro_dir}")
    print(f"Placements:    {len(sources)}")
    print(f"Run enabled:   {'yes' if args.run else 'no'}")

    for index, source in enumerate(sources, start=1):
        beam_on = args.beam_on if args.beam_on > 0 else count_positive_sources(source) * args.samples_per_step
        source_rel = "../" + relpath(source, project_root)
        macro_path = macro_dir / source.name.replace("_zns_step_sources.csv", "_StageC_OpticalRVE.mac")
        macro_rel_to_build = "../" + relpath(macro_path, project_root)
        macro_path.write_text(
            macro_text(source_rel, args.samples_per_step, beam_on, args.optical_params),
            encoding="utf-8",
        )

        print(f"[{index}/{len(sources)}] {source.name}: beamOn={beam_on}")
        print(f"  macro: {macro_path}")
        print(f"  cmd:   cd {build_dir} && ./{args.b1_name} {macro_rel_to_build}")

        if args.run:
            cmd = [str(b1_path), macro_rel_to_build]
            subprocess.run(cmd, cwd=str(build_dir), check=True)

    if args.run_macro_coupling:
        if args.l_att_um is None or args.l_att_um <= 0.0:
            raise SystemExit("--run-macro-coupling requires --l-att-um > 0")
        cmd = [
            sys.executable,
            str(project_root / "stageC_macro_coupling.py"),
            "--ratio-tag",
            args.ratio_tag,
            "--l-att-um",
            str(args.l_att_um),
            "--macro-model",
            args.macro_model,
        ]
        print("Macro coupling:")
        print("  " + " ".join(cmd))
        if args.run:
            subprocess.run(cmd, cwd=str(project_root), check=True)


if __name__ == "__main__":
    main()
