#!/usr/bin/env python3
"""Reproduce pure temporary-raster and bounded transport component tests.

No GIMP/GEGL adapter is exercised here. Hold the shared build lock when using
its build directory. Sanitizers instrument these units, not GLib/system libs.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path)
parser.add_argument("--report", type=Path, required=True)
parser.add_argument("--sanitize", action="store_true")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
out = args.build.resolve() / ("filter-storage-sanitizers" if args.sanitize else "filter-storage-tests")
out.mkdir(parents=True, exist_ok=True)
units = {"raster": ["app/painter/filter-raster.cpp", "app/painter/tests/test-filter-raster.cpp"],
         "spool": ["app/painter/filter-raster.cpp", "app/painter/filter-spool.cpp", "app/painter/tests/test-filter-spool.cpp"]}
headers = ["app/painter/filter-raster.hpp", "app/painter/filter-spool.hpp", "app/painter/work-admission.hpp"]
sources = sorted(set(headers + [str(Path(__file__).resolve().relative_to(root))] + sum(units.values(), [])))
sha = lambda path: hashlib.sha256((root / path).read_bytes()).hexdigest()
before = {path: sha(path) for path in sources}
flags = ["-std=c++14", "-D_FILE_OFFSET_BITS=64", "-Wall", "-Wextra", "-Werror", "-g", "-pthread", "-I" + str(root / "app/painter")]
flags += ["-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-frtti"] if args.sanitize else ["-O2"]
packages = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0"], text=True))
env = dict(os.environ)
if args.sanitize:
    env.update(ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:abort_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
runs = []
for name, translation_units in units.items():
    exe = out / ("painter-filter-" + name)
    command = ["g++", *flags, *[str(root / path) for path in translation_units], *packages, "-o", str(exe)]
    subprocess.run(command, check=True)
    result = subprocess.run([str(exe)], env=env, capture_output=True, text=True)
    runs.append({"name": name, "translation_units": translation_units, "compile_command": command,
                 "test_command": [str(exe)], "exit_code": result.returncode, "stdout": result.stdout, "stderr": result.stderr})
    print(result.stdout)
changed = [path for path in sources if sha(path) != before[path]]
report = {"scope": "Six worker-only temporary-raster/transpose cases and nine bounded input/result transport cases; GLib/system libraries uninstrumented; no live GIMP large-image gate claimed",
          "source_sha256": before, "changed_after_compile": changed,
          "sanitizers": ["address", "undefined"] if args.sanitize else [], "leak_detection": False,
          "runs": runs, "all_passed": not changed and all(run["exit_code"] == 0 for run in runs)}
args.report.write_text(json.dumps(report, indent=2) + "\n")
if not report["all_passed"]:
    for run in runs:
        if run["exit_code"]:
            print(run["stderr"], file=sys.stderr)
sys.exit(0 if report["all_passed"] else 1)
