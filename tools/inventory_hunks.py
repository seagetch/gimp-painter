#!/usr/bin/env python3
"""Enumerate each changed legacy hunk and assign a provisional work category."""

import argparse
import csv
import hashlib
import json
import re
import subprocess
from pathlib import Path


HEADER = re.compile(r"^@@ (-\d+(?:,\d+)?) \+(\d+(?:,\d+)?) @@")


def category(path: str) -> str:
    if path.startswith(("app/", "libgimp/", "libgimpbase/", "libgimpwidgets/", "libgimpconfig/")):
        return "application-or-library"
    if path.startswith(("data/", "po/")):
        return "asset-or-translation"
    if path.startswith(("plug-ins/", "modules/", "themes/")):
        return "plugin-or-module"
    if path.startswith(("build/", "tools/", "autogen", "configure", "Makefile")):
        return "build-or-tool"
    return "other"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    cmd = ["git", "-c", "core.quotePath=false", "diff", "--no-renames",
           "--no-ext-diff", "--no-textconv", "--no-color", "--unified=0",
           args.base, args.source]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True,
                            encoding="utf-8", errors="surrogateescape")
    rows = []
    path, number, section_started = "", 0, False

    def append(header: str, kind: str) -> None:
        nonlocal number, section_started
        number += 1
        section_started = True
        rows.append((f"01.002/{len(rows) + 1:06d}", "DONE", path, number, kind,
                     header, category(path), "", "", "Git diff section enumerated"))

    for line in proc.stdout:
        if line.startswith("diff --git a/"):
            if path and not section_started:
                append("no textual hunk", "metadata-or-binary")
            path = line[len("diff --git a/"):].split(" b/", 1)[0]
            number, section_started = 0, False
        elif line.startswith("@@ "):
            match = HEADER.match(line)
            if not match:
                raise ValueError(f"unexpected hunk header: {line.strip()}")
            append(f"{match[1]} -> {match[2]}", "text")
    if path and not section_started:
        append("no textual hunk", "metadata-or-binary")
    if proc.wait():
        raise subprocess.CalledProcessError(proc.returncode, cmd)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("child_id", "status", "path", "index", "kind", "old_new_lines",
                         "provisional_category", "feature_or_base", "disposition_task", "result"))
        writer.writerows(rows)
    with Path("migration/inventory/changed-files.tsv").open(newline="", encoding="utf-8") as file:
        files = {row["path"] for row in csv.DictReader(file, delimiter="\t")}
    covered = {row[2] for row in rows}
    if covered != files:
        raise ValueError(f"uncovered files: {sorted(files - covered)}; extras: {sorted(covered - files)}")
    args.output.with_suffix(".json").write_text(json.dumps({
        "hunks_or_binary_sections": len(rows), "changed_paths": len(covered),
        "inventory_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
        "assignment_state": "provisional; 01.003 and 01.013 remain open"
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(rows)} sections in {len(covered)} files")


if __name__ == "__main__":
    main()
