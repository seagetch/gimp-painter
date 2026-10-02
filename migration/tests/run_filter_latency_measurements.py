#!/usr/bin/env python3
"""Observe actual FilterLayer first-use/edit latency in fresh GIMP processes.

Run with the normal build environment and shared build lock. These measurements
are not a fixed reference-machine performance gate and do not hide cold setup.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path)
parser.add_argument("--report", type=Path, required=True)
parser.add_argument("--runs", type=int, default=3)
args = parser.parse_args()
if args.runs < 1 or args.runs > 10:
    parser.error("runs must be between 1 and 10")
root = Path(__file__).resolve().parents[2]
build = args.build.resolve()
env = dict(os.environ)
env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(root),
           GIMP_TESTING_ABS_TOP_BUILDDIR=str(build),
           GIMP_TESTING_PLUGINDIRS=str(build / "plug-ins/common"), UI_TEST="yes")
command = [str(build / "app/tests/gimp-filter-layer"), "--verbose", "-p",
           "/gimp-filter-layer/complex_graph_remains_responsive"]
report = {"scope": "Real GIMP 1024x1024, 64 partially opaque legacy-Normal lower layers, edge executor",
          "fixed_hardware_gate": False,
          "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
          "source_sha256": {}, "executable_sha256": hashlib.sha256((build / "app/tests/gimp-filter-layer").read_bytes()).hexdigest(),
          "command": command, "runs": [],
          "limits": ["Fresh-process first includes cold graph/format setup; edit uses that same graph after a source edit.",
                     "max_quantum_us is cumulative per FilterLayer; heartbeat percentiles are per phase.",
                     "Shared host load is uncontrolled; this is an observation, not p95/p99 acceptance certification.",
                     "This complex graph is at the small-vector cutoff; large spill resource/latency evidence is separate. Cold graph latency remains outstanding."],
          "environment": {k: env[k] for k in ("GIMP_DEPS_DIRECTORY", "GEGL_THREADS", "BABL_TOLERANCE",
              "GIMP_TESTING_ABS_TOP_SRCDIR", "GIMP_TESTING_ABS_TOP_BUILDDIR", "GIMP_TESTING_PLUGINDIRS", "UI_TEST") if k in env}}
for name in ("app/core/gimpfilterlayer.cpp", "app/painter/filter-scheduler.cpp",
             "app/painter/filter-scheduler.hpp", "app/tests/test-gimp-filter-layer.c"):
    report["source_sha256"][name] = hashlib.sha256((root / name).read_bytes()).hexdigest()
for run in range(args.runs):
    result = subprocess.run(command, env=env, cwd=root, capture_output=True, text=True, timeout=90)
    observations = []
    for line in result.stdout.splitlines():
        if "complex-graph phase=" in line:
            values = dict(re.findall(r"(\w+)=(\w+)", line))
            observations.append({k: int(v) if v.isdigit() else v for k, v in values.items()})
    report["runs"].append({"run": run+1, "returncode": result.returncode,
                           "observations": observations, "stdout": result.stdout,
                           "stderr": result.stderr})
    if result.returncode or len(observations) != 2:
        report["status"] = "failed"
        args.report.write_text(json.dumps(report, indent=2) + "\n")
        raise SystemExit(result.returncode or 1)
report["changed_after_run"] = [name for name, digest in report["source_sha256"].items()
    if hashlib.sha256((root / name).read_bytes()).hexdigest() != digest]
report["status"] = "failed" if report["changed_after_run"] else "passed"
args.report.write_text(json.dumps(report, indent=2) + "\n")
for run in report["runs"]:
    for observation in run["observations"]:
        print(json.dumps({"run": run["run"], **observation}))

raise SystemExit(0 if report["status"] == "passed" else 1)
