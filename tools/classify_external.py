#!/usr/bin/env python3
"""Assign a migration owner to every legacy change outside app/."""

import csv
from pathlib import Path


def treatment(path: str) -> tuple[str, str]:
    if path.startswith("data/mypaint-brushes/"):
        if path.endswith(".myb"):
            return "brush-definition", "19,20,36"
        if path.endswith(".png"):
            return "brush-preview", "24,30,36"
        return "brush-manifest-or-metadata", "19,30"
    if path.startswith("data/layer-presets/"):
        return "layer-preset-or-manifest", "28,36"
    if path.startswith("data/"):
        return "asset-build-manifest", "30"
    if path.startswith("plug-ins/"):
        return "plugin-procedure-or-behavior", "16,31,36"
    if path.startswith("libgimp"):
        return "public-library-api-or-enum", "04,08,13,19,30"
    if path.startswith("themes/"):
        return "theme-resource", "29,30"
    if path.startswith("menus/"):
        return "menu-action", "24,30"
    if path.startswith("tools/"):
        return "generator-source", "01.010,04.013,30"
    if path.startswith("build/"):
        return "packaging-or-dependency", "32,33,34"
    if path == "configure.ac":
        return "legacy-build-definition", "03,04,31"
    if path.startswith("etc/"):
        return "user-default-or-configuration", "30"
    if path.startswith("modules/"):
        return "optional-module", "30,31"
    if path.startswith("po/"):
        return "translation", "30"
    raise ValueError(f"unassigned external file: {path}")


def main() -> None:
    source = Path("migration/inventory/changed-files.tsv")
    output = Path("migration/inventory/external-changes.tsv")
    with source.open(newline="", encoding="utf-8") as file:
        files = list(csv.DictReader(file, delimiter="\t"))
    rows = []
    for record in files:
        path = record["path"]
        if path.startswith("app/"):
            continue
        kind, tasks = treatment(path)
        rows.append((f"01.003/{len(rows) + 1:04d}", "DONE", path,
                     record["change"], kind, "retain-and-verify", tasks,
                     "Individual implementation and comparison remain in assigned WBS sections"))
    with output.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("child_id", "status", "path", "change", "kind",
                         "disposition", "implementation_and_test_sections", "limitation"))
        writer.writerows(rows)
    if len(rows) != sum(not row["path"].startswith("app/") for row in files):
        raise ValueError("some external paths were not classified")
    print(f"{len(rows)} external changes assigned")


if __name__ == "__main__":
    main()
