#!/usr/bin/env python3
import argparse
import random
import subprocess
import sys
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Generate and optionally run StageD_OpticalHomogenization macros "
            "for placement CSV files under one BN/ZnS ratio."
        )
    )
    parser.add_argument(
        "--ratio-tag",
        required=True,
        help="Ratio folder under Input/placements, e.g. 1-2 or 2-1.",
    )
    parser.add_argument(
        "--project-root",
        default=None,
        help="Project root. Defaults to the directory containing this script.",
    )
    parser.add_argument(
        "--build-dir",
        default=None,
        help="Build directory containing Geant4-MicroLight-BNZS. Defaults to <project-root>/build.",
    )
    parser.add_argument(
        "--placement-dir",
        default=None,
        help="Placement directory. Defaults to Input/placements/<ratio-tag>.",
    )
    parser.add_argument(
        "--placements",
        nargs="*",
        default=None,
        help=(
            "Specific placement basenames or stems to run, e.g. "
            "placement_f_0.64_0004.csv placement_f_0.64_0008. "
            "If omitted, all placements in placement-dir are candidates."
        ),
    )
    parser.add_argument(
        "--max-placements",
        type=int,
        default=0,
        help="Run only the first N selected placements. Use 0 for all.",
    )
    parser.add_argument(
        "--start-index",
        type=int,
        default=0,
        help="Skip the first N sorted selected placements.",
    )
    parser.add_argument(
        "--shuffle",
        action="store_true",
        help="Shuffle placement order before applying start-index/max-placements.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=20260507,
        help="Shuffle seed when --shuffle is used.",
    )
    parser.add_argument(
        "--beam-on",
        type=int,
        default=10000,
        help="Geant4 /run/beamOn value for every placement.",
    )
    parser.add_argument(
        "--wavelength-nm",
        type=float,
        default=450.0,
        help="StageD photon wavelength in nm.",
    )
    parser.add_argument(
        "--source-mode",
        default="uniform_ZnS",
        choices=["uniform_ZnS", "uniform_all_phase", "from_zns_step_sources"],
        help="StageD source mode.",
    )
    parser.add_argument(
        "--boundary-mode",
        default="same_phase_reentry",
        choices=["escape", "same_phase_reentry"],
        help="StageD boundary mode.",
    )
    parser.add_argument(
        "--reentry-mode",
        default="same_phase_rho_over_R",
        choices=["same_phase_rho_over_R", "same_phase_random"],
        help="StageD same-phase re-entry mode.",
    )
    parser.add_argument(
        "--matrix-reentry-mode",
        default="random_matrix",
        choices=["random_matrix", "distance_matched_matrix"],
        help="StageD matrix re-entry mode.",
    )
    parser.add_argument(
        "--theta-threshold-deg",
        type=float,
        default=1.0,
        help="StageD effective scatter angle threshold.",
    )
    parser.add_argument(
        "--max-reentry",
        type=int,
        default=10000,
        help="StageD maximum re-entry count per photon.",
    )
    parser.add_argument(
        "--max-steps",
        type=int,
        default=100000,
        help="StageD maximum Geant4 step count per photon.",
    )
    parser.add_argument(
        "--max-path-length-um",
        type=float,
        default=1.0e6,
        help="StageD maximum physical path length per photon in um.",
    )
    parser.add_argument(
        "--macro-dir",
        default=None,
        help="Directory for generated macros. Defaults to Output/stageD_macros/<ratio-tag>.",
    )
    parser.add_argument(
        "--log-dir",
        default=None,
        help="Directory for run logs. Defaults to logs/stageD/<ratio-tag>.",
    )
    parser.add_argument(
        "--run",
        action="store_true",
        help="Actually run Geant4-MicroLight-BNZS for each generated macro.",
    )
    parser.add_argument(
        "--executable-name",
        default="Geant4-MicroLight-BNZS",
        help="Executable name inside build-dir. Default: Geant4-MicroLight-BNZS.",
    )
    return parser.parse_args()


def as_posix(path: Path) -> str:
    return path.as_posix()


def rel_to_project(path: Path, project_root: Path) -> str:
    return path.resolve().relative_to(project_root.resolve()).as_posix()


def ratio_parts(ratio_tag: str):
    if "-" not in ratio_tag:
        raise SystemExit(f"Invalid ratio tag: {ratio_tag}")
    lhs, rhs = ratio_tag.split("-", 1)
    try:
        float(lhs)
        float(rhs)
    except ValueError as exc:
        raise SystemExit(f"Invalid ratio tag: {ratio_tag}") from exc
    return lhs, rhs


def normalize_requested_placement(raw: str) -> str:
    return raw if raw.endswith(".csv") else raw + ".csv"


def resolve_placements(placement_dir: Path, requested):
    if not requested:
        return sorted(placement_dir.glob("*.csv"))

    resolved = []
    for item in requested:
        name = normalize_requested_placement(item)
        candidate = placement_dir / name
        if not candidate.is_file():
            raise SystemExit(f"Placement not found: {candidate}")
        resolved.append(candidate)
    return resolved


def macro_text(
    placement_rel_to_build: str,
    bn_wt: str,
    zns_wt: str,
    beam_on: int,
    wavelength_nm: float,
    source_mode: str,
    boundary_mode: str,
    reentry_mode: str,
    matrix_reentry_mode: str,
    theta_threshold_deg: float,
    max_reentry: int,
    max_steps: int,
    max_path_length_um: float,
):
    return "\n".join(
        [
            "/run/verbose 0",
            "/event/verbose 0",
            "/tracking/verbose 0",
            "",
            "/cfg/setRunMode StageD_OpticalHomogenization",
            f"/cfg/setWeightRatio {bn_wt} {zns_wt}",
            f"/cfg/setPlacementFile {placement_rel_to_build}",
            f"/cfg/stageD/setWavelengthNm {wavelength_nm}",
            f"/cfg/stageD/setSourceMode {source_mode}",
            f"/cfg/stageD/setBoundaryMode {boundary_mode}",
            f"/cfg/stageD/setReentryMode {reentry_mode}",
            f"/cfg/stageD/setMatrixReentryMode {matrix_reentry_mode}",
            f"/cfg/stageD/setThetaThresholdDeg {theta_threshold_deg}",
            f"/cfg/stageD/setMaxReentry {max_reentry}",
            f"/cfg/stageD/setMaxSteps {max_steps}",
            f"/cfg/stageD/setMaxPathLengthUm {max_path_length_um}",
            "",
            "/run/initialize",
            f"/run/beamOn {beam_on}",
            "",
        ]
    )


def main():
    args = parse_args()
    if args.max_placements < 0 or args.start_index < 0:
        raise SystemExit("--max-placements and --start-index must be >= 0")
    if args.beam_on <= 0:
        raise SystemExit("--beam-on must be > 0")
    if args.wavelength_nm <= 0.0:
        raise SystemExit("--wavelength-nm must be > 0")
    if args.theta_threshold_deg < 0.0:
        raise SystemExit("--theta-threshold-deg must be >= 0")
    if args.max_reentry <= 0 or args.max_steps <= 0 or args.max_path_length_um <= 0.0:
        raise SystemExit("--max-reentry, --max-steps, and --max-path-length-um must be > 0")

    project_root = (
        Path(args.project_root).resolve()
        if args.project_root
        else Path(__file__).resolve().parent
    )
    build_dir = (
        Path(args.build_dir).resolve()
        if args.build_dir
        else project_root / "build"
    )
    placement_dir = (
        Path(args.placement_dir).resolve()
        if args.placement_dir
        else project_root / "Input" / "placements" / args.ratio_tag
    )
    macro_dir = (
        Path(args.macro_dir).resolve()
        if args.macro_dir
        else project_root / "Output" / "stageD_macros" / args.ratio_tag
    )
    log_dir = (
        Path(args.log_dir).resolve()
        if args.log_dir
        else project_root / "logs" / "stageD" / args.ratio_tag
    )
    executable = build_dir / args.executable_name

    if not build_dir.is_dir():
        raise SystemExit(f"Build directory not found: {build_dir}")
    if not placement_dir.is_dir():
        raise SystemExit(f"Placement directory not found: {placement_dir}")
    if args.run and not executable.is_file():
        raise SystemExit(f"Executable not found: {executable}")

    bn_wt, zns_wt = ratio_parts(args.ratio_tag)
    placements = resolve_placements(placement_dir, args.placements)

    if args.shuffle:
        rng = random.Random(args.seed)
        placements = list(placements)
        rng.shuffle(placements)
    else:
        placements = sorted(placements)

    placements = placements[args.start_index :]
    if args.max_placements > 0:
        placements = placements[: args.max_placements]
    if not placements:
        raise SystemExit("No placements selected.")

    macro_dir.mkdir(parents=True, exist_ok=True)
    log_dir.mkdir(parents=True, exist_ok=True)

    print(f"Project root: {project_root}")
    print(f"Build dir:    {build_dir}")
    print(f"Ratio tag:    {args.ratio_tag}")
    print(f"Placement dir:{placement_dir}")
    print(f"Placements:   {len(placements)}")
    print(f"Run enabled:  {'yes' if args.run else 'no'}")

    for idx, placement in enumerate(placements, start=1):
        placement_rel_to_build = "../" + rel_to_project(placement, project_root)
        placement_stem = placement.stem
        macro_path = macro_dir / f"{placement_stem}_StageD_OpticalHomogenization.mac"
        macro_rel_to_build = "../" + rel_to_project(macro_path, project_root)
        log_path = log_dir / f"p{idx:04d}_{placement_stem}.log"

        macro_path.write_text(
            macro_text(
                placement_rel_to_build=placement_rel_to_build,
                bn_wt=bn_wt,
                zns_wt=zns_wt,
                beam_on=args.beam_on,
                wavelength_nm=args.wavelength_nm,
                source_mode=args.source_mode,
                boundary_mode=args.boundary_mode,
                reentry_mode=args.reentry_mode,
                matrix_reentry_mode=args.matrix_reentry_mode,
                theta_threshold_deg=args.theta_threshold_deg,
                max_reentry=args.max_reentry,
                max_steps=args.max_steps,
                max_path_length_um=args.max_path_length_um,
            ),
            encoding="utf-8",
        )

        print(f"[{idx}/{len(placements)}] {placement.name}")
        print(f"  macro: {macro_path}")
        print(f"  log:   {log_path}")
        print(f"  cmd:   cd {build_dir} && ./{args.executable_name} {macro_rel_to_build}")

        if args.run:
            with log_path.open("w", encoding="utf-8") as log_file:
                subprocess.run(
                    [str(executable), macro_rel_to_build],
                    cwd=str(build_dir),
                    stdout=log_file,
                    stderr=subprocess.STDOUT,
                    check=True,
                )

    if not args.run:
        print("Macros generated. Re-run with --run to execute them.")


if __name__ == "__main__":
    main()
