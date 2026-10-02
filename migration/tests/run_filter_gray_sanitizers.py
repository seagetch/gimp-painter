#!/usr/bin/env python3
"""Instrument both pure kernels and all genuine native Gray byte comparisons."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path)
parser.add_argument("--report", type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
out = args.build.resolve() / "filter-gray-sanitizers"
out.mkdir(exist_ok=True)
exe = out / "painter-filter-gray"
sources = ["app/painter/filter-edge.cpp", "app/painter/filter-gauss.cpp", "app/painter/tests/test-filter-gray.cpp"]
manifest = root / "migration/fixtures/legacy-gray-filter/fixtures.tsv"
command = ["g++", "-std=c++14", "-Wall", "-Wextra", "-Werror", "-g", "-O1", "-pthread",
           "-fsanitize=address,undefined,float-cast-overflow", "-fno-omit-frame-pointer", "-frtti",
           "-I" + str(root / "app/painter")]
command += [str(root / s) for s in sources] + ["-o", str(exe)]
subprocess.run(command, check=True)
env = dict(os.environ)
env.update({"ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1:abort_on_error=1",
            "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})
result = subprocess.run([str(exe), str(manifest)], env=env, capture_output=True, text=True)
args.report.write_text(json.dumps({
    "scope": "Independent Edge/Gauss kernels and 77 genuine native Gray/Gray-alpha corpus cases; system libraries uninstrumented",
    "sources": sources, "command": command, "test_command": [str(exe), str(manifest)],
    "source_sha256": {s: hashlib.sha256((root / s).read_bytes()).hexdigest() for s in sources},
    "manifest_sha256": hashlib.sha256(manifest.read_bytes()).hexdigest(),
    "sanitizers": ["address", "undefined", "float-cast-overflow"],
    "leak_detection": False, "exit_code": result.returncode,
    "stdout": result.stdout, "stderr": result.stderr,
}, indent=2) + "\n")
print(result.stdout)
if result.returncode:
    print(result.stderr, file=sys.stderr)
sys.exit(result.returncode)
