#!/usr/bin/env python3
"""Detect incomplete or non-executable outputs after a Linux Meson build."""

import argparse
import json
import os
import stat
import subprocess
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--repair-permissions", action="store_true",
                        help="restore execute bits only for valid ELF targets")
    options = parser.parse_args()
    targets = json.loads(subprocess.check_output(
        ["meson", "introspect", str(options.build_dir), "--targets"], text=True))
    build_root = options.build_dir.resolve()
    checked = 0
    absent = []
    optional = []
    broken = []
    repaired = []
    for target in targets:
        if target["type"] != "executable":
            continue
        for name in target["filename"]:
            path = Path(name)
            label = str(path.resolve().relative_to(build_root))
            if not path.exists():
                (absent if target["build_by_default"] else optional).append(label)
                continue
            checked += 1
            with path.open("rb") as file:
                magic = file.read(4)
            if magic == b"\x7fELF" and not (path.stat().st_mode & stat.S_IXUSR) \
                    and options.repair_permissions:
                path.chmod(path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
                repaired.append(label)
            if magic != b"\x7fELF" or not (path.stat().st_mode & stat.S_IXUSR):
                broken.append({"path": label, "bytes": path.stat().st_size,
                               "magic": magic.hex(), "executable": os.access(path, os.X_OK)})
    result = {"checked_elf_executables": checked,
              "unbuilt_targets": absent, "invalid_executables": broken,
              "repaired_executable_permissions": repaired}
    result["optional_unbuilt_targets"] = optional
    if options.report:
        options.report.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"{checked} executables checked; {len(broken)} invalid; "
          f"{len(absent)} unbuilt; {len(optional)} optional targets skipped; "
          f"{len(repaired)} executable permissions restored")
    for problem in broken:
        print(problem)
    return 1 if broken or absent else 0


if __name__ == "__main__":
    raise SystemExit(main())
