#!/usr/bin/env python3
"""Reproduce the independent scheduler ASan/UBSan suite without runtime env dumps."""
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
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
out = args.build.resolve() / "filter-scheduler-sanitizers"
out.mkdir(exist_ok=True)
exe = out / "painter-filter-scheduler"
sources = ["app/painter/filter-raster.cpp", "app/painter/filter-spool.cpp", "app/painter/filter-scheduler.cpp", "app/painter/tests/test-filter-scheduler.cpp"]
source_hashes = {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in sources}
package_flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0"], text=True))
command = ["g++", "-std=c++14", "-Wall", "-Wextra", "-Werror", "-g", "-O1", "-pthread",
           "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-frtti", "-I" + str(root / "app/painter")]
command += [str(root / s) for s in sources] + package_flags + ["-o", str(exe)]
subprocess.run(command, check=True)
env = dict(os.environ)
env.update({"ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1:abort_on_error=1",
            "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})
result = subprocess.run([str(exe)], env=env, capture_output=True, text=True)
args.report.write_text(json.dumps({
    "scope": "Independent FilterScheduler and regression tests; GLib/system libraries uninstrumented",
    "sources": sources, "source_sha256": source_hashes, "command": command, "sanitizers": ["address", "undefined"],
    "leak_detection": False, "exit_code": result.returncode,
    "stdout": result.stdout, "stderr": result.stderr,
}, indent=2) + "\n")
print(result.stdout)
if result.returncode:
    print(result.stderr, file=sys.stderr)
sys.exit(result.returncode)
