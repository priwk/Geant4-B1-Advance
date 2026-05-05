#!/usr/bin/env python3
import argparse
import csv
import math
import random
import re
from collections import defaultdict
from pathlib import Path


KERNEL_RE = re.compile(
    r"^(?P<stem>(?P<thickness>[+]?(?:\d+(?:\.\d*)?|\.\d+)).*)_local_optical_kernel_events\.csv$"
)


OUTPUT_FIELDS = [
    "ratio_label",
    "bn_wt",
    "zns_wt",
    "thickness_um",
    "P_capture",
    "n_captured_events",
    "n_total_incident_neutrons",
    "mean_visible_edep_ZnS_keV_per_capture",
    "mean_n_photon0_per_capture",
    "mean_local_front_escape_per_capture",
    "mean_local_back_escape_per_capture",
    "mean_local_side_escape_per_capture",
    "mean_macro_weighted_front_light_per_capture",
    "mean_macro_weighted_back_light_per_capture",
    "mean_macro_weighted_front_light_per_incident_neutron",
    "mean_macro_weighted_back_light_per_incident_neutron",
    "side_escape_fraction",
    "absorbed_fraction",
    "lost_fraction",
    "L_att_um",
    "mu_eff_per_um",
    "macro_model",
    "n_placements",
    "bootstrap_ci_low",
    "bootstrap_ci_high",
]


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Couple Stage C local optical kernels to a macro optical attenuation "
            "model and write a thickness light-yield curve."
        )
    )
    parser.add_argument("--ratio-tag", required=True, help="Ratio folder, e.g. 1-1.5")
    parser.add_argument(
        "--project-root",
        default=None,
        help="Project root. Defaults to the directory containing this script.",
    )
    parser.add_argument(
        "--stagec-dir",
        default=None,
        help="Stage C output directory. Defaults to Output/stageC/<ratio-tag>.",
    )
    parser.add_argument(
        "--event-source-dir",
        default=None,
        help="Stage C event source directory. Defaults to Input/alpha_li_steps/<ratio-tag>.",
    )
    parser.add_argument(
        "--stagea-summary",
        default=None,
        help=(
            "Macro-model neutron_transport_summary.csv. Defaults to "
            "Input/stageA/<ratio-tag>/neutron_transport_summary/neutron_transport_summary.csv."
        ),
    )
    parser.add_argument(
        "--attenuation-csv",
        default=None,
        help="CSV with ratio_label,bn_wt,zns_wt,L_att_um,mu_eff_per_um,...",
    )
    parser.add_argument(
        "--l-att-um",
        type=float,
        default=None,
        help="Fallback attenuation length in um if no attenuation CSV is provided.",
    )
    parser.add_argument(
        "--macro-model",
        choices=["depth-only", "angle-resolved"],
        default="depth-only",
    )
    parser.add_argument(
        "--epsilon-cos",
        type=float,
        default=1.0e-3,
        help="Minimum |cos(theta)| for angle-resolved path length.",
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Output CSV. Defaults to Output/stageC/<ratio-tag>/thickness_light_yield_curve.csv.",
    )
    parser.add_argument(
        "--bootstrap",
        type=int,
        default=0,
        help="Bootstrap iterations for the front per-capture light CI. Use 0 to disable.",
    )
    parser.add_argument("--seed", type=int, default=12345)
    return parser.parse_args()


def read_csv(path):
    path = Path(path)
    with path.open(newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def to_float(row, name, default=0.0):
    value = row.get(name, "")
    if value == "" or value is None:
        return default
    try:
        number = float(value)
    except ValueError:
        return default
    return number if math.isfinite(number) else default


def to_int(row, name, default=0):
    value = row.get(name, "")
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return default


def fmt(value):
    if value == "" or value is None:
        return ""
    return f"{float(value):.12g}"


def ratio_label_from_parts(bn_wt, zns_wt):
    return f"{bn_wt:g}-{zns_wt:g}"


def placement_name(value):
    return Path(str(value).strip()).name


def event_key(row):
    uid = row.get("source_event_uid", "").strip()
    if uid:
        return ("uid", uid)
    return (
        "event-placement",
        str(row.get("eventID", "")).strip(),
        placement_name(row.get("placement_file", "")),
    )


def find_event_sources(event_source_dir, thickness_label, source_stems):
    root = Path(event_source_dir)
    paths = []
    for stem in source_stems:
        candidate = root / "by_placement" / thickness_label / f"{stem}_event_light_sources.csv"
        if candidate.is_file():
            paths.append(candidate)

    if paths:
        return sorted(set(paths))

    direct = root / f"{thickness_label}_event_light_sources.csv"
    if direct.is_file():
        return [direct]

    matches = sorted(root.glob(f"**/{thickness_label}_*_event_light_sources.csv"))
    return matches


def read_attenuation(args):
    if args.attenuation_csv:
        rows = read_csv(args.attenuation_csv)
        for row in rows:
            ratio = row.get("ratio_label", "").strip()
            bn = to_float(row, "bn_wt", -1.0)
            zns = to_float(row, "zns_wt", -1.0)
            if ratio == args.ratio_tag or ratio_label_from_parts(bn, zns) == args.ratio_tag:
                l_att = to_float(row, "L_att_um", 0.0)
                mu = to_float(row, "mu_eff_per_um", 0.0)
                if l_att <= 0.0 and mu > 0.0:
                    l_att = 1.0 / mu
                if mu <= 0.0 and l_att > 0.0:
                    mu = 1.0 / l_att
                if l_att > 0.0:
                    return l_att, mu
        raise SystemExit(f"No attenuation row found for ratio {args.ratio_tag}: {args.attenuation_csv}")

    if args.l_att_um is not None and args.l_att_um > 0.0:
        return args.l_att_um, 1.0 / args.l_att_um

    raise SystemExit("Provide --attenuation-csv or --l-att-um.")


def read_stagea_capture(stagea_summary):
    path = Path(stagea_summary)
    if not path.is_file():
        return None, None

    rows = read_csv(path)
    sigmas = [to_float(row, "sigma_eff_per_um", 0.0) for row in rows]
    sigmas = [value for value in sigmas if value > 0.0]
    incidents = [
        to_int(
            row,
            "incident_count",
            to_int(row, "n_incident", 0),
        )
        for row in rows
    ]
    incidents = [value for value in incidents if value > 0]
    mean_sigma = sum(sigmas) / len(sigmas) if sigmas else None
    mean_incident = sum(incidents) / len(incidents) if incidents else None
    return mean_sigma, mean_incident


def read_stagea_summary_by_thickness(stagea_summary):
    path = Path(stagea_summary)
    if not path.is_file():
        return {}

    rows = read_csv(path)
    by_thickness = {}
    for row in rows:
        thickness = row.get("thickness_um", "").strip()
        if not thickness:
            continue
        by_thickness[thickness] = row
    return by_thickness


def p_capture_for_thickness(mean_sigma_eff, thickness_um):
    if mean_sigma_eff is None:
        return ""
    return 1.0 - math.exp(-mean_sigma_eff * thickness_um)


def load_event_sources(event_source_paths):
    if not event_source_paths:
        return {}, 0, False

    rows = []
    for path in event_source_paths:
        rows.extend(read_csv(path))

    by_key = {}
    has_zero_light_columns = "has_zns_edep" in (rows[0].keys() if rows else [])
    for row in rows:
        by_key[event_key(row)] = row
        fallback = (
            "event-placement",
            str(row.get("eventID", "")).strip(),
            placement_name(row.get("placement_file", "")),
        )
        by_key.setdefault(fallback, row)
    return by_key, len(rows), has_zero_light_columns


def attenuation_path_length(row, thickness_um, face, macro_model, epsilon_cos):
    depth = to_float(row, "capture_depth_um", to_float(row, "depth_um", 0.0))
    if face == "front":
        depth_to_readout = max(0.0, depth)
    elif face == "back":
        depth_to_readout = max(0.0, thickness_um - depth)
    else:
        depth_to_readout = max(0.0, depth)

    if macro_model == "depth-only":
        return depth_to_readout

    cosz = abs(to_float(row, "exit_dir_z", 1.0))
    return depth_to_readout / max(cosz, epsilon_cos)


def bootstrap_ci(values, iterations, rng):
    if iterations <= 0 or not values:
        return "", ""
    n = len(values)
    means = []
    for _ in range(iterations):
        means.append(sum(values[rng.randrange(n)] for _ in range(n)) / n)
    means.sort()
    low = means[max(0, int(0.025 * iterations) - 1)]
    high = means[min(iterations - 1, int(0.975 * iterations))]
    return low, high


def process_thickness(
    kernel_files,
    stagec_dir,
    event_source_dir,
    stagea_sigma,
    stagea_incident,
    stagea_rows_by_thickness,
    l_att_um,
    mu_eff,
    macro_model,
    epsilon_cos,
    bootstrap,
    rng,
):
    first_match = KERNEL_RE.fullmatch(kernel_files[0].name)
    thickness_label = first_match.group("thickness")
    thickness_um = float(thickness_label)

    source_stems = [
        path.name.replace("_local_optical_kernel_events.csv", "")
        for path in kernel_files
    ]
    event_source_paths = find_event_sources(event_source_dir, thickness_label, source_stems)
    event_sources, n_source_events, has_zero_light_columns = load_event_sources(event_source_paths)

    events = {}
    placements = set()
    bn_wt = 0.0
    zns_wt = 0.0
    ratio_label = ""

    for kernel_file in kernel_files:
        for row in read_csv(kernel_file):
            key = event_key(row)
            src = event_sources.get(key)
            fallback_key = (
                "event-placement",
                str(row.get("eventID", "")).strip(),
                placement_name(row.get("placement_file", "")),
            )
            if src is None:
                src = event_sources.get(fallback_key)

            event = events.setdefault(
                fallback_key,
                {
                    "visible": 0.0,
                    "n_photon0": 0.0,
                    "front": 0.0,
                    "back": 0.0,
                    "side": 0.0,
                    "absorbed": 0.0,
                    "lost": 0.0,
                    "macro_front": 0.0,
                    "macro_back": 0.0,
                },
            )

            visible = to_float(src, "visible_edep_ZnS_keV", to_float(row, "visible_edep_ZnS_keV", 0.0)) if src else to_float(row, "visible_edep_ZnS_keV", 0.0)
            n_photon0 = to_float(src, "n_photon0", to_float(row, "initial_photon_weight", 0.0)) if src else to_float(row, "initial_photon_weight", 0.0)
            event["visible"] = max(event["visible"], visible)
            event["n_photon0"] = max(event["n_photon0"], n_photon0)
            event["front"] += to_float(row, "escaped_front_weight", 0.0)
            event["back"] += to_float(row, "escaped_back_weight", 0.0)
            event["side"] += to_float(row, "escaped_side_weight", 0.0)
            event["absorbed"] += to_float(row, "absorbed_weight", 0.0)
            event["lost"] += to_float(row, "lost_weight", 0.0)

            placements.add(placement_name(row.get("placement_file", "")))
            bn_wt = to_float(row, "bn_wt", bn_wt)
            zns_wt = to_float(row, "zns_wt", zns_wt)
            ratio_label = row.get("ratio_label", "").strip() or ratio_label

    exit_files = []
    for kernel_file in kernel_files:
        stem = kernel_file.name.replace("_local_optical_kernel_events.csv", "")
        exit_path = stagec_dir / f"{stem}_local_optical_exit_photons.csv"
        if exit_path.is_file():
            exit_files.append(exit_path)

    for exit_file in exit_files:
        for row in read_csv(exit_file):
            face = row.get("exit_surface", "").strip()
            if face not in ("front", "back"):
                continue
            key = (
                "event-placement",
                str(row.get("eventID", "")).strip(),
                placement_name(row.get("placement_file", "")),
            )
            event = events.get(key)
            if event is None:
                continue
            s_um = attenuation_path_length(row, thickness_um, face, macro_model, epsilon_cos)
            weight = to_float(row, "weight", 0.0)
            transmitted = weight * math.exp(-s_um / l_att_um)
            if face == "front":
                event["macro_front"] += transmitted
            else:
                event["macro_back"] += transmitted

    if n_source_events > len(events):
        for row in event_sources.values():
            if not isinstance(row, dict):
                continue
            if placement_name(row.get("placement_file", "")) not in placements:
                continue
            key = (
                "event-placement",
                str(row.get("eventID", "")).strip(),
                placement_name(row.get("placement_file", "")),
            )
            events.setdefault(
                key,
                {
                    "visible": to_float(row, "visible_edep_ZnS_keV", 0.0),
                    "n_photon0": to_float(row, "n_photon0", 0.0),
                    "front": 0.0,
                    "back": 0.0,
                    "side": 0.0,
                    "absorbed": 0.0,
                    "lost": 0.0,
                    "macro_front": 0.0,
                    "macro_back": 0.0,
                },
            )

    n_events = len(events)
    denom = n_events if n_events > 0 else 1
    sums = defaultdict(float)
    front_values = []
    for event in events.values():
        for name, value in event.items():
            sums[name] += value
        front_values.append(event["macro_front"])

    total_initial = sums["front"] + sums["back"] + sums["side"] + sums["absorbed"] + sums["lost"]
    frac_denom = total_initial if total_initial > 0.0 else 1.0
    summary_row = stagea_rows_by_thickness.get(thickness_label, {})
    n_incident = to_int(
        summary_row,
        "n_incident",
        to_int(summary_row, "incident_count", 0),
    )
    n_absorb = to_int(
        summary_row,
        "n_absorb",
        to_int(summary_row, "capture_count", 0),
    )

    p_capture = ""
    if n_incident > 0 and n_absorb >= 0:
        p_capture = n_absorb / n_incident
    else:
        p_capture = p_capture_for_thickness(stagea_sigma, thickness_um)
        if stagea_incident is not None and p_capture != "":
            n_incident = int(round(stagea_incident))
        else:
            n_incident = ""

    ci_low, ci_high = bootstrap_ci(front_values, bootstrap, rng)
    ratio_label = ratio_label or ratio_label_from_parts(bn_wt, zns_wt)

    return {
        "ratio_label": ratio_label,
        "bn_wt": fmt(bn_wt),
        "zns_wt": fmt(zns_wt),
        "thickness_um": fmt(thickness_um),
        "P_capture": fmt(p_capture) if p_capture != "" else "",
        "n_captured_events": str(n_events),
        "n_total_incident_neutrons": str(n_incident) if n_incident != "" else "",
        "mean_visible_edep_ZnS_keV_per_capture": fmt(sums["visible"] / denom),
        "mean_n_photon0_per_capture": fmt(sums["n_photon0"] / denom),
        "mean_local_front_escape_per_capture": fmt(sums["front"] / denom),
        "mean_local_back_escape_per_capture": fmt(sums["back"] / denom),
        "mean_local_side_escape_per_capture": fmt(sums["side"] / denom),
        "mean_macro_weighted_front_light_per_capture": fmt(sums["macro_front"] / denom),
        "mean_macro_weighted_back_light_per_capture": fmt(sums["macro_back"] / denom),
        "mean_macro_weighted_front_light_per_incident_neutron": fmt((p_capture * sums["macro_front"] / denom) if p_capture != "" else ""),
        "mean_macro_weighted_back_light_per_incident_neutron": fmt((p_capture * sums["macro_back"] / denom) if p_capture != "" else ""),
        "side_escape_fraction": fmt(sums["side"] / frac_denom),
        "absorbed_fraction": fmt(sums["absorbed"] / frac_denom),
        "lost_fraction": fmt(sums["lost"] / frac_denom),
        "L_att_um": fmt(l_att_um),
        "mu_eff_per_um": fmt(mu_eff),
        "macro_model": macro_model,
        "n_placements": str(len([p for p in placements if p])),
        "bootstrap_ci_low": fmt(ci_low) if ci_low != "" else "",
        "bootstrap_ci_high": fmt(ci_high) if ci_high != "" else "",
        "_event_source_path": ";".join(str(path) for path in event_source_paths),
        "_has_zero_light_columns": has_zero_light_columns,
    }


def main():
    args = parse_args()
    project_root = Path(args.project_root).resolve() if args.project_root else Path(__file__).resolve().parent
    stagec_dir = Path(args.stagec_dir).resolve() if args.stagec_dir else project_root / "Output" / "stageC" / args.ratio_tag
    event_source_dir = Path(args.event_source_dir).resolve() if args.event_source_dir else project_root / "Input" / "alpha_li_steps" / args.ratio_tag
    stagea_summary = (
        Path(args.stagea_summary).resolve()
        if args.stagea_summary
        else project_root
        / "Input"
        / "stageA"
        / args.ratio_tag
        / "neutron_transport_summary"
        / "neutron_transport_summary.csv"
    )
    output = Path(args.output).resolve() if args.output else stagec_dir / "thickness_light_yield_curve.csv"

    if not stagec_dir.is_dir():
        raise SystemExit(f"Stage C directory not found: {stagec_dir}")
    if not event_source_dir.is_dir():
        raise SystemExit(f"Event source directory not found: {event_source_dir}")
    if args.epsilon_cos <= 0.0:
        raise SystemExit("--epsilon-cos must be > 0")

    l_att_um, mu_eff = read_attenuation(args)
    stagea_sigma, stagea_incident = read_stagea_capture(stagea_summary)
    stagea_rows_by_thickness = read_stagea_summary_by_thickness(stagea_summary)

    grouped = defaultdict(list)
    for path in sorted(stagec_dir.glob("*_local_optical_kernel_events.csv")):
        match = KERNEL_RE.fullmatch(path.name)
        if match is None:
            continue
        grouped[match.group("thickness")].append(path)

    if not grouped:
        raise SystemExit(f"No *_local_optical_kernel_events.csv files found in {stagec_dir}")

    rng = random.Random(args.seed)
    rows = []
    for _, kernel_files in sorted(grouped.items(), key=lambda item: float(item[0])):
        rows.append(
            process_thickness(
                kernel_files,
                stagec_dir,
                event_source_dir,
                stagea_sigma,
                stagea_incident,
                stagea_rows_by_thickness,
                l_att_um,
                mu_eff,
                args.macro_model,
                args.epsilon_cos,
                args.bootstrap,
                rng,
            )
        )

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=OUTPUT_FIELDS)
        writer.writeheader()
        for row in rows:
            writer.writerow({name: row.get(name, "") for name in OUTPUT_FIELDS})

    print(f"Output:          {output}")
    print(f"Ratio:           {args.ratio_tag}")
    print(f"Stage C dir:     {stagec_dir}")
    print(f"Event source dir:{event_source_dir}")
    print(f"Stage A summary: {stagea_summary if stagea_summary.is_file() else 'not found'}")
    print(f"Macro model:     {args.macro_model}")
    print(f"L_att_um:        {l_att_um:g}")
    print(f"Thickness rows:  {len(rows)}")
    for row in rows:
        if not row.get("_has_zero_light_columns", False):
            print(
                "Warning: event source file lacks has_zns_edep; per-capture means "
                "may miss zero-light captures until sources are regenerated."
            )
            break


if __name__ == "__main__":
    main()
