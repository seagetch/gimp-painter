#!/usr/bin/env python3
"""Reproduce the fair dispatcher ASan/UBSan suite without runtime env dumps."""
import argparse
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
out = args.build.resolve() / "fair-dispatcher-sanitizers"
out.mkdir(exist_ok=True)
exe = out / "painter-fair-dispatcher"
sources = ["app/painter/gimp-painter-error.cpp", "app/painter/tests/test-fair-dispatcher.cpp"]
package_flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "gobject-2.0"], text=True))
command = ["g++", "-std=c++14", "-Wall", "-Wextra", "-Werror", "-g", "-O1", "-pthread",
           "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-frtti", "-I" + str(root / "app/painter")]
command += [str(root / s) for s in sources] + package_flags + ["-o", str(exe)]
subprocess.run(command, check=True)
env = dict(os.environ)
env.update({"ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1:abort_on_error=1",
            "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})
result = subprocess.run([str(exe)], env=env, capture_output=True, text=True)
args.report.write_text(json.dumps({
    "scope": "FairDispatcher, Source, and regression tests; GLib/GObject/system libraries uninstrumented",
    "sources": sources, "command": command, "sanitizers": ["address", "undefined"],
    "leak_detection": False, "exit_code": result.returncode,
    "stdout": result.stdout, "stderr": result.stderr,
}, indent=2) + "\n")
print(result.stdout)
if result.returncode:
    print(result.stderr, file=sys.stderr)
sys.exit(result.returncode)
