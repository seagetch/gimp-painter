#!/usr/bin/env python3
"""Observe the real >64-Mi-pixel FilterLayer in fresh Linux processes.

Use the normal dependency environment and shared build lock. wait4 measures
individual child high-water RSS, not a cumulative previous-child maximum.
RSS excludes kernel filesystem cache/backing; this is not total-memory proof.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path)
parser.add_argument("--report", type=Path, required=True)
parser.add_argument("--runs", type=int, default=3)
args = parser.parse_args()
if not 1 <= args.runs <= 10:
    parser.error("runs must be 1..10")
if not hasattr(os, "wait4") or os.uname().sysname != "Linux":
    parser.error("this RSS observation runner requires Linux wait4")
root = Path(__file__).resolve().parents[2]
build = args.build.resolve()
exe = build / "app/tests/gimp-filter-layer"
command = [str(exe), "--verbose", "-p", "/gimp-filter-layer/large_spill_execution_finishes"]
env = dict(os.environ)
env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(root), GIMP_TESTING_ABS_TOP_BUILDDIR=str(build),
           GIMP_TESTING_PLUGINDIRS=str(build / "plug-ins/common"), UI_TEST="yes")
sources = ["app/core/gimpfilterlayer.cpp", "app/core/gimpfilterlayer.h", "app/painter/filter-scheduler.cpp", "app/painter/filter-scheduler.hpp",
           "app/painter/filter-spool.cpp", "app/painter/filter-spool.hpp", "app/painter/filter-raster.cpp",
           "app/painter/filter-raster.hpp", "app/painter/filter-raster-kernels.cpp", "app/painter/filter-raster-kernels.hpp",
           "app/painter/work-admission.hpp", "app/tests/test-gimp-filter-layer.c", "app/tests/test-gimp-filter-layout.cpp"]
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
before = {name: sha(root / name) for name in sources}
report = {"scope": "Actual GIMP 8193x8193 RGB U8 Edge spill, sparse opaque far corner; complete cache and final-pixel assertions",
          "fixed_hardware_gate": False, "command": command,
          "source_sha256": before, "executable_sha256": sha(exe), "runs": [],
          "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
          "limits": ["Linux child RSS includes its resident GIMP/GEGL heap but excludes kernel filesystem cache/backing.",
                     "Configured test swap is build/filter-spill-cache, not implicit /tmp; production uses its configured expanded GEGL swap path.",
                     "Shared host load is uncontrolled; heartbeat p95/p99/max are observations, not latency acceptance thresholds. max_quantum_us now covers full callback/destructor scope.",
                     "This is one sparse large Edge workload; dense/complex graphs, many simultaneous images and native Windows remain separate gates."],
          "environment": {key: env[key] for key in ("GIMP_DEPS_DIRECTORY", "GEGL_THREADS", "BABL_TOLERANCE",
              "GIMP_TESTING_ABS_TOP_SRCDIR", "GIMP_TESTING_ABS_TOP_BUILDDIR", "GIMP_TESTING_PLUGINDIRS", "UI_TEST") if key in env}}
for index in range(args.runs):
    with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
        process_started = time.monotonic()
        child = subprocess.Popen(command, cwd=root, env=env, stdout=stdout, stderr=stderr)
        deadline = time.monotonic() + 150
        timed_out = False
        while True:
            pid, status, usage = os.wait4(child.pid, os.WNOHANG)
            if pid:
                child.returncode = os.waitstatus_to_exitcode(status)
                break
            if time.monotonic() > deadline:
                timed_out = True
                child.kill()  # only this disposable test process, never a production worker thread
                pid, status, usage = os.wait4(child.pid, 0)
                child.returncode = os.waitstatus_to_exitcode(status)
                break
            time.sleep(0.05)
        stdout.seek(0); stderr.seek(0)
        text = stdout.read().decode(errors="replace")
        errors = stderr.read().decode(errors="replace")
    elapsed = time.monotonic() - process_started
    observations = []
    close_observations = []
    for line in text.splitlines():
        if "large-spill width=" in line:
            observations.append({key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", line)})
        elif "large-spill-close " in line:
            close_observations.append({key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", line)})
    report["runs"].append({"run": index+1, "exit_code": child.returncode, "timed_out": timed_out,
        "observations": observations, "close_observations": close_observations, "process_wall_seconds": elapsed, "peak_rss_kib": usage.ru_maxrss,
        "user_cpu_seconds": usage.ru_utime, "system_cpu_seconds": usage.ru_stime,
        "ru_inblock": usage.ru_inblock, "ru_oublock": usage.ru_oublock,
        "stdout": text, "stderr": errors})
    print(json.dumps({"run": index+1, "peak_rss_kib": usage.ru_maxrss, "observations": observations}))
    if child.returncode or len(observations) != 1 or len(close_observations) != 1:
        break
cache = build / "filter-spill-cache"
report["swap_directory"] = str(cache)
report["swap_filesystem"] = subprocess.check_output(["stat", "-f", "-c", "%T", str(cache)], text=True).strip()
report["changed_after_run"] = [name for name in sources if sha(root / name) != before[name]]
report["all_passed"] = not report["changed_after_run"] and len(report["runs"]) == args.runs and all(
    run["exit_code"] == 0 and len(run["observations"]) == 1 and len(run["close_observations"]) == 1 for run in report["runs"])
args.report.write_text(json.dumps(report, indent=2) + "\n")
raise SystemExit(0 if report["all_passed"] else 1)
