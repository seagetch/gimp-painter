#!/usr/bin/env python3
"""Instrument the Painter legacy mode and its test adapter in the existing full GIMP harness.

Run after building app/tests/painter-layer-modes, with the build environment loaded.
Does not claim to instrument all upstream GIMP/dependencies. Original build
objects are never replaced. Hold /tmp/gimp-painter-build.lock when sharing it.
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
args = parser.parse_args()
build = args.build.resolve()
root = Path(__file__).resolve().parents[2]
output = build / "layer-mode-sanitizers"
output.mkdir(exist_ok=True)
report = {"scope": "Painter legacy operation, cached direct-mode dispatch and C app test; remaining upstream GIMP/GEGL/dependencies uninstrumented",
          "sanitizers": ["address", "undefined"], "leak_detection": False, "instrumented_cpp_rtti": False,
          "sources": [], "commands": []}
flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-O1"]
wanted = {"app/operations/layer-modes-legacy/gimpoperationpainterlegacy.c",
          "app/operations/layer-modes/gimp-layer-modes.c",
          "app/tests/test-painter-layer-modes.c"}
replacements = {}
extra = []
archive_replacements = {}
commands = json.loads((build / "compile_commands.json").read_text())
for entry in commands:
    source = (Path(entry["directory"]) / entry["file"]).resolve()
    try:
        relative = source.relative_to(root).as_posix()
    except ValueError:
        continue
    if relative not in wanted:
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
    if source.suffix == ".cpp":
        cleaned += ["-frtti"] # consistent shared_ptr COMDAT RTTI for UBSan vptr

    report["sources"].append(relative)
    report["commands"].append(cleaned)
    subprocess.run(cleaned, cwd=build, check=True)
    if relative.startswith("app/tests/"):
        replacements[entry["output"]] = str(obj)
    else:
        archive_replacements[entry["output"]] = str(obj)
if set(report["sources"]) != wanted:
    raise RuntimeError("Compile database lacks required source(s)")
link_text = subprocess.check_output(["ninja", "-t", "commands", "app/tests/painter-layer-modes"], cwd=build, text=True)
link = shlex.split(link_text.strip().splitlines()[-1])
exe = build / "app/tests/painter-layer-modes-asan"
link[link.index("-o") + 1] = str(exe)
link = [replacements.get(arg, arg) for arg in link]
# Replace instrumented members inside private thin archives. Leaving the old
# members available can pull in their RTTI COMDAT symbols and duplicate feature
# definitions when -frtti is used for the vptr sanitizer.
for archive in ["app/operations/layer-modes-legacy/libapplayermodeslegacy.a", "app/operations/layer-modes/libapplayermodes.a"]:
    members = subprocess.check_output(["ar", "t", archive], cwd=build, text=True).splitlines()
    rewritten = []
    for member in members:
        member_path = str((build / member).resolve())
        rewritten.append(archive_replacements.get(member, member_path))
    sanitized_archive = output / Path(archive).name
    if sanitized_archive.exists():
        sanitized_archive.unlink()
    command = ["ar", "crsT", str(sanitized_archive)] + rewritten
    report["commands"].append(command)
    subprocess.run(command, cwd=build, check=True)
    link = [str(sanitized_archive) if arg == archive else arg for arg in link]
link[1:1] = flags
report["commands"].append(link)
subprocess.run(link, cwd=build, check=True)
env = dict(os.environ)
env.update({"GIMP_TESTING_ABS_TOP_SRCDIR": str(root),
            "GIMP_TESTING_ABS_TOP_BUILDDIR": str(build),
            "GIMP_TESTING_PLUGINDIRS": str(build / "plug-ins/common"),
            "UI_TEST": "yes", "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1:abort_on_error=1",
            "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})
result = subprocess.run([str(exe)], cwd=build, env=env, capture_output=True, text=True)
report["source_sha256"] = {p: hashlib.sha256((root/p).read_bytes()).hexdigest() for p in wanted}
report.update({"exit_code": result.returncode, "stdout": result.stdout, "stderr": result.stderr})
args.report.write_text(json.dumps(report, indent=2) + "\n")
print(result.stdout)
if result.returncode:
    print(result.stderr, file=sys.stderr)
sys.exit(result.returncode)
