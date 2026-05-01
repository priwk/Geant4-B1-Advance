#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path


TARGET_COLUMNS = {"placement_file", "source_csv"}
SKIP_DIRS = {"build", "Output", "logs", ".git", "__pycache__"}


def project_root():
    return Path(__file__).resolve().parent.parent


def should_skip(path: Path) -> bool:
    return any(part in SKIP_DIRS for part in path.parts)


def normalize_value(value: str, root: Path) -> str:
    raw = (value or "").strip()
    if not raw:
        return raw

    path = Path(raw)
    candidate = path if path.is_absolute() else (root / path)
    candidate = candidate.resolve(strict=False)

    try:
        return candidate.relative_to(root).as_posix()
    except ValueError:
        return candidate.as_posix()


def rewrite_csv(path: Path, root: Path) -> bool:
    with path.open(newline="", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        fieldnames = reader.fieldnames
        if not fieldnames:
            return False
        target_fields = [name for name in fieldnames if name in TARGET_COLUMNS]
        if not target_fields:
            return False
        rows = list(reader)

    changed = False
    for row in rows:
        for field in target_fields:
            original = row.get(field, "")
            normalized = normalize_value(original, root)
            if normalized != original:
                row[field] = normalized
                changed = True

    if not changed:
        return False

    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    return True


def main():
    parser = argparse.ArgumentParser(description="Normalize repo-local paths inside CSV fields.")
    parser.add_argument(
        "targets",
        nargs="*",
        help="Optional files or directories to scan. Defaults to Input/alpha_li_steps.",
    )
    args = parser.parse_args()

    root = project_root()
    changed_files = []
    targets = args.targets or ["Input/alpha_li_steps"]

    for target in targets:
        resolved = (root / target).resolve() if not Path(target).is_absolute() else Path(target).resolve()
        if resolved.is_dir():
            iterable = resolved.rglob("*.csv")
        elif resolved.is_file() and resolved.suffix.lower() == ".csv":
            iterable = [resolved]
        else:
            continue

        for path in iterable:
            if should_skip(path):
                continue
            if rewrite_csv(path, root):
                changed_files.append(path.relative_to(root).as_posix())

    for rel in changed_files:
        print(rel)

    print(f"normalized_files={len(changed_files)}")


if __name__ == "__main__":
    main()
