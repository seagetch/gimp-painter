#!/usr/bin/env python3
"""Build an instrumented Painter display event regression executable.

Run after building app/tests/painter-navigation-events, with the build environment loaded.
This builder does not start GTK. Run the emitted executable on a real display.
Use ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1, plus the regular test environment.
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
output = build / "painter-navigation-event-sanitizers"
output.mkdir(exist_ok=True)
report = {"scope": "Navigation arithmetic, modifier configuration, rotation and event adapter plus full GUI test; underlying core/GTK/dependencies uninstrumented",
          "sanitizers": ["address", "undefined"], "leak_detection": False,
          "sources": [], "commands": [], "run_status": "not run by builder"}
flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-O1"]
wanted = {"app/display/gimppainternavigation.c", "app/display/gimpmodifiersmanager.c",
          "app/display/gimpdisplayshell-rotate.c", "app/display/gimpdisplayshell-tool-events.c",
          "app/tests/test-painter-navigation-events.c"}
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
    if relative == "app/display/gimppainternavigation.c" and "libappdisplay.a.p" not in entry["output"]:
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
link_text = subprocess.check_output(["ninja", "-t", "commands", "app/tests/painter-navigation-events"], cwd=build, text=True)
link = shlex.split(link_text.strip().splitlines()[-1])
exe = build / "app/tests/painter-navigation-events-asan"
link[link.index("-o") + 1] = str(exe)
link = [replacements.get(arg, arg) for arg in link]
# Replace instrumented members inside private thin archives. Leaving the old
# members available can pull in their RTTI COMDAT symbols and duplicate feature
# definitions when -frtti is used for the vptr sanitizer.
for archive in ["app/display/libappdisplay.a"]:
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
report["executable"] = str(exe)
report["source_sha256"] = {source: hashlib.sha256((root/source).read_bytes()).hexdigest() for source in wanted}
args.report.write_text(json.dumps(report, indent=2) + "\n")
print(exe)
