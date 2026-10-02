#!/usr/bin/env python3
"""Instrument XCF application loading and independent layer adapters in the existing full GIMP harness.

Run after building app/tests/painter-xcf-open, with the build environment loaded.
Does not claim to instrument all upstream GIMP/dependencies. Original build
objects are never replaced. Hold /tmp/gimp-painter-build.lock when sharing it.
"""
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
build = args.build.resolve()
root = Path(__file__).resolve().parents[2]
output = build / "painter-xcf-open-sanitizers"
output.mkdir(exist_ok=True)
report = {"scope": "XCF load/probe/source/seek/read, Clone/Filter adapters, test harness and shared BindingStore; remaining GIMP/dependencies uninstrumented",
          "sanitizers": ["address", "undefined"], "leak_detection": False,
          "sources": [], "commands": []}
flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-O1"]
wanted = {"app/xcf/xcf.c", "app/xcf/xcf-load.c", "app/xcf/xcf-read.c", "app/xcf/xcf-seek.c",
          "app/xcf/painter-xcf-load.cpp", "app/xcf/painter-xcf-compat.cpp",
          "app/tests/test-painter-xcf-open.c", "app/core/gimpclonelayer.cpp", "app/core/gimpfilterlayer.cpp",
          "app/painter/binding-store.cpp", "app/painter/gimp-painter-binding.cpp", "app/painter/gimp-painter-error.cpp"}
replacements = {}
extra = []
commands = json.loads((build / "compile_commands.json").read_text())
for entry in commands:
    source = (Path(entry["directory"]) / entry["file"]).resolve()
    try:
        relative = source.relative_to(root).as_posix()
    except ValueError:
        continue
    if relative not in wanted or relative in report["sources"]:
        continue
    cmd = shlex.split(entry["command"])
    cleaned = []
    skip = False
    for arg in cmd:
        if skip:
            skip = False
            continue
        if arg in ("-MF", "-MQ", "-MT"):
            skip = True
            continue
        if arg in ("-MD", "-MMD"):
            continue
        cleaned.append(arg)
    obj = output / (source.name + ".o")
    cleaned[cleaned.index("-o") + 1] = str(obj)
    cleaned += flags
    report["sources"].append(relative)
    report["commands"].append(cleaned)
    subprocess.run(cleaned, cwd=build, check=True)
    if relative.startswith("app/tests/"):
        replacements[entry["output"]] = str(obj)
    else:
        extra.append(str(obj))
if set(report["sources"]) != wanted:
    raise RuntimeError("Compile database lacks required source(s)")
link_text = subprocess.check_output(["ninja", "-t", "commands", "app/tests/painter-xcf-open"], cwd=build, text=True)
link = shlex.split(link_text.strip().splitlines()[-1])
exe = build / "app/tests/painter-xcf-open-asan"
link[link.index("-o") + 1] = str(exe)
link = [replacements.get(arg, arg) for arg in link]
link[1:1] = flags + extra
report["commands"].append(link)
subprocess.run(link, cwd=build, check=True)
env = dict(os.environ)
env.update({"GIMP_TESTING_ABS_TOP_SRCDIR": str(root),
            "GIMP_TESTING_ABS_TOP_BUILDDIR": str(build),
            "GIMP_TESTING_PLUGINDIRS": str(build / "plug-ins/common"),
            "UI_TEST": "yes", "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1:abort_on_error=1",
            "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})
result = subprocess.run([str(exe)], cwd=build, env=env, capture_output=True, text=True)
report.update({"exit_code": result.returncode, "stdout": result.stdout, "stderr": result.stderr})
args.report.write_text(json.dumps(report, indent=2) + "\n")
print(result.stdout)
if result.returncode:
    print(result.stderr, file=sys.stderr)
sys.exit(result.returncode)
